#include "AsyncHttpClient.hpp"
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/version.hpp>
#include <PluginCore/Logger/Log>
#include <boost/url/parse.hpp>
#include <chrono>
#include <exception>
#include <src/Logger/Log.hpp>

#undef LOG_NAME
#define LOG_NAME ("AsyncHttpClient_" + host_clean_).c_str()
namespace http = beast::http;
namespace d3156
{

    AsyncHttpClient::AsyncHttpClient(net::io_context &ioc, const std::string &host, const std::string &cookie,
                                     const std::string &authorization, size_t max_async_send)
        : authorization_(authorization), cookie_(cookie), ioc_(ioc), ssl_ctx_(ssl::context::tlsv13_client),
          max_queued_senders(max_async_send)
    {
        ssl_ctx_.set_default_verify_paths();
        host_clean_ = host;
        service_    = "443";
        if (host_clean_.find("https://") == 0)
            host_clean_ = host_clean_.substr(8);
        else if (host_clean_.find("http://") == 0) {
            host_clean_ = host_clean_.substr(7);
            service_    = "80";
            use_ssl_    = false;
        }
        auto pos = host_clean_.find(":");
        if (pos != std::string::npos) {
            service_    = host_clean_.substr(pos + 1);
            host_clean_ = host_clean_.substr(0, pos);
        }
    }

    net::awaitable<bool> AsyncHttpClient::reconnectAsync()
    {
        if (!running_) co_return false;
        if (stream_) {
            LOG(1, "Closing existing SSL stream");
            stream_.reset();
        }
        if (tcp_stream_) {
            LOG(1, "Closing existing TCP stream");
            tcp_stream_.reset();
        }
        try {
            tcp::resolver resolver(ioc_);
            auto results = co_await resolver.async_resolve({host_clean_, service_}, net::use_awaitable);
            if (use_ssl_) {
                stream_ = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(ioc_, ssl_ctx_);
                stream_->set_verify_mode(ssl::verify_peer);
                co_await beast::get_lowest_layer(*stream_).async_connect(results, net::use_awaitable);
                beast::get_lowest_layer(*stream_).expires_after(std::chrono::seconds(30));
                if (!SSL_set_tlsext_host_name(stream_->native_handle(), host_clean_.c_str())) {
                    beast::system_error er{
                        beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category())};
                    R_LOG(1, "SSL_set_tlsext_host_name error: " << er.what());
                    is_http_connected = false;
                    co_return false;
                }
                co_await stream_->async_handshake(ssl::stream_base::client, net::use_awaitable);
                G_LOG(1, "Connected with new SSL session to " << host_clean_ << ":" << service_);
                is_http_connected = true;
            } else {
                tcp_stream_ = std::make_unique<beast::tcp_stream>(ioc_);
                co_await tcp_stream_->async_connect(results, net::use_awaitable);
                tcp_stream_->expires_after(std::chrono::seconds(30));
                G_LOG(1, "Connected HTTP to " << host_clean_ << ":" << service_);
                is_http_connected = true;
            }
        } catch (std::exception &e) {
            is_http_connected = false;
            R_LOG(1, "Reconnect error: " << e.what() << " to " << host_clean_ << ":" << service_);
            co_return false;
        }
        co_return true;
    }

    net::awaitable<resp_dynamic_body> AsyncHttpClient::sendAsync(req_string_body req, size_t retry,
                                                                 std::chrono::milliseconds timeout)
    {
        if (queued_senders >= max_queued_senders) co_return resp_dynamic_body{http::status::bad_request, 11};
        ++queued_senders;
        size_t spins = 0, spin_max = timeout / std::chrono::milliseconds(10);
        while (busy_.exchange(true)) {
            if (++spins > spin_max) co_return resp_dynamic_body{http::status::bad_request, 11};
            co_await boost::asio::steady_timer(ioc_, std::chrono::milliseconds(10)).async_wait(net::use_awaitable);
        }
        --queued_senders;

        req.set(http::field::host, host_clean_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, payload_type_);
        if (!authorization_.empty()) { req.set(http::field::authorization, authorization_); }
        if (!cookie_.empty()) { req.set(http::field::cookie, cookie_); }
        req.prepare_payload();
        LOG(5, "Send async request: " << req);
        if (!co_await ensureConnected()) {
            R_LOG(1, "Failed to ensure connection for async request");
            co_return resp_dynamic_body{http::status::bad_request, 11};
        }
        try {
            beast::flat_buffer buffer;
            resp_dynamic_body res;
            if (use_ssl_) {
                stream_->next_layer().expires_after(timeout);
                co_await http::async_write(*stream_, req, net::use_awaitable);
                co_await http::async_read(*stream_, buffer, res, net::use_awaitable);
            } else {
                tcp_stream_->expires_after(timeout);
                co_await http::async_write(*tcp_stream_, req, net::use_awaitable);
                co_await http::async_read(*tcp_stream_, buffer, res, net::use_awaitable);
            }
            LOG(5, "Async response: " << res.result_int());
            busy_ = false;
            co_return res;
        } catch (const std::exception &e) {
            R_LOG(1, "Async request failed " << req.target() << ": " << e.what());
            busy_             = false;
            is_http_connected = false;
            if (retry == 0) co_return resp_dynamic_body{http::status::bad_request, 11};
        }
        co_return co_await sendAsync(req, --retry, timeout);
    }

    net::awaitable<resp_dynamic_body> AsyncHttpClient::postAsync(std::string path, std::string body, size_t retry,
                                                                 std::chrono::milliseconds timeout)
    {
        req_string_body r{http::verb::post, basePath_ + path, 11};
        r.body() = std::move(body);
        co_return co_await sendAsync(std::move(r), retry, timeout);
    }

    net::awaitable<resp_dynamic_body> AsyncHttpClient::getAsync(std::string path, std::string body, size_t retry,
                                                                std::chrono::milliseconds timeout)
    {
        req_string_body r{http::verb::get, basePath_ + path, 11};
        r.body() = std::move(body);
        co_return co_await sendAsync(std::move(r), retry, timeout);
    }

    net::awaitable<resp_dynamic_body> AsyncHttpClient::patchAsync(std::string path, std::string body, size_t retry,
                                                                  std::chrono::milliseconds timeout)
    {
        req_string_body r{http::verb::patch, basePath_ + path, 11};
        r.body() = std::move(body);
        co_return co_await sendAsync(std::move(r), retry, timeout);
    }

    net::awaitable<bool> AsyncHttpClient::ensureConnected()
    {
        if (!inited) {
            inited = true;
            co_return co_await reconnectAsync();
        }
        if (isConnected()) co_return true;
        R_LOG(0, "Stream was closed");
        co_return co_await reconnectAsync();
    }

    net::awaitable<void> AsyncHttpClient::disconnect()
    {
        running_ = false;
        if (stream_) {
            try {
                co_await stream_->async_shutdown(net::use_awaitable);
                G_LOG(1, "SSL shutdown completed");
            } catch (const beast::system_error &e) {
                if (e.code() == net::error::eof || e.code() == ssl::error::stream_truncated)
                    G_LOG(1, "SSL shutdown finished normally: " << e.what());
                else
                    R_LOG(1, "SSL shutdown error: " << e.what());

            } catch (const std::exception &e) {
                R_LOG(1, "Shutdown exception: " << e.what());
            }
            stream_.reset();
        }
        if (tcp_stream_) tcp_stream_.reset();
        is_http_connected = false;
        co_return;
    }

    void AsyncHttpClient::setBasePath(std::string basePath) { basePath_ = std::move(basePath); }

    void AsyncHttpClient::setContentType(std::string payload) { payload_type_ = std::move(payload); }

    bool AsyncHttpClient::isConnected() { return is_http_connected; }

    AsyncHttpClient::~AsyncHttpClient()
    {
        running_ = false;
        G_LOG(1, "AsyncHttpClient destroyed");
    }

    namespace http = beast::http;

    static bool is_redirect(unsigned c) { return c == 301 || c == 302 || c == 307 || c == 308 || c == 303; }

    static std::string make_absolute_url(boost::urls::url_view base, std::string_view loc)
    {
        if (!loc.empty() && loc.front() == '/')
            return std::string(base.scheme()) + "://" + std::string(base.host()) +
                   (base.has_port() ? ":" + std::string(base.port()) : "") + std::string(loc);
        return std::string(loc);
    }

#define LOG_NAME ("AsyncHttpClient::wget(" + url + ")").c_str()
    net::awaitable<bool> AsyncHttpClient::wget(net::io_context &ioc, std::string url, std::string target_path,
                                               std::string authorization, std::string cookie, int max_redirects,
                                               std::chrono::seconds timeout)
    {

        std::unique_ptr<AsyncHttpClient> cli;
        for (int hop = 0; hop <= max_redirects; ++hop) {
            auto r = boost::urls::parse_uri(url);
            if (!r) co_return false;
            boost::urls::url_view u = r.value();
            if (u.scheme() != "https" && u.scheme() != "http") {
                R_LOG(1, "Unsupported scheme:" << u.scheme());
                co_return false;
            }
            if (u.host().empty()) {
                R_LOG(1, "Host was empty:" << u.scheme());
                co_return false;
            }
            std::string origin = std::string(u.encoded_origin());
            std::string target = std::string(u.encoded_target());
            if (target.empty()) target = std::string("/");
            if (!cli) {
                cli = std::make_unique<AsyncHttpClient>(ioc, origin, cookie, authorization);
            } else {
                if (cli->host_clean_ != u.host() || (cli->use_ssl_ && u.scheme() != "https") ||
                    (!cli->use_ssl_ && u.scheme() != "http") ||
                    cli->service_ != (u.has_port() ? u.port() : (u.scheme() == "https" ? "443" : "80"))) {
                    co_await cli->disconnect();
                    if (cli->host_clean_ != u.host()) authorization = "";
                    cli = std::make_unique<AsyncHttpClient>(ioc, origin, cookie, authorization);
                }
            }
            if (!co_await cli->ensureConnected()) co_return false;
            http::request<http::empty_body> req{http::verb::get, target, 11};
            req.set(http::field::host, cli->host_clean_);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            if (!authorization.empty()) req.set(http::field::authorization, authorization);
            if (!cookie.empty()) req.set(http::field::cookie, cookie);
            beast::flat_buffer buffer;
            http::response_parser<http::file_body> parser;
            try {
                if (cli->use_ssl_) {
                    cli->stream_->next_layer().expires_after(timeout);
                    co_await http::async_write(*cli->stream_, req, net::use_awaitable);
                    co_await http::async_read_header(*cli->stream_, buffer, parser, net::use_awaitable);
                } else {
                    cli->tcp_stream_->expires_after(timeout);
                    co_await http::async_write(*cli->tcp_stream_, req, net::use_awaitable);
                    co_await http::async_read_header(*cli->tcp_stream_, buffer, parser, net::use_awaitable);
                }
            } catch (std::exception &e) {
                R_LOG(1, "Error on wget file " << e.what());
                co_return false;
            }
            unsigned code = parser.get().result_int();
            if (is_redirect(code)) {
                auto it = parser.get().base().find(http::field::location);
                if (it == parser.get().base().end()) co_return false;
                std::string loc = std::string(it->value());
                url             = make_absolute_url(u, loc);
                continue;
            }
            if (code != 200) {
                R_LOG(1, "Recv not 200 code " << parser.get().base());
                co_return false;
            }
            beast::error_code ec;
            parser.get().body().open(target_path.c_str(), beast::file_mode::write, ec);
            if (ec) co_return false;
            try {
                if (cli->use_ssl_)
                    co_await http::async_read(*cli->stream_, buffer, parser, net::use_awaitable);
                else
                    co_await http::async_read(*cli->tcp_stream_, buffer, parser, net::use_awaitable);
            } catch (std::exception &e) {
                R_LOG(1, "Error on wget file " << e.what());
                co_return false;
            }
            co_return true;
        }
        co_return false;
    }

} // namespace d3156
