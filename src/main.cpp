#include <coroutine>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "co_helper.hpp"
#include "http_client.hpp"

using namespace std;

auto switch_to_new_thread(std::jthread &out)
{
    struct awaitable
    {
        std::jthread *p_out;
        bool await_ready() { return false; }
        void await_suspend(std::coroutine_handle<> h)
        {
            std::jthread &out = *p_out;
            if (out.joinable())
                throw std::runtime_error("Output jthread parameter not empty");
            out = std::jthread([h]
                               { h.resume(); });
            // Potential undefined behavior: accessing potentially destroyed *this
            // std::cout << "New thread ID: " << p_out->get_id() << '\n';
            std::cout << "New thread ID: " << out.get_id() << '\n'; // this is OK
        }
        void await_resume() {}
    };
    return awaitable{&out};
}

struct task
{
    struct promise_type
    {
        task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

task resuming_on_new_thread(std::jthread &out)
{
    std::cout << "Coroutine started on thread: " << std::this_thread::get_id() << '\n';
    co_await switch_to_new_thread(out);
    // awaiter destroyed here
    std::cout << "Coroutine resumed on thread: " << std::this_thread::get_id() << '\n';
}

co_helper::Task<void> test(net::io_context &ioc)
{
    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv12_client};

    // This holds the root certificate used for verification
    // load_root_certificates(ctx);

    // Verify the remote server's certificate
    ctx.set_verify_mode(ssl::verify_none);

    // Launch the asynchronous operation
    // std::make_shared<session>(ioc, ctx)->run(host, port, target, version);
    // std::cout << "Coroutine started on thread: " << std::this_thread::get_id() << '\n';

    try
    {
        string response = co_await std::make_shared<http_client>(ioc, ctx)->send_request("cn.bing.com", "443", "/", 11);
        cout << response << endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << __func__ << " : " << e.what() << '\n';
    }

    // std::cout << "Coroutine resumed on thread: " << std::this_thread::get_id() << '\n';
}

template <std::integral T>
co_helper::Generator<T> range(T first, const T last)
{
    while (first < last)
    {
        co_yield first++;
    }
}

int main()
{
    // std::jthread out;
    // resuming_on_new_thread(out);

    // The io_context is required for all I/O
    net::io_context ioc;

    std::cout << "The main start at thread : " << std::this_thread::get_id() << '\n';

    try
    {
        test(ioc);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    std::cout << "The main ended at thread: " << std::this_thread::get_id() << '\n';

    ioc.run();
    // getchar();
}