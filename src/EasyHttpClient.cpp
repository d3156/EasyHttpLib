#include "EasyHttpClient.hpp"

#include <boost/beast/http/impl/write.hpp>
#include <boost/beast/version.hpp>
#include <exception>
#include <iostream>
#include <string>
#define LOG(args) std::cout << "[EasyHttpClient " << host_clean_ << "] " << args << std::endl

namespace d3156
{

    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace net   = boost::asio;
    namespace ssl   = boost::asio::ssl;

    using tcp = boost::asio::ip::tcp;

    EasyHttpClient::EasyHttpClient(net::io_context &ioc, const std::string &host, const std::string &cookie,
                                   const std::string &token)
        : token_(token), cookie_(cookie), io_(ioc), ssl_ctx_(ssl::context::tlsv13_client)
    {
        ssl_ctx_.set_default_verify_paths();
        host_clean_ = host;
        if (host_clean_.find("https://") == 0)
            host_clean_ = host_clean_.substr(8);
        else if (host_clean_.find("http://") == 0)
            host_clean_ = host_clean_.substr(7);
        service = host.find("https") != std::string::npos ? "https" : "http";
        reconnect();
    }

    bool EasyHttpClient::reconnect()
    {
        if (!runing) return false;
        if (stream_) {
            LOG("Reconnecting SSL session");
            beast::error_code ec;
            stream_->shutdown(ec); // Игнорируем ошибки
            stream_.reset();
            if (ec != net::error::eof && ec) { LOG("On close error: " << ec.to_string()); }
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
                LOG("SSL_set_tlsext_host_name error: " << er.what());
                return false;
            }
            stream_->handshake(ssl::stream_base::client);
            LOG("Сonnected with new SSL session");
        } catch (std::exception &e) {
            LOG("Error: " << e.what() << " with service " << service << " to host " << host_clean_);
            return false;
        }

        return true;
    }

    EasyHttpClient::~EasyHttpClient()
    {
        runing = false;
        beast::error_code ec;
        stream_->shutdown(ec);
        if (ec != net::error::eof)
            LOG("SSL_set_tlsext_host_name error: " << ec.to_string());
        else
            LOG("Closed SSL session");
    }

    http::response<http::dynamic_body> EasyHttpClient::send(http::request<http::string_body> req,
                                                            std::chrono::milliseconds timeout)
    {
        try {
            // Construct request
            req.set(http::field::host, host_clean_);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set(http::field::content_type, payload_type);
            if (token_.size()) req.set(http::field::authorization, "Bearer " + token_);
            if (cookie_.size()) req.set(http::field::cookie, cookie_);
            req.prepare_payload();
            beast::tcp_stream &tcp_layer = stream_->next_layer();
            tcp_layer.expires_after(timeout);
            http::write(*stream_, req);

            // Receive the response
            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            http::read(*stream_, buffer, res);
            return res;
        } catch (const std::exception &e) {
            LOG("Request to " << req.target() << " failed.");
            LOG("Request error: " << e.what());
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
        if (token_.size()) req.set(http::field::authorization, "Bearer " + token_);
        if (cookie_.size()) req.set(http::field::cookie, cookie_);
        try {
            // Construct request
            req.prepare_payload();
            beast::tcp_stream &tcp_layer = stream_->next_layer();
            tcp_layer.expires_after(timeout);
            http::write(*stream_, req);

            // Receive the response
            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            http::read(*stream_, buffer, res);
            return res;
        } catch (const std::exception &e) {
            LOG("Request to " << req.target() << " failed.");
            LOG("Request error: " << e.what());
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