#include "catch.hpp"

#include "../source/opalKellyAtisSepia.hpp"

#include <mutex>
#include <iostream>

TEST_CASE("Event counter", "[opalKellyAtisSepia]") {
    std::exception_ptr sharedException;
    std::mutex lock;
    lock.lock();

    try {
        auto camera = opalKellyAtisSepia::make_camera(
            []() {
                size_t count = 0;
                int64_t timestampThreshold = 0;
                return [count, timestampThreshold](sepia::Event event) mutable -> void {
                    if (event.timestamp  > timestampThreshold) {
                        std::cout << count << " events / second" << std::endl;
                        timestampThreshold = event.timestamp + 1e6;
                        count = 0;
                    }
                    ++count;
                };
            }(),
            [&sharedException, &lock](std::exception_ptr exception) {
                sharedException = exception;
                lock.unlock();
            }
        );

        lock.lock();
        lock.unlock();
        if (sharedException) {
            std::rethrow_exception(sharedException);
        }
    } catch (const std::runtime_error& exception) {
        std::cout << "\e[31m" << exception.what() << "\e[0m" << std::endl;
    }
}
