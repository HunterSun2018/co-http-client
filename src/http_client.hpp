#pragma once
#include <string_view>
#include <functional>
#include <algorithm>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/function.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>
using error_code = boost::system::error_code;

class http_client : public std::enable_shared_from_this<http_client>
{
private:
    /* data */
    tcp::resolver resolver_;
    beast::ssl_stream<beast::tcp_stream> stream_;
    beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::empty_body> req_;
    http::response<http::string_body> res_;
    using on_response_handler = std::function<void(const error_code &errorCode,
                                                   const std::string &response)>;
    on_response_handler _on_response;
    error_code _ec;

public:
    http_client(net::io_context &ioc, ssl::context &ctx);

    ~http_client();

    auto send_request(std::string_view host,
                      std::string_view port,
                      std::string_view target,
                      int version);

private:
    void fail(beast::error_code ec, char const *what);
};

http_client::http_client(net::io_context &ioc, ssl::context &ctx)
    : resolver_(net::make_strand(ioc)),
      stream_(net::make_strand(ioc), ctx)
{
}

http_client::~http_client()
{
}

void http_client::fail(beast::error_code ec, char const *what)
{
    _ec = ec;

    //std::cerr << what << ": " << ec.message() << "\n";
    _on_response(ec, "");
}

auto http_client::send_request(std::string_view host,
                               std::string_view port,
                               std::string_view target,
                               int version)
{
    // Set up an HTTP GET request message
    req_.version(version);
    req_.method(http::verb::get);
    req_.target(target.data());
    req_.set(http::field::host, host);
    req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    auto on_shutdown = [=, this](beast::error_code ec)
    {
        if (ec == net::error::eof)
        {
            ec = {};
        }

        if (ec)
            return fail(ec, "shutdown");

        // If we get here then the connection is closed gracefully
        _on_response(ec, res_.body());
    };

    auto on_read = [=, this](beast::error_code ec,
                             std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, __func__);

        // Write the message to standard out
        // std::cout << res_ << std::endl;

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Gracefully close the stream
        // stream_.async_shutdown(on_shutdown);

        _on_response(ec, res_.body());
    };

    auto on_write = [=, this](beast::error_code ec,
                              std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, __func__);

        // Receive the HTTP response
        http::async_read(stream_,
                         buffer_,
                         res_,
                         on_read);
    };

    auto on_handshake = [=, this](beast::error_code ec)
    {
        if (ec)
            return fail(ec, __func__);

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Send the HTTP request to the remote host
        http::async_write(stream_, req_, on_write);
    };

    auto on_connect = [=, this](beast::error_code ec,
                                tcp::resolver::results_type::endpoint_type)
    {
        if (ec)
            return fail(ec, __func__);

        // Perform the SSL handshake
        stream_.async_handshake(ssl::stream_base::client,
                                on_handshake);
    };

    auto on_resolve = [=, this](beast::error_code ec,
                                tcp::resolver::results_type results)
    {
        if (ec)
            return fail(ec, __func__);

        // Set a timeout on the operation
        beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream_).async_connect(
            results,
            on_connect);
    };

    auto request = [=, this](on_response_handler callback)
    {
        _on_response = callback;

        // Set SNI Hostname (many hosts need this to handshake successfully)
        if (!SSL_set_tlsext_host_name(stream_.native_handle(), host.data()))
        {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            return fail(ec, __func__);
        }

        // Look up the domain name
        resolver_.async_resolve(
            host,
            port,
            on_resolve);
    };

    struct awaitable
    {
        using handler = std::function<void(on_response_handler handler)>;
        handler _request;
        std::string _response;
        error_code _ec;

        awaitable(handler resolver) : _request(resolver) {}

        bool await_ready() { return false; }

        void await_suspend(std::coroutine_handle<> handle)
        {
            auto on_resolve = [=, this](const error_code &ec,
                                        const std::string &response)
            {
                _ec = ec;
                _response = response;

                handle.resume();
            };

            _request(on_resolve);
        }

        std::string await_resume()
        {
            if (_ec)
                throw std::runtime_error(_ec.message());

            return _response;
        }
    };

    return awaitable{request}; //shared_from_this()
}