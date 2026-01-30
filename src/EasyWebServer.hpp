#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <unordered_map>
#include <utility>

namespace d3156
{
    namespace asio  = boost::asio;
    namespace beast = boost::beast;
    namespace http  = beast::http;
    using tcp       = asio::ip::tcp;
    using address   = asio::ip::address;

    class EasyWebServer
    {
    public:
        using RequestHandler = std::function<std::pair<bool, std::string>(const http::request<http::string_body> &,
                                                                          const address &client_ip)>;
        ~EasyWebServer();

        EasyWebServer(asio::io_context &io, unsigned short port);

        /// Добавить обработчик запросов по заданному пути
        void addPath(std::string path, RequestHandler handler);

        void stop();

        void setContentType(std::string payload);

    private:
        void accept();

        void handle_connection(std::shared_ptr<tcp::socket> socket);

        void process_request(std::shared_ptr<tcp::socket> socket, const http::request<http::string_body> &req);

    private:
        std::string payload_type = "text/plain; charset=utf-8";
        unsigned short port_;
        std::unordered_map<std::string, RequestHandler> handlers_;
        asio::io_context &io_;
        tcp::acceptor acceptor_;
        std::atomic<bool> is_running_ = true;
    };
}