#include "catch.hpp"

#include "../source/sepia.hpp"

#include <mutex>

TEST_CASE("Event counter", "[sepia]") {
    std::size_t count = 0;
    std::exception_ptr sharedException;
    std::mutex lock;
    lock.lock();
    try {
        auto camera = sepia::make_eventStreamObservable(
            "../../test/sepiaTest.es",
            [&](sepia::Event) -> void {
                ++count;
            },
            [&](std::exception_ptr exception) {
                sharedException = exception;
                lock.unlock();
            }
        );
        lock.lock();
        lock.unlock();
        if (sharedException) {
            std::rethrow_exception(sharedException);
        }
    } catch (const sepia::EndOfFile&) {
        if (count != 2649650) {
            FAIL("the event stream observable generated an unexpected number of events (expected 200, got " + std::to_string(count) + ")");
        }
    } catch (const std::runtime_error& exception) {
        FAIL(exception.what());
    }
}
