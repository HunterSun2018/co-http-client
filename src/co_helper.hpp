#pragma once

namespace co_helper
{
    template <typename T>
    struct Task
    {
        struct promise_type;
        using co_handle = std::coroutine_handle<promise_type>;
        co_handle _handle;

        struct promise_type
        {
            T value;

            Task get_return_object() { return Task{co_handle::from_promise(*this)}; }

            std::suspend_never initial_suspend() { return {}; }

            std::suspend_never final_suspend() { return {}; }

            void unhandled_exception() { std::rethrow_exception(std::current_exception()); }

            std::suspend_never return_value(T v)
            {
                value = v;
                return {};
            }
        };

        T get() { return _handle.promise().value; }

        ~Task()
        {
            if (_handle)
                _handle.destroy();
        }
    };

    template <>
    struct Task<void>
    {
        struct promise_type
        {
            Task<void> get_return_object() { return {}; }
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
        };
    };
}
