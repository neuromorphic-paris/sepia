#define CATCH_CONFIG_MAIN
#include "../source/sepia.hpp"
#include "../third_party/Catch2/single_include/catch.hpp"

TEST_CASE("Read DVS header type", "[sepia::read_type]") {
    REQUIRE(sepia::read_header("../../test/dvs.es").event_stream_type == sepia::event_stream_type::dvs);
}

TEST_CASE("Read ATIS header type", "[sepia::read_type]") {
    REQUIRE(sepia::read_header("../../test/atis.es").event_stream_type == sepia::event_stream_type::atis);
}

TEST_CASE("Read color header type", "[sepia::read_type]") {
    REQUIRE(sepia::read_header("../../test/color.es").event_stream_type == sepia::event_stream_type::color);
}

TEST_CASE("Count DVS events", "[sepia::dvs_event_stream_observable]") {
    std::size_t count = 0;
    sepia::join_dvs_event_stream_observable("../../test/dvs.es", [&](sepia::dvs_event) -> void { ++count; });
    if (count != 476203) {
        FAIL(
            "the event stream observable generated an unexpected number of events (expected 2418241, got "
            + std::to_string(count) + ")");
    }
}

TEST_CASE("Count ATIS events", "[sepia::atis_event_stream_observable]") {
    std::size_t count = 0;
    sepia::join_atis_event_stream_observable("../../test/atis.es", [&](sepia::atis_event) -> void { ++count; });
    if (count != 1428204) {
        FAIL(
            "the event stream observable generated an unexpected number of events (expected 2418241, got "
            + std::to_string(count) + ")");
    }
}

TEST_CASE("Count color events", "[sepia::color_event_stream_observable]") {
    std::size_t count = 0;
    sepia::join_color_event_stream_observable("../../test/color.es", [&](sepia::color_event) -> void { ++count; });
    if (count != 976510) {
        FAIL(
            "the event stream observable generated an unexpected number of events (expected 2839574, got "
            + std::to_string(count) + ")");
    }
}
