#pragma once
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/awaitable.hpp>

namespace beast = boost::beast;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;

namespace d3156
{

    using tcp               = boost::asio::ip::tcp;
    using resp_dynamic_body = beast::http::response<beast::http::dynamic_body>;
    using req_string_body   = beast::http::request<beast::http::string_body>;

    class AsyncHttpClient
    {
    public:
        AsyncHttpClient(net::io_context &ioc, const std::string &host, const std::string &cookie = "",
                        const std::string &authorization = "", size_t max_queued_senders = 10);

        // Только асинхронные методы
        net::awaitable<resp_dynamic_body> sendAsync(req_string_body req, size_t retry = 1,
                                                    std::chrono::milliseconds timeout = std::chrono::milliseconds{0});

        net::awaitable<resp_dynamic_body> postAsync(std::string path, std::string body, size_t retry = 1,
                                                    std::chrono::milliseconds timeout = std::chrono::milliseconds{500});

        net::awaitable<resp_dynamic_body> getAsync(std::string path, std::string body, size_t retry = 1,
                                                   std::chrono::milliseconds timeout = std::chrono::milliseconds{500});

        net::awaitable<resp_dynamic_body> patchAsync(std::string path, std::string body, size_t retry = 1,
                                                     std::chrono::milliseconds timeout = std::chrono::milliseconds{
                                                         500});

        void setBasePath(std::string basePath);
        void setContentType(std::string payload);

        net::awaitable<bool> ensureConnected();
        net::awaitable<void> disconnect();

        bool isConnected();

        static net::awaitable<bool> wget(net::io_context &ioc, std::string url, std::string target_path,
                                         std::string authorization = "", std::string cookie = "", int max_redirects = 5,
                                         std::chrono::seconds timeout = std::chrono::seconds{30});

        ~AsyncHttpClient();

    private:
        std::atomic<bool> is_http_connected;

        size_t max_queued_senders          = 10;
        std::atomic<size_t> queued_senders = 0;
        std::atomic<bool> busy_            = false;
        bool use_ssl_                      = true;
        std::string payload_type_          = "text/plain; charset=utf-8";
        std::string authorization_         = "";
        std::string cookie_                = "";
        std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> stream_;
        std::unique_ptr<beast::tcp_stream> tcp_stream_;
        ssl::context ssl_ctx_;
        std::string host_clean_ = "";
        std::string service_    = "";
        net::io_context &ioc_;
        bool running_         = true;
        bool inited           = false;
        std::string basePath_ = "";
        net::awaitable<bool> reconnectAsync();
    };

} // namespace d3156
