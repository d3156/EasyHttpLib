#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/ssl.hpp>

namespace beast = boost::beast;
namespace asio  = boost::asio;

using resp_dynamic_body = beast::http::response<beast::http::dynamic_body>;
using req_string_body   = beast::http::request<beast::http::string_body>;

namespace d3156
{
    class EasyHttpClient
    {

    public:
        EasyHttpClient(asio::io_context &ioc, const std::string &host, const std::string &cookie = "",
                       const std::string &token = "");

        resp_dynamic_body send(std::string target, std::string body, beast::http::verb type = beast::http::verb::post,
                               std::chrono::milliseconds timeout = std::chrono::milliseconds{500});

        resp_dynamic_body send(beast::http::request<beast::http::string_body> req,
                               std::chrono::milliseconds timeout = std::chrono::milliseconds{0});

        resp_dynamic_body post(std::string path, std::string body, std::chrono::milliseconds timeout = std::chrono::milliseconds{500});
        
        resp_dynamic_body get(std::string path, std::string body, std::chrono::milliseconds timeout = std::chrono::milliseconds{500});

        void setBasePath(std::string basePath);

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
        std::string basePath_;
    };
}