#include "EasyWebServer.hpp"
#include <PluginCore/Logger/Log.hpp>

#undef LOG_NAME
#define LOG_NAME ("EasyWebServer " + std::to_string(port_)).c_str()

namespace d3156
{
    EasyWebServer::EasyWebServer(asio::io_context &io, unsigned short port)
        : io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)), port_(port)
    {
        accept();
    }

    void EasyWebServer::accept()
    {
        if (!is_running_) return;
        auto socket = std::make_shared<tcp::socket>(io_);
        acceptor_.async_accept(*socket, [this, socket](beast::error_code ec) {
            if (!ec) handle_connection(socket);
            accept();
        });
    }

    void EasyWebServer::handle_connection(std::shared_ptr<tcp::socket> socket)
    {
        auto buffer  = std::make_shared<beast::flat_buffer>();
        auto request = std::make_shared<http::request<http::string_body>>();
        http::async_read(*socket, *buffer, *request,
                         [this, socket, buffer, request](beast::error_code ec, std::size_t) {
                             if (!ec) { process_request(socket, *request); }
                         });
    }

    void EasyWebServer::process_request(std::shared_ptr<tcp::socket> socket,
                                        const http::request<http::string_body> &req)
    {
        if (req.target().size() > 1000) return;
        auto handler = handlers_.find(req.target());
        if (handler == handlers_.end()) return;
        auto res      = handler->second(req, socket->remote_endpoint().address());
        auto response = std::make_shared<http::response<http::string_body>>(
            res.first ? http::status::ok : http::status::forbidden, req.version());
        if (!res.first) {
            R_LOG(1, "Bad Request " << req);
            R_LOG(1, " What:" << res.second);
        }
        response->set(http::field::content_type,  payload_type);
        response->body() = res.first ? res.second : "Bad Request";
        response->prepare_payload();
        response->keep_alive(false);
        http::async_write(*socket, *response, [this, socket, response](beast::error_code, std::size_t) {
            beast::error_code ec;
            ec = socket->shutdown(tcp::socket::shutdown_send, ec);
            if (ec && ec != boost::asio::error::not_connected) R_LOG(1, " Error on close Session" << ec);
            ec = socket->close(ec);
        });
    }

    void EasyWebServer::addPath(std::string path, RequestHandler handler) { handlers_[path] = handler; }

    void EasyWebServer::stop()
    {
        is_running_ = false;
        beast::error_code ec;
        ec = acceptor_.cancel(ec);
        ec = acceptor_.close(ec);
        handlers_.clear();
    }

    EasyWebServer::~EasyWebServer()
    {
        if (is_running_) stop();
    }

    void EasyWebServer::setContentType(std::string payload) { payload_type = payload; }
}