#include "EasyHttpClient.hpp"

#include <boost/beast/http/impl/write.hpp>
#include <boost/beast/version.hpp>
#include <exception>
#include <string>
#include <PluginCore/Logger/Log.hpp>

#undef LOG_NAME
#define LOG_NAME ("EasyHttpClient_" + host_clean_).c_str()

namespace d3156
{

    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace net   = boost::asio;
    namespace ssl   = boost::asio::ssl;

    using tcp = boost::asio::ip::tcp;

    EasyHttpClient::EasyHttpClient(net::io_context &ioc, const std::string &host, const std::string &cookie,
                                   const std::string &authorization)
        : authorization_(authorization), cookie_(cookie), io_(ioc), ssl_ctx_(ssl::context::tlsv13_client)
    {
        ssl_ctx_.set_default_verify_paths();
        host_clean_ = host;
        service     = "443";
        if (host_clean_.find("https://") == 0)
            host_clean_ = host_clean_.substr(8);
        else if (host_clean_.find("http://") == 0) {
            host_clean_ = host_clean_.substr(7);
            service     = "80";
            use_ssl_    = false;
        }
        auto pos = host_clean_.find(":");
        if (pos != std::string::npos) {
            service     = host_clean_.substr(pos + 1);
            host_clean_ = host_clean_.substr(0, pos);
        }
        reconnect();
    }

    bool EasyHttpClient::reconnect()
    {
        if (!runing) return false;
        if (use_ssl_) {
            if (stream_) {
                LOG(1, "Reconnecting SSL session");
                beast::error_code ec;
                stream_.reset();
                if (ec != net::error::eof && ec) { R_LOG(1, "On close error: " << ec.to_string()); }
            }
            try {
                stream_ = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(io_, ssl_ctx_);
                stream_->set_verify_mode(ssl::verify_peer);
                tcp::resolver resolver(io_);
                get_lowest_layer(*stream_).connect(resolver.resolve({host_clean_, service}));
                get_lowest_layer(*stream_).expires_after(std::chrono::seconds(30));
                if (!SSL_set_tlsext_host_name(stream_->native_handle(), host_clean_.c_str())) {
                    beast::system_error er{
                        beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category())};
                    R_LOG(1, "SSL_set_tlsext_host_name error: " << er.what());
                    return false;
                }
                stream_->handshake(ssl::stream_base::client);
                G_LOG(1, "Сonnected with new SSL session");
            } catch (std::exception &e) {
                R_LOG(1, "Error: " << e.what() << " with service " << service << " to host " << host_clean_);
                return false;
            }

            return true;
        }
        try {
            tcp_stream_.reset();
            tcp_stream_ = std::make_unique<beast::tcp_stream>(io_);
            tcp::resolver resolver(io_);
            tcp_stream_->connect(resolver.resolve(host_clean_, service));
            tcp_stream_->expires_after(std::chrono::seconds(30));
            G_LOG(1, "Connected HTTP");
        } catch (std::exception &e) {
            R_LOG(1, "Error: " << e.what() << " with service " << service << " to host " << host_clean_);
            return false;
        }
        return true;
    }

    EasyHttpClient::~EasyHttpClient()
    {
        runing = false;
        beast::error_code ec;
        if (stream_) stream_->shutdown(ec);
        if (ec != net::error::eof)
            R_LOG(1, "Close socket error: " << ec.to_string());
        else
            G_LOG(1, "Closed session");
    }

    http::response<http::dynamic_body> EasyHttpClient::send(http::request<http::string_body> req,
                                                            std::chrono::milliseconds timeout)
    {
        try {
            // Construct request
            req.set(http::field::host, host_clean_);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set(http::field::content_type, payload_type);
            if (authorization_.size()) req.set(http::field::authorization, authorization_);
            if (cookie_.size()) req.set(http::field::cookie, cookie_);
            req.prepare_payload();
            LOG(5, "Send request: " << req);
            if (use_ssl_) {
                stream_->next_layer().expires_after(timeout);
                http::write(*stream_, req);
            } else {
                tcp_stream_->expires_after(timeout);
                http::write(*tcp_stream_, req);
            }

            // Receive the response
            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            if (use_ssl_)
                http::read(*stream_, buffer, res);
            else
                http::read(*tcp_stream_, buffer, res);
            LOG(5, "Answer: " << res);
            return res;
        } catch (const std::exception &e) {
            R_LOG(1, "Request to " << req.target() << " failed.");
            R_LOG(1, "Request error: " << e.what());
            reconnect();
        }
        return {};
    }

    http::response<http::dynamic_body> EasyHttpClient::send(std::string target, std::string body, http::verb type,
                                                            std::chrono::milliseconds timeout)
    {
        boost::beast::http::request<boost::beast::http::string_body> req{type, basePath_ + target, 11};
        req.body() = body;
        req.set(http::field::host, host_clean_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, payload_type);
        if (authorization_.size()) req.set(http::field::authorization, authorization_);
        if (cookie_.size()) req.set(http::field::cookie, cookie_);
        try {
            // Construct request
            req.prepare_payload();
            LOG(5, "Send request: " << req);
            if (use_ssl_) {
                stream_->next_layer().expires_after(timeout);
                http::write(*stream_, req);
            } else {
                tcp_stream_->expires_after(timeout);
                http::write(*tcp_stream_, req);
            }

            // Receive the response
            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            if (use_ssl_)
                http::read(*stream_, buffer, res);
            else
                http::read(*tcp_stream_, buffer, res);
            LOG(5, "Answer: " << res);
            return res;
        } catch (const std::exception &e) {
            R_LOG(1, "Request to " << req.target() << " failed.");
            R_LOG(1, "Request error: " << e.what());
            reconnect();
        }
        return {};
    }

    resp_dynamic_body EasyHttpClient::post(std::string path, std::string body, std::chrono::milliseconds timeout)
    {
        req_string_body r{http::verb::post, basePath_ + path, 11};
        r.body() = body;
        return send(r, timeout);
    }

    resp_dynamic_body EasyHttpClient::get(std::string path, std::string body, std::chrono::milliseconds timeout)
    {
        req_string_body r{http::verb::get, basePath_ + path, 11};
        r.body() = body;
        return send(r, timeout);
    }

    void EasyHttpClient::setBasePath(std::string basePath) { basePath_ = basePath; }

    void EasyHttpClient::setContentType(std::string payload) { payload_type = payload; }
}