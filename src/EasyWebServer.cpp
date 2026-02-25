#include "EasyWebServer.hpp"
#include <PluginCore/Logger/Log>

#undef LOG_NAME
#define LOG_NAME ("EasyWebServer " + std::to_string(port_)).c_str()

namespace d3156
{
    EasyWebServer::EasyWebServer(asio::io_context &io, unsigned short port)
        : io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port)), port_(port)
    {
        G_LOG(1, "created on port " << port_);
        accept();
    }

    void EasyWebServer::accept()
    {
        if (!is_running_) {
            LOG(5, "accept(): server is not running, stop accepting");
            return;
        }
        auto socket = std::make_shared<tcp::socket>(io_);
        LOG(10, "accept(): waiting for new connection...");
        acceptor_.async_accept(*socket, [this, socket](beast::error_code ec) {
            if (ec)
                R_LOG(1, "accept(): error on accept: " << ec.message());
            else {
                G_LOG(5, "accept(): new connection from " << socket->remote_endpoint(ec).address().to_string());
                handle_connection(socket);
            }
            accept();
        });
    }

    void EasyWebServer::handle_connection(std::shared_ptr<tcp::socket> socket)
    {
        LOG(10, "handle_connection(): start reading request");
        auto buffer  = std::make_shared<beast::flat_buffer>();
        auto request = std::make_shared<http::request<http::string_body>>();
        http::async_read(*socket, *buffer, *request,
                         [this, socket, buffer, request](beast::error_code ec, std::size_t bytes) {
                             if (ec) {
                                 R_LOG(1, "handle_connection(): read error: " << ec.message() << " bytes=" << bytes);
                                 return;
                             }
                             LOG(5, "handle_connection(): request received from "
                                        << socket->remote_endpoint(ec).address().to_string()
                                        << " method=" << request->method_string() << " target=" << request->target());
                             boost::asio::co_spawn(io_, this->process_request(socket, request), boost::asio::detached);
                         });
    }

    boost::asio::awaitable<void> EasyWebServer::process_request(std::shared_ptr<tcp::socket> socket,
                                                                std::shared_ptr<http::request<http::string_body>> req)
    {
        LOG(10, "process_request(): start, method=" << req->method_string() << " target=" << req->target()
                                                    << " body_size=" << req->body().size());
        if (req->target().size() > 1000) {
            R_LOG(1, "process_request(): target too long, size=" << req->target().size());
            co_return;
        }
        auto handler      = handlers_.find(req->target());
        auto handlerAsync = asyncHandlers_.find(req->target());
        if (handler == handlers_.end() && handlerAsync == asyncHandlers_.end()) {
            R_LOG(1, "process_request(): no handler for target=" << req->target());
            co_return;
        }
        Answer res;
        try {
            if (handlerAsync != asyncHandlers_.end()) {
                LOG(10, "process_request(): calling async handler for " << req->target());
                res = co_await handlerAsync->second(*req, socket->remote_endpoint().address());
            } else {
                LOG(10, "process_request(): calling sync handler for " << req->target());
                res = handler->second(*req, socket->remote_endpoint().address());
            }
        } catch (const std::exception &e) {
            R_LOG(1, "process_request(): handler threw exception for target=" << req->target() << " what=" << e.what());
            co_return;
        }

        auto response = std::make_shared<http::response<http::string_body>>(
            res.first ? http::status::ok : http::status::forbidden, req->version());

        if (!res.first) {
            R_LOG(1, "Bad Request " << *req);
            R_LOG(1, "What: " << res.second);
        }
        response->set(http::field::content_type, payload_type);
        response->body() = res.second;
        response->prepare_payload();
        response->keep_alive(false);
        LOG(10, "process_request(): sending response status=" << response->result_int()
                                                              << " content_length=" << response->body().size());
        http::async_write(*socket, *response, [this, socket, response](beast::error_code ec, std::size_t bytes) {
            if (ec)
                R_LOG(1, "async_write(): error: " << ec.message() << " bytes=" << bytes);
            else
                LOG(10, "async_write(): response sent, bytes=" << bytes);
            ec = socket->shutdown(tcp::socket::shutdown_send, ec);
            if (ec && ec != boost::asio::error::not_connected) R_LOG(1, "Error on close Session " << ec.message());
            ec = socket->close(ec);
            if (ec)
                R_LOG(1, "Error on close socket: " << ec.message());
            else
                LOG(10, "Socket closed successfully");
        });
    }

    void EasyWebServer::addPath(const std::string& path, RequestHandlerAsync handler)
    {
        G_LOG(1, "Add async server path http://0.0.0.0:" << port_ << path);
        asyncHandlers_[path] = handler;
    }

    void EasyWebServer::addPath(const std::string& path, RequestHandler handler)
    {
        G_LOG(1, "Add server path http://0.0.0.0:" << port_ << path);
        handlers_[path] = handler;
    }

    void EasyWebServer::stop()
    {
        LOG(5, "Stopping on port " << port_);
        is_running_ = false;
        beast::error_code ec;
        ec = acceptor_.cancel(ec);
        if (ec) R_LOG(1, "stop(): acceptor cancel error: " << ec.message());
        ec = acceptor_.close(ec);
        if (ec) R_LOG(1, "stop(): acceptor close error: " << ec.message());
        handlers_.clear();
        LOG(5, "stopped on port " << port_);
    }

    EasyWebServer::~EasyWebServer()
    {
        LOG(5, "dtor, is_running_=" << is_running_ << " port=" << port_);
        if (is_running_) stop();
    }

    void EasyWebServer::setContentType(const std::string& payload)
    {
        LOG(10, "setContentType(" << payload << ")");
        payload_type = payload;
    }
}