#define CATCH_CONFIG_MAIN
#include "../source/sepia.hpp"
#include "../third_party/Catch2/single_include/catch.hpp"
#include <sstream>

TEST_CASE("Read DVS header type", "[sepia::read_type]") {
    REQUIRE(
        sepia::read_header(sepia::filename_to_ifstream(sepia::join({sepia::dirname(__FILE__), "dvs.es"})))
            .event_stream_type
        == sepia::event_stream_type::dvs);
}

TEST_CASE("Read ATIS header type", "[sepia::read_type]") {
    REQUIRE(
        sepia::read_header(sepia::filename_to_ifstream(sepia::join({sepia::dirname(__FILE__), "atis.es"})))
            .event_stream_type
        == sepia::event_stream_type::atis);
}

TEST_CASE("Read color header type", "[sepia::read_type]") {
    REQUIRE(
        sepia::read_header(sepia::filename_to_ifstream(sepia::join({sepia::dirname(__FILE__), "color.es"})))
            .event_stream_type
        == sepia::event_stream_type::color);
}

TEST_CASE("Count DVS events", "[sepia::dvs_event_stream_observable]") {
    std::size_t count = 0;
    sepia::join_dvs_event_stream_observable(
        sepia::filename_to_ifstream(sepia::join({sepia::dirname(__FILE__), "dvs.es"})),
        [&](sepia::dvs_event) -> void { ++count; });
    if (count != 476203) {
        FAIL(
            "the event stream observable generated an unexpected number of events (expected 2418241, got "
            + std::to_string(count) + ")");
    }
}

TEST_CASE("Count ATIS events", "[sepia::atis_event_stream_observable]") {
    std::size_t count = 0;
    sepia::join_atis_event_stream_observable(
        sepia::filename_to_ifstream(sepia::join({sepia::dirname(__FILE__), "atis.es"})),
        [&](sepia::atis_event) -> void { ++count; });
    if (count != 1428204) {
        FAIL(
            "the event stream observable generated an unexpected number of events (expected 2418241, got "
            + std::to_string(count) + ")");
    }
}

TEST_CASE("Count color events", "[sepia::color_event_stream_observable]") {
    std::size_t count = 0;
    sepia::join_color_event_stream_observable(
        sepia::filename_to_ifstream(sepia::join({sepia::dirname(__FILE__), "color.es"})),
        [&](sepia::color_event) -> void { ++count; });
    if (count != 976510) {
        FAIL(
            "the event stream observable generated an unexpected number of events (expected 2839574, got "
            + std::to_string(count) + ")");
    }
}

TEST_CASE("Parse JSON parameters", "[sepia::parameter]") {
    auto parameter = sepia::make_unique<sepia::object_parameter>(
        "key 0",
        sepia::array_parameter::make_empty(sepia::make_unique<sepia::char_parameter>(0)),
        "key 1",
        sepia::make_unique<sepia::object_parameter>(
            "subkey 0",
            sepia::make_unique<sepia::enum_parameter>("r", std::unordered_set<std::string>{"r", "g", "b"}),
            "subkey 1",
            sepia::make_unique<sepia::number_parameter>(0, 0, 1, false),
            "subkey 2",
            sepia::make_unique<sepia::number_parameter>(0, 0, 1000, true),
            "subkey 3",
            sepia::make_unique<sepia::boolean_parameter>(false)));
    parameter->parse(sepia::make_unique<std::stringstream>(R""(
        {
            "key 0": [0, 10, 20],
            "key 1": {
                "subkey 0": "g",
                "subkey 1": 5e-2,
                "subkey 2": 500,
                "subkey 3": true
            }
        }
    )""));
}
