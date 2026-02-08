#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/awaitable.hpp>

namespace beast = boost::beast;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;

namespace d3156 {

using tcp = boost::asio::ip::tcp;
using resp_dynamic_body = beast::http::response<beast::http::dynamic_body>;
using req_string_body   = beast::http::request<beast::http::string_body>;

class AsyncHttpClient {
public:
    AsyncHttpClient(net::io_context& ioc, const std::string& host, 
                   const std::string& cookie = "", 
                   const std::string& authorization = "");

    // Только асинхронные методы
    net::awaitable<resp_dynamic_body> sendAsync(req_string_body req,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{0});

    net::awaitable<resp_dynamic_body> postAsync(std::string path, std::string body,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{500});

    net::awaitable<resp_dynamic_body> getAsync(std::string path, std::string body,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{500});

    void setBasePath(std::string basePath);
    void setContentType(std::string payload);
    
    net::awaitable<bool> ensureConnected();
    net::awaitable<void> disconnect();
    
    bool isConnected();

    ~AsyncHttpClient();

private:
    bool use_ssl_ = true;
    std::string payload_type_ = "text/plain; charset=utf-8";
    std::string authorization_;
    std::string cookie_;
    std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> stream_;
    std::unique_ptr<beast::tcp_stream> tcp_stream_;
    ssl::context ssl_ctx_;
    std::string host_clean_;
    std::string service_;
    net::io_context& ioc_;
    bool running_ = true;
    std::string basePath_;
    net::awaitable<bool> reconnectAsync();   
};

} // namespace d3156
