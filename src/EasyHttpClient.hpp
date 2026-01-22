#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/ssl.hpp>

namespace d3156
{
    namespace beast = boost::beast;
    namespace asio  = boost::asio;
    namespace http  = beast::http;
    class EasyHttpClient
    {

    public:
        EasyHttpClient(asio::io_context &ioc, const std::string &host, const std::string &cookie = "",
                       const std::string &token = "");

        http::response<http::dynamic_body> send(std::string target, std::string body,
                                                http::verb type                   = http::verb::post,
                                                std::chrono::milliseconds timeout = std::chrono::milliseconds{500});

        http::response<beast::http::dynamic_body> send(beast::http::request<beast::http::string_body> req,
                                                       std::chrono::milliseconds timeout = std::chrono::milliseconds{
                                                           0});

        ~EasyHttpClient();

    private:
        bool reconnect();
        std::string token_;
        std::string cookie_;
        std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> stream_;
        asio::ssl::context ssl_ctx_;
        std::string host_ = "";
        asio::io_context &io_;
        bool runing = true;
    };
}