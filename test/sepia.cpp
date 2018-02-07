#define CATCH_CONFIG_MAIN
#include "../source/sepia.hpp"
#include "../third_party/Catch2/single_include/catch.hpp"

TEST_CASE("Count DVS events", "[sepia::dvs_event_stream_observable]") {
    std::size_t count = 0;
    try {
        sepia::join_dvs_event_stream_observable(
            "../../test/dvs.es",
            [&](sepia::dvs_event_t) -> void { ++count; }
        );
    } catch (const std::runtime_error& exception) {
        FAIL(exception.what());
    }
    if (count != 2418241) {
        FAIL(
            "the event stream observable generated an unexpected number of events (expected 2418241, got "
            + std::to_string(count) + ")");
    }
}

TEST_CASE("Count ATIS events", "[sepia::atis_event_stream_observable]") {
    std::size_t count = 0;
    try {
        sepia::join_atis_event_stream_observable(
            "../../test/atis.es",
            [&](sepia::atis_event_t) -> void { ++count; }
        );
    } catch (const std::runtime_error& exception) {
        FAIL(exception.what());
    }
    if (count != 2649650) {
        FAIL(
            "the event stream observable generated an unexpected number of events (expected 2418241, got "
            + std::to_string(count) + ")");
    }
}
