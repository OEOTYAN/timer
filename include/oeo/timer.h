#pragma once

#include <chrono>
#include <optional>
#include <semaphore>

#include "oeo/cancellable_callback.h"
#include "oeo/concurrent_priority_queue.h"

namespace oeo {

struct default_timer_invoke {
    template <class F>
    constexpr void operator()(F&& f) const {
        f->try_call();
    }
};

template <class Fn = std::function<void()>, class Init = std::identity, class Invoke = default_timer_invoke>
class timer {
    using clock = std::chrono::steady_clock;

    struct work {
        clock::time_point                         time;
        std::shared_ptr<cancellable_callback<Fn>> callback;
    };
    struct work_cmp {
        constexpr bool operator()(work const& x, work const& y) { return x.time > y.time; }
    };

    std::thread                               thread;
    concurrent_priority_queue<work, work_cmp> works;
    std::binary_semaphore                     sleeper{0};
    std::atomic_bool                          working{true};
    Init                                      initer;
    Invoke                                    invoker;

public:
    timer()
        requires(std::is_default_constructible_v<Init> && std::is_default_constructible_v<Invoke>)
    : timer(Init{}, Invoke{}) {}

    timer(Init init)
        requires(std::is_default_constructible_v<Invoke>)
    : timer(std::move(init), Invoke{}) {}

    timer(Init init, Invoke invoke) : initer(std::move(init)), invoker(std::move(invoke)) {
        thread = std::thread{[this] {
            (void)initer(nullptr);
            while (working) {
                std::optional<clock::duration> front_time{};

                auto now = clock::now();
                {
                    std::shared_ptr<cancellable_callback<Fn>> callback;
                    while (works.try_pop_if([&](work& w) {
                        if (w.time <= now) {
                            callback = std::move(w.callback);
                            return true;
                        }
                        front_time = w.time - now;
                        return false;
                    })) {
                        invoker(callback);
                    }
                }
                if (!front_time) {
                    sleeper.acquire();
                } else {
                    (void)sleeper.try_acquire_for(*front_time);
                }
            }
        }};
    }

    ~timer() {
        working = false;
        sleeper.release();
        if (thread.joinable())
            thread.join();
    }

    std::shared_ptr<cancellable_callback<Fn>> set_time_out(Fn fn, clock::duration duration) {
        auto callback = std::make_shared<cancellable_callback<Fn>>(std::move(fn));
        works.emplace(clock::now() + duration, callback);
        sleeper.release();
        return callback;
    }
};

} // namespace oeo
