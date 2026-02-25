#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/beast.hpp>
#include <unordered_map>
#include <utility>

namespace d3156
{
    namespace asio    = boost::asio;
    namespace beast   = boost::beast;
    namespace http    = beast::http;
    using tcp         = asio::ip::tcp;
    using address     = asio::ip::address;
    using Answer      = std::pair<bool, std::string>;
    using AnswerAsync = boost::asio::awaitable<Answer>;
    using string_req  = http::request<http::string_body>;

    class EasyWebServer
    {
    public:
        using RequestHandler      = std::function<Answer(const string_req &, const address &client_ip)>;
        using RequestHandlerAsync = std::function<AnswerAsync(const string_req &, const address &client_ip)>;
        ~EasyWebServer();

        EasyWebServer(asio::io_context &io, unsigned short port);

        /// Добавить обработчик запросов по заданному пути
        void addPath(const std::string& path, RequestHandler handler);

        void addPath(const std::string& path, RequestHandlerAsync handler);

        void stop();

        void setContentType(const std::string& payload);

    private:
        void accept();

        void handle_connection(std::shared_ptr<tcp::socket> socket);

        boost::asio::awaitable<void> process_request(std::shared_ptr<tcp::socket> socket,
                                                     std::shared_ptr<http::request<http::string_body>> req);

    private:
        std::string payload_type = "text/plain; charset=utf-8";
        unsigned short port_;
        std::unordered_map<std::string, RequestHandler> handlers_;
        std::unordered_map<std::string, RequestHandlerAsync> asyncHandlers_;
        asio::io_context &io_;
        tcp::acceptor acceptor_;
        std::atomic<bool> is_running_ = true;
    };
}