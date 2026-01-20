#include "EasyHttpClient.hpp"

#include <boost/beast/version.hpp>
#include <iostream>

namespace d3156
{

#define LOG(args) std::cout << "[EasyHttpClient " << host_ << "] " << args << std::endl
    namespace beast = boost::beast;
    namespace http  = beast::http;
    namespace net   = boost::asio;
    namespace ssl   = boost::asio::ssl;

    using tcp = boost::asio::ip::tcp;

    EasyHttpClient::EasyHttpClient(net::io_context &ioc, const std::string &host, const std::string &cookie,
                                   const std::string &token)
        : token_(token), cookie_(cookie), host_(host), io_(ioc), ssl_ctx_(ssl::context::tlsv13_client)
    {
        reconnect();
    }

    bool EasyHttpClient::reconnect()
    {
        if (stream_) {
            LOG("Reconnecting SSL session");
            beast::error_code ec;
            stream_->shutdown(ec); // Игнорируем ошибки
            stream_.reset();
            if (ec != net::error::eof && ec) { LOG("On close error: " << ec.to_string()); }
        }
        // Создаем новое соединение
        stream_ = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(io_, ssl_ctx_);
        stream_->set_verify_mode(ssl::verify_none);
        // TCP подключение
        tcp::resolver resolver(io_);

        get_lowest_layer(*stream_).connect(resolver.resolve({host_, "https"}));
        get_lowest_layer(*stream_).expires_after(std::chrono::seconds(30));

        if (!SSL_set_tlsext_host_name(stream_->native_handle(), host_.c_str())) {
            beast::system_error er{
                beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category())};
            LOG("SSL_set_tlsext_host_name error: " << er.what());
            return false;
        }
        stream_->handshake(ssl::stream_base::client);
        LOG("Сonnected with new SSL session");
        return true;
    }

    EasyHttpClient::~EasyHttpClient()
    {
        beast::error_code ec;
        stream_->shutdown(ec);
        if (ec != net::error::eof)
            LOG("SSL_set_tlsext_host_name error: " << ec.to_string());
        else
            LOG("Closed SSL session");
    }

    http::response<http::dynamic_body> EasyHttpClient::send(http::request<http::string_body> req)
    {
        try {
            // Construct request
            req.set(http::field::host, host_);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set(http::field::content_type, "application/json");
            if (token_.size()) req.set(http::field::authorization, "Bearer " + token_);
            if (cookie_.size()) req.set(http::field::cookie, cookie_);
            req.prepare_payload();
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
}