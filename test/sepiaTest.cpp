#include "catch.hpp"

#include "../source/sepia.hpp"

#include <mutex>

TEST_CASE("DVS event counter", "[AtisEventStreamObservable]") {
    std::size_t count = 0;
    std::exception_ptr sharedException;
    std::mutex lock;
    lock.lock();
    try {
        auto camera = sepia::make_dvsEventStreamObservable(
            "../../test/sepiaDvsTest.es",
            [&](sepia::DvsEvent) -> void {
                ++count;
            },
            [&](std::exception_ptr exception) -> void {
                sharedException = exception;
                lock.unlock();
            },
            []() -> bool {
                return false;
            },
            sepia::EventStreamObservable::Dispatch::asFastAsPossible
        );
        lock.lock();
        lock.unlock();
        if (sharedException) {
            std::rethrow_exception(sharedException);
        }
    } catch (const sepia::EndOfFile&) {
        if (count != 2418241) {
            FAIL("the event stream observable generated an unexpected number of events (expected 2418241, got " + std::to_string(count) + ")");
        }
    } catch (const std::runtime_error& exception) {
        FAIL(exception.what());
    }
}


TEST_CASE("ATIS event counter", "[AtisEventStreamObservable]") {
    std::size_t count = 0;
    std::exception_ptr sharedException;
    std::mutex lock;
    lock.lock();
    try {
        auto camera = sepia::make_atisEventStreamObservable(
            "../../test/sepiaAtisTest.es",
            sepia::make_split(
                [&](sepia::DvsEvent) -> void {
                    ++count;
                },
                [&](sepia::ThresholdCrossing) -> void {
                    ++count;
                }
            ),
            [&](std::exception_ptr exception) -> void {
                sharedException = exception;
                lock.unlock();
            },
            []() -> bool {
                return false;
            },
            sepia::EventStreamObservable::Dispatch::asFastAsPossible
        );
        lock.lock();
        lock.unlock();
        if (sharedException) {
            std::rethrow_exception(sharedException);
        }
    } catch (const sepia::EndOfFile&) {
        if (count != 2649650) {
            FAIL("the event stream observable generated an unexpected number of events (expected 2649650, got " + std::to_string(count) + ")");
        }
    } catch (const std::runtime_error& exception) {
        FAIL(exception.what());
    }
}
