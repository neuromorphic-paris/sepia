#include "catch.hpp"

#include "../source/opalKellyAtisSepia.hpp"

#include <mutex>

TEST_CASE("Event counter", "[sepia]") {
    auto count = static_cast<std::size_t>(0);
    try {
        auto camera = sepia::make_eventStreamObservable(
            "../../test/sepiaTest.es",
            [&](sepia::Event) -> void {
                ++count;
            },
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
        FAIL(exception.what());
    }
}
