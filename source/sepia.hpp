#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/// sepia bundles functions and classes to represent a camera and handle its raw stream of events.
namespace sepia {

    /// event_stream_version returns the implemented Event Stream version.
    inline std::array<uint8_t, 3> event_stream_version() {
        return {2, 0, 0};
    }

    /// event_stream_signature returns the Event Stream format signature.
    inline std::string event_stream_signature() {
        return "Event Stream";
    }

    /// event_stream_type associates an Event Stream type name with its byte.
    enum class event_stream_type : uint8_t {
        generic = 0,
        dvs = 1,
        atis = 2,
        color = 4,
    };

    /// make_unique creates a unique_ptr.
    template <typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    /// false_function is a function returning false.
    inline bool false_function() {
        return false;
    }

    /// generic_event represents the parameters of a generic event.
    struct generic_event {
        /// t represents the event's timestamp.
        uint64_t t;

        /// bytes stores the data payload associated with the event.
        std::vector<uint8_t> bytes;
    };

    /// dvs_event represents the parameters of a change detection.
    struct dvs_event {
        /// t represents the event's timestamp.
        uint64_t t;

        /// x represents the coordinate of the event on the sensor grid alongside the horizontal axis.
        /// x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the vertical axis.
        /// y is 0 on the bottom, and increases from bottom to top.
        uint16_t y;

        /// is_increase is false if the light is decreasing.
        bool is_increase;
    } __attribute__((packed));

    /// atis_event represents the parameters of a change detection or an exposure measurement.
    struct atis_event {
        /// t represents the event's timestamp.
        uint64_t t;

        /// x represents the coordinate of the event on the sensor grid alongside the horizontal axis.
        /// x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the vertical axis.
        /// y is 0 on the bottom, and increases bottom to top.
        uint16_t y;

        /// is_threshold_crossing is false if the event is a change detection, and true if it is a threshold crossing.
        bool is_threshold_crossing;

        /// change detection: polarity is false if the light is decreasing.
        /// exposure measurement: polarity is false for a first threshold crossing.
        bool polarity;
    } __attribute__((packed));

    /// threshold_crossing represent the parameters of a threshold crossing.
    struct threshold_crossing {
        /// t represents the event's timestamp.
        uint64_t t;

        /// x represents the coordinate of the event on the sensor grid alongside the horizontal axis.
        /// x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the vertical axis.
        /// y is 0 on the bottom, and increases from bottom to top.
        uint16_t y;

        /// is_second is false if the event is a first threshold crossing.
        bool is_second;
    } __attribute__((packed));

    /// color_event represents the parameters of a color event.
    struct color_event {
        /// t represents the event's timestamp.
        uint64_t t;

        /// x represents the coordinate of the event on the sensor grid alongside the horizontal axis.
        /// x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the vertical axis.
        /// y is 0 on the bottom, and increases bottom to top.
        uint16_t y;

        /// r represents the red component of the color.
        uint8_t r;

        /// g represents the green component of the color.
        uint8_t g;

        /// b represents the blue component of the color.
        uint8_t b;
    } __attribute__((packed));

    /// unreadable_file is thrown when an input file does not exist or is not readable.
    class unreadable_file : public std::runtime_error {
        public:
        unreadable_file(const std::string& filename) :
            std::runtime_error("The file '" + filename + "' could not be open for reading") {}
    };

    /// unwritable_file is thrown whenan output file is not writable.
    class unwritable_file : public std::runtime_error {
        public:
        unwritable_file(const std::string& filename) :
            std::runtime_error("The file '" + filename + "'' could not be open for writing") {}
    };

    /// wrong_signature is thrown when an input file does not have the expected signature.
    class wrong_signature : public std::runtime_error {
        public:
        wrong_signature() : std::runtime_error("The stream does not have the expected signature") {}
    };

    /// unsupported_version is thrown when an Event Stream file uses an unsupported version.
    class unsupported_version : public std::runtime_error {
        public:
        unsupported_version() : std::runtime_error("The stream uses an unsupported version") {}
    };

    /// incomplete_header is thrown when the end of file is reached while reading the header.
    class incomplete_header : public std::runtime_error {
        public:
        incomplete_header() : std::runtime_error("The stream has an incomplete header") {}
    };

    /// unsupported_event_type is thrown when an Event Stream file uses an unsupported event type.
    class unsupported_event_type : public std::runtime_error {
        public:
        unsupported_event_type() : std::runtime_error("The stream uses an unsupported event type") {}
    };

    /// coordinates_overflow is thrown when an event has coordinates outside the range provided by the header.
    class coordinates_overflow : public std::runtime_error {
        public:
        coordinates_overflow() : std::runtime_error("An event has coordinates outside the header-provided range") {}
    };

    /// end_of_file is thrown when the end of an input file is reached.
    class end_of_file : public std::runtime_error {
        public:
        end_of_file() : std::runtime_error("End of file reached") {}
    };

    /// no_device_connected is thrown when device auto-select is called without devices connected.
    class no_device_connected : public std::runtime_error {
        public:
        no_device_connected(const std::string& device_family) :
            std::runtime_error("No " + device_family + " is connected") {}
    };

    /// device_disconnected is thrown when an active device is disonnected.
    class device_disconnected : public std::runtime_error {
        public:
        device_disconnected(const std::string& device_name) : std::runtime_error(device_name + " disconnected") {}
    };

    /// parse_error is thrown when a JSON parse error occurs.
    class parse_error : public std::runtime_error {
        public:
        parse_error(const std::string& what, std::size_t character_count, std::size_t line_count) :
            std::runtime_error(
                "JSON parse error: " + what + " (line " + std::to_string(line_count) + ":"
                + std::to_string(character_count) + ")") {}
    };

    /// parameter_error is a logical error regarding a parameter.
    class parameter_error : public std::logic_error {
        public:
        parameter_error(const std::string& what) : std::logic_error(what) {}
    };

    /// filename_to_ifstream creates a readable stream from a file.
    inline std::unique_ptr<std::ifstream> filename_to_ifstream(const std::string& filename) {
        auto stream = make_unique<std::ifstream>(filename);
        if (!stream->good()) {
            throw unreadable_file(filename);
        }
        return stream;
    }

    /// filename_to_ofstream creates a writable stream from a file.
    inline std::unique_ptr<std::ofstream> filename_to_ofstream(const std::string& filename) {
        auto stream = make_unique<std::ofstream>(filename);
        if (!stream->good()) {
            throw unwritable_file(filename);
        }
        return stream;
    }

    /// dirname returns the directory part of the given path.
    inline std::string dirname(const std::string& path) {
        for (std::size_t index = path.size();;) {
            index = path.find_last_of('/', index);
            if (index == std::string::npos) {
                return ".";
            }
            if (index == 0 || path[index - 1] != '\\') {
                return path.substr(0, index);
            }
        }
    }

    /// join concatenates several path components.
    template <typename Iterator>
    std::string join(Iterator begin, Iterator end) {
        std::string path;
        for (; begin != end; ++begin) {
            path += *begin;
            if (!path.empty() && begin != std::prev(end) && path.back() != '/') {
                path.push_back('/');
            }
        }
        return path;
    }
    std::string join(std::initializer_list<std::string> components) {
        return join(components.begin(), components.end());
    }

    /// header bundles the parameters composing the stream's header.
    struct header {
        std::array<uint8_t, 3> event_stream_version;
        event_stream_type event_stream_type;
        uint16_t width;
        uint16_t height;
    };

    /// read_header checks the header and retrieves meta-information from the given stream.
    inline header read_header(std::istream& event_stream) {
        {
            auto read_signature = event_stream_signature();
            event_stream.read(&read_signature[0], read_signature.size());
            if (event_stream.eof() || read_signature != event_stream_signature()) {
                throw wrong_signature();
            }
        }
        header header = {};
        {
            event_stream.read(
                reinterpret_cast<char*>(header.event_stream_version.data()), header.event_stream_version.size());
            if (event_stream.eof()) {
                throw incomplete_header();
            }
            if (std::get<0>(header.event_stream_version) != std::get<0>(event_stream_version())
                || std::get<1>(header.event_stream_version) < std::get<1>(event_stream_version())) {
                throw unsupported_version();
            }
        }
        {
            const char type_char = event_stream.get();
            if (event_stream.eof()) {
                throw incomplete_header();
            }
            const auto type_byte = *reinterpret_cast<const uint8_t*>(&type_char);
            if (type_byte == static_cast<uint8_t>(event_stream_type::generic)) {
                header.event_stream_type = event_stream_type::generic;
            } else if (type_byte == static_cast<uint8_t>(event_stream_type::dvs)) {
                header.event_stream_type = event_stream_type::dvs;
            } else if (type_byte == static_cast<uint8_t>(event_stream_type::atis)) {
                header.event_stream_type = event_stream_type::atis;
            } else if (type_byte == static_cast<uint8_t>(event_stream_type::color)) {
                header.event_stream_type = event_stream_type::color;
            } else {
                throw unsupported_event_type();
            }
        }
        if (header.event_stream_type != event_stream_type::generic) {
            std::array<uint8_t, 4> size_bytes;
            event_stream.read(reinterpret_cast<char*>(size_bytes.data()), size_bytes.size());
            if (event_stream.eof()) {
                throw incomplete_header();
            }
            header.width =
                (static_cast<uint16_t>(std::get<0>(size_bytes))
                 | (static_cast<uint16_t>(std::get<1>(size_bytes)) << 8));
            header.height =
                (static_cast<uint16_t>(std::get<2>(size_bytes))
                 | (static_cast<uint16_t>(std::get<3>(size_bytes)) << 8));
        }
        return header;
    }

    /// read_header checks the header and retrieves meta-information from the given stream.
    inline header read_header(std::unique_ptr<std::istream> event_stream) {
        return read_header(*event_stream);
    }

    /// write_header writes the header bytes to an byte stream.
    inline void
    write_header(std::ostream& event_stream, event_stream_type event_stream_type, uint16_t width, uint16_t height) {
        event_stream.write("Event Stream", 12);
        event_stream.write(reinterpret_cast<char*>(event_stream_version().data()), event_stream_version().size());
        auto type_byte = static_cast<uint8_t>(event_stream_type);
        event_stream.put(*reinterpret_cast<char*>(&type_byte));
        if (event_stream_type != event_stream_type::generic) {
            event_stream.put(width & 0b11111111);
            event_stream.put((width & 0b1111111100000000) >> 8);
            event_stream.put(height & 0b11111111);
            event_stream.put((height & 0b1111111100000000) >> 8);
        }
    }

    /// split separates a stream of ATIS events into a stream of DVS events and a stream of theshold crossings.
    template <typename HandleDvsEvent, typename HandleThresholdCrossing>
    class split {
        public:
        split(HandleDvsEvent handle_dvs_event, HandleThresholdCrossing handle_threshold_crossing) :
            _handle_dvs_event(std::forward<HandleDvsEvent>(handle_dvs_event)),
            _handle_threshold_crossing(std::forward<HandleThresholdCrossing>(handle_threshold_crossing)) {}
        split(const split&) = delete;
        split(split&&) = default;
        split& operator=(const split&) = delete;
        split& operator=(split&&) = default;
        virtual ~split() {}

        /// operator() handles an event.
        virtual void operator()(atis_event atis_event) {
            if (atis_event.is_threshold_crossing) {
                _handle_threshold_crossing(
                    threshold_crossing{atis_event.t, atis_event.x, atis_event.y, atis_event.polarity});
            } else {
                _handle_dvs_event(dvs_event{atis_event.t, atis_event.x, atis_event.y, atis_event.polarity});
            }
        }

        protected:
        HandleDvsEvent _handle_dvs_event;
        HandleThresholdCrossing _handle_threshold_crossing;
    };

    /// make_split creates a split from functors.
    template <typename HandleDvsEvent, typename HandleThresholdCrossing>
    split<HandleDvsEvent, HandleThresholdCrossing>
    make_split(HandleDvsEvent handle_dvs_event, HandleThresholdCrossing handle_threshold_crossing) {
        return split<HandleDvsEvent, HandleThresholdCrossing>(
            std::forward<HandleDvsEvent>(handle_dvs_event),
            std::forward<HandleThresholdCrossing>(handle_threshold_crossing));
    }

    /// event_stream_observable is a base class for event stream observables.
    class event_stream_observable {
        public:
        /// dispatch specifies when the events are dispatched.
        enum class dispatch {
            synchronously_but_skip_offset,
            synchronously,
            as_fast_as_possible,
        };

        event_stream_observable() {}
        event_stream_observable(const event_stream_observable&) = delete;
        event_stream_observable(event_stream_observable&&) = default;
        event_stream_observable& operator=(const event_stream_observable&) = delete;
        event_stream_observable& operator=(event_stream_observable&&) = default;
        virtual ~event_stream_observable() {}

        protected:
        /// read_and_dispatch implements a generic dispatch mechanism for event stream files.
        template <
            typename Event,
            typename HandleByte,
            typename MustRestart,
            typename HandleEvent,
            typename HandleException>
        static void read_and_dispatch(
            std::istream& event_stream,
            std::atomic_bool& running,
            event_stream_observable::dispatch dispatch,
            std::size_t chunk_size,
            HandleByte handle_byte,
            MustRestart must_restart,
            HandleEvent handle_event,
            HandleException handle_exception) {
            try {
                Event event = {};
                std::vector<uint8_t> bytes(chunk_size);
                switch (dispatch) {
                    case event_stream_observable::dispatch::synchronously_but_skip_offset: {
                        auto offset_skipped = false;
                        auto time_reference = std::chrono::system_clock::now();
                        uint64_t initial_t = 0;
                        uint64_t previous_t = 0;
                        while (running.load(std::memory_order_relaxed)) {
                            event_stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                            if (event_stream.eof()) {
                                for (auto byte_iterator = bytes.begin();
                                     byte_iterator != std::next(bytes.begin(), event_stream.gcount());
                                     ++byte_iterator) {
                                    if (handle_byte(*byte_iterator, event)) {
                                        if (offset_skipped) {
                                            if (event.t > previous_t) {
                                                previous_t = event.t;
                                                std::this_thread::sleep_until(
                                                    time_reference + std::chrono::microseconds(event.t - initial_t));
                                            }
                                        } else {
                                            offset_skipped = true;
                                            initial_t = event.t;
                                            previous_t = event.t;
                                        }
                                        handle_event(event);
                                    }
                                }
                                if (must_restart()) {
                                    event_stream.clear();
                                    event_stream.seekg(
                                        event_stream_signature().size() + event_stream_version().size() + 1);
                                    offset_skipped = false;
                                    handle_byte.reset();
                                    event = Event{};
                                    time_reference = std::chrono::system_clock::now();
                                    continue;
                                }
                                throw end_of_file();
                            }
                            for (auto byte : bytes) {
                                if (handle_byte(byte, event)) {
                                    if (offset_skipped) {
                                        if (event.t > previous_t) {
                                            previous_t = event.t;
                                            std::this_thread::sleep_until(
                                                time_reference + std::chrono::microseconds(event.t - initial_t));
                                        }
                                    } else {
                                        offset_skipped = true;
                                        initial_t = event.t;
                                        previous_t = event.t;
                                    }
                                    handle_event(event);
                                }
                            }
                        }
                    }
                    case event_stream_observable::dispatch::synchronously: {
                        auto time_reference = std::chrono::system_clock::now();
                        uint64_t previous_t = 0;
                        while (running.load(std::memory_order_relaxed)) {
                            event_stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                            if (event_stream.eof()) {
                                for (auto byte_iterator = bytes.begin();
                                     byte_iterator != std::next(bytes.begin(), event_stream.gcount());
                                     ++byte_iterator) {
                                    if (handle_byte(*byte_iterator, event)) {
                                        if (event.t > previous_t) {
                                            std::this_thread::sleep_until(
                                                time_reference + std::chrono::microseconds(event.t));
                                        }
                                        previous_t = event.t;
                                        handle_event(event);
                                    }
                                }
                                if (must_restart()) {
                                    event_stream.clear();
                                    event_stream.seekg(
                                        event_stream_signature().size() + event_stream_version().size() + 1);
                                    handle_byte.reset();
                                    event = Event{};
                                    time_reference = std::chrono::system_clock::now();
                                    continue;
                                }
                                throw end_of_file();
                            }
                            for (auto byte : bytes) {
                                if (handle_byte(byte, event)) {
                                    if (event.t > previous_t) {
                                        std::this_thread::sleep_until(
                                            time_reference + std::chrono::microseconds(event.t));
                                    }
                                    previous_t = event.t;
                                    handle_event(event);
                                }
                            }
                        }
                    }
                    case event_stream_observable::dispatch::as_fast_as_possible: {
                        while (running.load(std::memory_order_relaxed)) {
                            event_stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                            if (event_stream.eof()) {
                                for (auto byte_iterator = bytes.begin();
                                     byte_iterator != std::next(bytes.begin(), event_stream.gcount());
                                     ++byte_iterator) {
                                    if (handle_byte(*byte_iterator, event)) {
                                        handle_event(event);
                                    }
                                }
                                if (must_restart()) {
                                    event_stream.clear();
                                    event_stream.seekg(
                                        event_stream_signature().size() + event_stream_version().size() + 1);
                                    handle_byte.reset();
                                    event = Event{};
                                    continue;
                                }
                                throw end_of_file();
                            }
                            for (auto byte : bytes) {
                                if (handle_byte(byte, event)) {
                                    handle_event(event);
                                }
                            }
                        }
                    }
                }
            } catch (...) {
                handle_exception(std::current_exception());
            }
        }
    };

    /// capture_exception stores an exception pointer and notifies a condition variable.
    /// It is used internally by join_ functions.
    class capture_exception {
        public:
        capture_exception() {}
        capture_exception(const capture_exception&) = delete;
        capture_exception(capture_exception&&) = default;
        capture_exception& operator=(const capture_exception&) = delete;
        capture_exception& operator=(capture_exception&&) = default;
        virtual ~capture_exception() {}

        /// operator() handles an exception.
        virtual void operator()(std::exception_ptr exception) {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _exception = exception;
            }
            _condition_variable.notify_one();
        }

        /// wait blocks until the held exception is set.
        virtual void wait() {
            std::unique_lock<std::mutex> exception_lock(_mutex);
            if (_exception == nullptr) {
                _condition_variable.wait(exception_lock, [&] { return _exception != nullptr; });
            }
        }

        /// rethrow_unless raises the internally held exception unless it matches one of the given types.
        template <typename... Exceptions>
        void rethrow_unless() {
            catch_index<0, Exceptions...>();
        }

        protected:
        /// catch_index catches the n-th exception type.
        template <std::size_t index, typename... Exceptions>
            typename std::enable_if < index<sizeof...(Exceptions), void>::type catch_index() {
            using Exception = typename std::tuple_element<index, std::tuple<Exceptions...>>::type;
            try {
                catch_index<index + 1, Exceptions...>();
            } catch (const Exception&) {
                return;
            }
        }

        /// catch_index is a termination for the template loop.
        template <std::size_t index, typename... Exceptions>
        typename std::enable_if<index == sizeof...(Exceptions), void>::type catch_index() {
            std::rethrow_exception(_exception);
        }

        std::mutex _mutex;
        std::condition_variable _condition_variable;
        std::exception_ptr _exception;
    };

    /// generic_event_stream_writer writes events to a generic Event Stream file.
    class generic_event_stream_writer {
        public:
        generic_event_stream_writer() : _previous_t(0) {
            _accessing_event_stream.clear(std::memory_order_release);
        }
        generic_event_stream_writer(const generic_event_stream_writer&) = delete;
        generic_event_stream_writer(generic_event_stream_writer&&) = default;
        generic_event_stream_writer& operator=(const generic_event_stream_writer&) = delete;
        generic_event_stream_writer& operator=(generic_event_stream_writer&&) = default;
        virtual ~generic_event_stream_writer() {}

        /// operator() handles an event.
        virtual void operator()(generic_event generic_event) {
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (_event_stream) {
                auto relative_t = generic_event.t - _previous_t;
                if (relative_t >= 0b11111110) {
                    const auto number_of_overflows = relative_t / 0b11111110;
                    for (std::size_t index = 0; index < number_of_overflows; ++index) {
                        _event_stream->put(0b11111111);
                    }
                    relative_t -= number_of_overflows * 0b11111110;
                }
                _event_stream->put(relative_t);
                for (std::size_t size = generic_event.bytes.size(); size > 0; size >>= 7) {
                    _event_stream->put(((size & 0b1111111) << 1) | ((size >> 7) == 0 ? 0 : 1));
                }
                _event_stream->write(
                    reinterpret_cast<const char*>(generic_event.bytes.data()), generic_event.bytes.size());
                _previous_t = generic_event.t;
            }
            _accessing_event_stream.clear(std::memory_order_release);
        }

        /// open is a thread-safe method to start logging events to the given stream.
        virtual void open(std::unique_ptr<std::ostream> event_stream) {
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (_event_stream) {
                _accessing_event_stream.clear(std::memory_order_release);
                throw std::runtime_error("Already logging");
            }
            _event_stream = std::move(event_stream);
            write_header(*_event_stream, event_stream_type::generic, 0, 0);
            _accessing_event_stream.clear(std::memory_order_release);
        }

        /// target is a thread-safe method to start logging events to the given file.
        virtual void open(const std::string& filename) {
            auto event_stream = make_unique<std::ofstream>(filename);
            if (!event_stream->good()) {
                throw unwritable_file(filename);
            }
            open(std::move(event_stream));
        }

        /// close is a thread-safe method to stop logging the events and close the file.
        virtual void close() {
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (!_event_stream) {
                _accessing_event_stream.clear(std::memory_order_release);
                throw std::runtime_error("Was not logging");
            }
            _event_stream.reset();
            _previous_t = 0;
            _accessing_event_stream.clear(std::memory_order_release);
        }

        protected:
        std::unique_ptr<std::ostream> _event_stream;
        std::atomic_flag _accessing_event_stream;
        uint64_t _previous_t;
    };

    /// handle_generic_byte implements the event stream state machine for generic events.
    class handle_generic_byte {
        public:
        handle_generic_byte() : _state(state::idle), _index(0), _bytes_size(0) {}
        handle_generic_byte(const handle_generic_byte&) = default;
        handle_generic_byte(handle_generic_byte&&) = default;
        handle_generic_byte& operator=(const handle_generic_byte&) = default;
        handle_generic_byte& operator=(handle_generic_byte&&) = default;
        virtual ~handle_generic_byte() {}

        /// operator() handles a byte.
        virtual bool operator()(uint8_t byte, generic_event& generic_event) {
            switch (_state) {
                case state::idle: {
                    if (byte == 0b11111111) {
                        generic_event.t += 0b11111110;
                    } else if (byte != 0b11111110) {
                        generic_event.t += byte;
                        _state = state::byte0;
                    }
                }
                case state::byte0: {
                    _bytes_size |= (((byte & 0b1111111) >> 1) << (7 * _index));
                    if ((byte & 1) == 0) {
                        generic_event.bytes.clear();
                        _index = 0;
                        if (_bytes_size == 0) {
                            _state = state::idle;
                            return true;
                        }
                        generic_event.bytes.reserve(_bytes_size);
                        _state = state::size_byte;
                    }
                }
                case state::size_byte: {
                    generic_event.bytes.push_back(byte);
                    if (generic_event.bytes.size() == _bytes_size) {
                        _state = state::idle;
                        _index = 0;
                        _bytes_size = 0;
                        return true;
                    }
                }
            }
            return false;
        }

        /// reset initializes the state machine.
        virtual void reset() {
            _state = state::idle;
            _index = 0;
            _bytes_size = 0;
        }

        protected:
        /// state represents the current state machine's state.
        enum class state {
            idle,
            byte0,
            size_byte,
        };

        state _state;
        std::size_t _index;
        std::size_t _bytes_size;
    };

    /// generic_event_stream_observable is a template-specialized event_stream_observable for generic events.
    template <typename HandleEvent, typename HandleException, typename MustRestart>
    class generic_event_stream_observable : public event_stream_observable {
        public:
        generic_event_stream_observable(
            std::unique_ptr<std::istream> event_stream,
            HandleEvent handle_event,
            HandleException handle_exception,
            MustRestart must_restart,
            event_stream_observable::dispatch dispatch,
            std::size_t chunk_size) :
            _event_stream(std::move(event_stream)),
            _running(true) {
            const auto header = read_header(*_event_stream);
            if (header.event_stream_type != event_stream_type::generic) {
                throw unsupported_event_type();
            }
            _loop = std::thread(
                read_and_dispatch<generic_event, handle_generic_byte, MustRestart, HandleEvent, HandleException>,
                std::ref(*_event_stream),
                std::ref(_running),
                dispatch,
                chunk_size,
                handle_generic_byte(),
                std::forward<MustRestart>(must_restart),
                std::forward<HandleEvent>(handle_event),
                std::forward<HandleException>(handle_exception));
        }
        generic_event_stream_observable(const generic_event_stream_observable&) = delete;
        generic_event_stream_observable(generic_event_stream_observable&&) = default;
        generic_event_stream_observable& operator=(const generic_event_stream_observable&) = delete;
        generic_event_stream_observable& operator=(generic_event_stream_observable&&) = default;
        virtual ~generic_event_stream_observable() {
            _running.store(false, std::memory_order_relaxed);
            _loop.join();
        }

        protected:
        std::unique_ptr<std::istream> _event_stream;
        std::atomic_bool _running;
        std::thread _loop;
    };

    /// make_generic_event_stream_observable creates an event stream observable from functors.
    template <typename HandleEvent, typename HandleException, typename MustRestart = decltype(&false_function)>
    std::unique_ptr<generic_event_stream_observable<HandleEvent, HandleException, MustRestart>>
    make_generic_event_stream_observable(
        const std::string& filename,
        HandleEvent handle_event,
        HandleException handle_exception,
        MustRestart must_restart = &false_function,
        event_stream_observable::dispatch dispatch = event_stream_observable::dispatch::synchronously_but_skip_offset,
        std::size_t chunk_size = 1 << 10) {
        return sepia::make_unique<generic_event_stream_observable<HandleEvent, HandleException, MustRestart>>(
            filename,
            std::forward<HandleEvent>(handle_event),
            std::forward<HandleException>(handle_exception),
            std::forward<MustRestart>(must_restart),
            dispatch,
            chunk_size);
    }

    /// join_generic_event_stream_observable creates an event stream observable from functors and blocks until the end
    /// of the input file is reached.
    template <typename HandleEvent>
    void join_generic_event_stream_observable(
        const std::string& filename,
        HandleEvent handle_event,
        std::size_t chunk_size = 1 << 10) {
        capture_exception capture_observable_exception;
        auto generic_event_stream_observable = make_generic_event_stream_observable(
            filename,
            std::forward<HandleEvent>(handle_event),
            std::ref(capture_observable_exception),
            &false_function,
            event_stream_observable::dispatch::as_fast_as_possible,
            chunk_size);
        capture_observable_exception.wait();
        capture_observable_exception.rethrow_unless<end_of_file>();
    }

    /// dvs_event_stream_writer writes events to an Event Stream file.
    class dvs_event_stream_writer {
        public:
        dvs_event_stream_writer() : _logging(false), _previous_t(0) {
            _accessing_event_stream.clear(std::memory_order_release);
        }
        dvs_event_stream_writer(const dvs_event_stream_writer&) = delete;
        dvs_event_stream_writer(dvs_event_stream_writer&&) = default;
        dvs_event_stream_writer& operator=(const dvs_event_stream_writer&) = delete;
        dvs_event_stream_writer& operator=(dvs_event_stream_writer&&) = default;
        virtual ~dvs_event_stream_writer() {}

        /// operator() handles an event.
        virtual void operator()(dvs_event dvs_event) {
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (_logging) {
                auto relative_t = dvs_event.t - _previous_t;
                if (relative_t >= 0b1111111) {
                    const auto number_of_overflows = relative_t / 0b1111111;
                    for (std::size_t index = 0; index < number_of_overflows / 0b1111111; ++index) {
                        _event_stream.put(0b11111111);
                    }
                    relative_t -= number_of_overflows * 0b1111111;
                }
                _event_stream.put((relative_t << 1) | (dvs_event.is_increase ? 1 : 0));
                _event_stream.put(dvs_event.x & 0b11111111);
                _event_stream.put((dvs_event.x & 0b1111111100000000) >> 8);
                _event_stream.put(dvs_event.y & 0b11111111);
                _event_stream.put((dvs_event.y & 0b1111111100000000) >> 8);
                _previous_t = dvs_event.t;
            }
            _accessing_event_stream.clear(std::memory_order_release);
        }

        /// open is a thread-safe method to start logging events to the given file.
        virtual void open(const std::string& filename, uint16_t width, uint16_t height) {
            _event_stream.open(filename, std::ifstream::binary);
            if (!_event_stream.good()) {
                throw unwritable_file(filename);
            }
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (_logging) {
                _accessing_event_stream.clear(std::memory_order_release);
                throw std::runtime_error("Already logging");
            }
            write_header(_event_stream, event_stream_type::dvs, width, height);
            _logging = true;
            _accessing_event_stream.clear(std::memory_order_release);
        }

        /// close is a thread-safe method to stop logging the events and close the file.
        virtual void close() {
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (!_logging) {
                _accessing_event_stream.clear(std::memory_order_release);
                throw std::runtime_error("Was not logging");
            }
            _logging = false;
            _event_stream.close();
            _previous_t = 0;
            _accessing_event_stream.clear(std::memory_order_release);
        }

        protected:
        std::ofstream _event_stream;
        bool _logging;
        std::atomic_flag _accessing_event_stream;
        uint64_t _previous_t;
    };

    /// handle_dvs_byte implements the event stream state machine for DVS events.
    class handle_dvs_byte {
        public:
        handle_dvs_byte() : _state(state::idle) {}
        handle_dvs_byte(const handle_dvs_byte&) = default;
        handle_dvs_byte(handle_dvs_byte&&) = default;
        handle_dvs_byte& operator=(const handle_dvs_byte&) = default;
        handle_dvs_byte& operator=(handle_dvs_byte&&) = default;
        virtual ~handle_dvs_byte() {}

        /// operator() handles a byte.
        virtual bool operator()(uint8_t byte, dvs_event& dvs_event) {
            switch (_state) {
                case state::idle: {
                    if (byte == 0b11111111) {
                        dvs_event.t += 0b11111110;
                    } else if (byte != 0b11111110) {
                        dvs_event.t += ((byte & 0b1111111) >> 1);
                        dvs_event.is_increase = ((byte & 1) == 1);
                        _state = state::byte0;
                    }
                    return false;
                }
                case state::byte0: {
                    dvs_event.x = byte;
                    _state = state::byte1;
                    return false;
                }
                case state::byte1: {
                    dvs_event.x |= (byte << 8);
                    _state = state::byte2;
                    return false;
                }
                case state::byte2: {
                    dvs_event.y = byte;
                    _state = state::byte3;
                    return false;
                }
                case state::byte3: {
                    dvs_event.y |= (byte << 8);
                    _state = state::idle;
                    return true;
                }
            }
        }

        /// reset initializes the state machine.
        virtual void reset() {
            _state = state::idle;
        }

        protected:
        /// state represents the current state machine's state.
        enum class state {
            idle,
            byte0,
            byte1,
            byte2,
            byte3,
        };

        state _state;
    };

    /// dvs_event_stream_observable is a template-specialized event_stream_observable for DVS events.
    template <typename HandleEvent, typename HandleException, typename MustRestart>
    class dvs_event_stream_observable : public event_stream_observable {
        public:
        dvs_event_stream_observable(
            std::unique_ptr<std::istream> event_stream,
            HandleEvent handle_event,
            HandleException handle_exception,
            MustRestart must_restart,
            event_stream_observable::dispatch dispatch,
            std::size_t chunk_size) :
            _event_stream(std::move(event_stream)),
            _running(true) {
            const auto header = read_header(*_event_stream);
            if (header.event_stream_type != event_stream_type::dvs) {
                throw unsupported_event_type();
            }
            _loop = std::thread(
                read_and_dispatch<dvs_event, handle_dvs_byte, MustRestart, HandleEvent, HandleException>,
                std::ref(*_event_stream),
                std::ref(_running),
                dispatch,
                chunk_size,
                handle_dvs_byte(),
                std::forward<MustRestart>(must_restart),
                std::forward<HandleEvent>(handle_event),
                std::forward<HandleException>(handle_exception));
        }
        dvs_event_stream_observable(const dvs_event_stream_observable&) = delete;
        dvs_event_stream_observable(dvs_event_stream_observable&&) = default;
        dvs_event_stream_observable& operator=(const dvs_event_stream_observable&) = delete;
        dvs_event_stream_observable& operator=(dvs_event_stream_observable&&) = default;
        virtual ~dvs_event_stream_observable() {
            _running.store(false, std::memory_order_relaxed);
            _loop.join();
        }

        protected:
        std::unique_ptr<std::istream> _event_stream;
        std::atomic_bool _running;
        std::thread _loop;
    };

    /// make_dvs_event_stream_observable creates an event stream observable from functors.
    template <typename HandleEvent, typename HandleException, typename MustRestart = decltype(&false_function)>
    std::unique_ptr<dvs_event_stream_observable<HandleEvent, HandleException, MustRestart>>
    make_dvs_event_stream_observable(
        std::unique_ptr<std::istream> event_stream,
        HandleEvent handle_event,
        HandleException handle_exception,
        MustRestart must_restart = &false_function,
        event_stream_observable::dispatch dispatch = event_stream_observable::dispatch::synchronously_but_skip_offset,
        std::size_t chunk_size = 1 << 10) {
        return sepia::make_unique<dvs_event_stream_observable<HandleEvent, HandleException, MustRestart>>(
            std::move(event_stream),
            std::forward<HandleEvent>(handle_event),
            std::forward<HandleException>(handle_exception),
            std::forward<MustRestart>(must_restart),
            dispatch,
            chunk_size);
    }

    /// join_dvs_event_stream_observable creates an event stream observable from functors and blocks until the end of
    /// the input file is reached.
    template <typename HandleEvent>
    void join_dvs_event_stream_observable(
        std::unique_ptr<std::istream> event_stream,
        HandleEvent handle_event,
        std::size_t chunk_size = 1 << 10) {
        capture_exception capture_observable_exception;
        auto dvs_event_stream_observable = make_dvs_event_stream_observable(
            std::move(event_stream),
            std::forward<HandleEvent>(handle_event),
            std::ref(capture_observable_exception),
            &false_function,
            event_stream_observable::dispatch::as_fast_as_possible,
            chunk_size);
        capture_observable_exception.wait();
        capture_observable_exception.rethrow_unless<end_of_file>();
    }

    /// atis_event_stream_writer writes events to an Event Stream file.
    class atis_event_stream_writer {
        public:
        atis_event_stream_writer() : _logging(false), _previous_t(0) {
            _accessing_event_stream.clear(std::memory_order_release);
        }
        atis_event_stream_writer(const atis_event_stream_writer&) = delete;
        atis_event_stream_writer(atis_event_stream_writer&&) = default;
        atis_event_stream_writer& operator=(const atis_event_stream_writer&) = delete;
        atis_event_stream_writer& operator=(atis_event_stream_writer&&) = default;
        virtual ~atis_event_stream_writer() {}

        /// operator() handles an event.
        virtual void operator()(atis_event atis_event) {
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (_logging) {
                auto relative_t = atis_event.t - _previous_t;
                if (relative_t >= 0b111111) {
                    const auto number_of_overflows = relative_t / 0b111111;
                    for (std::size_t index = 0; index < number_of_overflows / 4; ++index) {
                        _event_stream.put(0b11111111);
                    }
                    const auto number_of_overflows_left = number_of_overflows % 4;
                    if (number_of_overflows_left > 0) {
                        _event_stream.put(0b11111100 | number_of_overflows_left);
                    }
                    relative_t -= number_of_overflows * 0b111111;
                }

                _event_stream.put(
                    (relative_t << 2) | (atis_event.polarity ? 0b10 : 0b00)
                    | (atis_event.is_threshold_crossing ? 1 : 0));
                _event_stream.put(atis_event.x & 0b11111111);
                _event_stream.put((atis_event.x & 0b1111111100000000) >> 8);
                _event_stream.put(atis_event.y & 0b11111111);
                _event_stream.put((atis_event.y & 0b1111111100000000) >> 8);
                _previous_t = atis_event.t;
            }
            _accessing_event_stream.clear(std::memory_order_release);
        }

        /// open is a thread-safe method to start logging events to the given file.
        virtual void open(const std::string& filename, uint16_t width, uint16_t height) {
            _event_stream.open(filename, std::ifstream::binary);
            if (!_event_stream.good()) {
                throw unwritable_file(filename);
            }
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (_logging) {
                _accessing_event_stream.clear(std::memory_order_release);
                throw std::runtime_error("Already logging");
            }
            write_header(_event_stream, event_stream_type::atis, width, height);
            _logging = true;
            _accessing_event_stream.clear(std::memory_order_release);
        }

        /// close is a thread-safe method to stop logging the events and close the file.
        virtual void close() {
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (!_logging) {
                _accessing_event_stream.clear(std::memory_order_release);
                throw std::runtime_error("Was not logging");
            }
            _logging = false;
            _event_stream.close();
            _previous_t = 0;
            _accessing_event_stream.clear(std::memory_order_release);
        }

        protected:
        std::ofstream _event_stream;
        bool _logging;
        std::atomic_flag _accessing_event_stream;
        uint64_t _previous_t;
    };

    /// handle_atis_byte implements the event stream state machine for ATIS events.
    class handle_atis_byte {
        public:
        handle_atis_byte() : _state(state::idle) {}
        handle_atis_byte(const handle_atis_byte&) = default;
        handle_atis_byte(handle_atis_byte&&) = default;
        handle_atis_byte& operator=(const handle_atis_byte&) = default;
        handle_atis_byte& operator=(handle_atis_byte&&) = default;
        virtual ~handle_atis_byte() {}

        /// operator() handles a byte.
        virtual bool operator()(uint8_t byte, atis_event& atis_event) {
            switch (_state) {
                case state::idle: {
                    if ((byte & 0b11111100) == 0b11111100) {
                        atis_event.t += 0b11111110 * (byte & 0b11);
                    } else {
                        atis_event.t += ((byte & 0b1111111) >> 1);
                        atis_event.is_threshold_crossing = ((byte & 1) == 1);
                        atis_event.polarity = ((byte & 0b10) == 0b10);
                        _state = state::byte0;
                    }
                    return false;
                }
                case state::byte0: {
                    atis_event.x = byte;
                    _state = state::byte1;
                    return false;
                }
                case state::byte1: {
                    atis_event.x |= (byte << 8);
                    _state = state::byte2;
                    return false;
                }
                case state::byte2: {
                    atis_event.y = byte;
                    _state = state::byte3;
                    return false;
                }
                case state::byte3: {
                    atis_event.y |= (byte << 8);
                    _state = state::idle;
                    return true;
                }
            }
        }

        /// reset initializes the state machine.
        virtual void reset() {
            _state = state::idle;
        }

        protected:
        /// state represents the current state machine's state.
        enum class state {
            idle,
            byte0,
            byte1,
            byte2,
            byte3,
        };

        state _state;
    };

    /// atis_event_stream_observable is a template-specialized event_stream_observable for ATIS events.
    template <typename HandleEvent, typename HandleException, typename MustRestart>
    class atis_event_stream_observable : public event_stream_observable {
        public:
        atis_event_stream_observable(
            std::unique_ptr<std::istream> event_stream,
            HandleEvent handle_event,
            HandleException handle_exception,
            MustRestart must_restart,
            event_stream_observable::dispatch dispatch,
            std::size_t chunk_size) :
            _event_stream(std::move(event_stream)),
            _running(true) {
            const auto header = read_header(*_event_stream);
            if (header.event_stream_type != event_stream_type::atis) {
                throw unsupported_event_type();
            }
            _loop = std::thread(
                read_and_dispatch<atis_event, handle_atis_byte, MustRestart, HandleEvent, HandleException>,
                std::ref(*_event_stream),
                std::ref(_running),
                dispatch,
                chunk_size,
                handle_atis_byte(),
                std::forward<MustRestart>(must_restart),
                std::forward<HandleEvent>(handle_event),
                std::forward<HandleException>(handle_exception));
        }
        atis_event_stream_observable(const atis_event_stream_observable&) = delete;
        atis_event_stream_observable(atis_event_stream_observable&&) = default;
        atis_event_stream_observable& operator=(const atis_event_stream_observable&) = delete;
        atis_event_stream_observable& operator=(atis_event_stream_observable&&) = default;
        virtual ~atis_event_stream_observable() {
            _running.store(false, std::memory_order_relaxed);
            _loop.join();
        }

        protected:
        std::unique_ptr<std::istream> _event_stream;
        std::atomic_bool _running;
        std::thread _loop;
    };

    /// make_atis_event_stream_observable creates an event stream observable from functors.
    template <typename HandleEvent, typename HandleException, typename MustRestart = decltype(&false_function)>
    std::unique_ptr<atis_event_stream_observable<HandleEvent, HandleException, MustRestart>>
    make_atis_event_stream_observable(
        std::unique_ptr<std::istream> event_stream,
        HandleEvent handle_event,
        HandleException handle_exception,
        MustRestart must_restart = &false_function,
        event_stream_observable::dispatch dispatch = event_stream_observable::dispatch::synchronously_but_skip_offset,
        std::size_t chunk_size = 1 << 10) {
        return sepia::make_unique<atis_event_stream_observable<HandleEvent, HandleException, MustRestart>>(
            std::move(event_stream),
            std::forward<HandleEvent>(handle_event),
            std::forward<HandleException>(handle_exception),
            std::forward<MustRestart>(must_restart),
            dispatch,
            chunk_size);
    }

    /// join_atis_event_stream_observable creates an event stream observable from functors and blocks until the end of
    /// the input file is reached.
    template <typename HandleEvent>
    void join_atis_event_stream_observable(
        std::unique_ptr<std::istream> event_stream,
        HandleEvent handle_event,
        std::size_t chunk_size = 1 << 10) {
        capture_exception capture_observable_exception;
        auto atis_event_stream_observable = make_atis_event_stream_observable(
            std::move(event_stream),
            std::forward<HandleEvent>(handle_event),
            std::ref(capture_observable_exception),
            &false_function,
            event_stream_observable::dispatch::as_fast_as_possible,
            chunk_size);
        capture_observable_exception.wait();
        capture_observable_exception.rethrow_unless<end_of_file>();
    }

    /// color_event_stream_writer writes events to a color Event Stream file.
    class color_event_stream_writer {
        public:
        color_event_stream_writer() : _logging(false), _previous_t(0) {
            _accessing_event_stream.clear(std::memory_order_release);
        }
        color_event_stream_writer(const color_event_stream_writer&) = delete;
        color_event_stream_writer(color_event_stream_writer&&) = default;
        color_event_stream_writer& operator=(const color_event_stream_writer&) = delete;
        color_event_stream_writer& operator=(color_event_stream_writer&&) = default;
        virtual ~color_event_stream_writer() {}

        /// operator() handles an event.
        virtual void operator()(color_event color_event) {
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (_logging) {
                auto relative_t = color_event.t - _previous_t;
                if (relative_t >= 0b11111110) {
                    const auto number_of_overflows = relative_t / 0b11111110;
                    for (std::size_t index = 0; index < number_of_overflows; ++index) {
                        _event_stream.put(static_cast<uint8_t>(0b11111111));
                    }
                    relative_t -= number_of_overflows * 0b11111110;
                }
                _event_stream.put(relative_t);
                _event_stream.put(color_event.x & 0b11111111);
                _event_stream.put((color_event.x & 0b1111111100000000) >> 8);
                _event_stream.put(color_event.y & 0b11111111);
                _event_stream.put((color_event.y & 0b1111111100000000) >> 8);
                _event_stream.put(color_event.r);
                _event_stream.put(color_event.g);
                _event_stream.put(color_event.b);
                _previous_t = color_event.t;
            }
            _accessing_event_stream.clear(std::memory_order_release);
        }

        /// open is a thread-safe method to start logging events to the given file.
        virtual void open(const std::string& filename, uint16_t width, uint16_t height) {
            _event_stream.open(filename, std::ifstream::binary);
            if (!_event_stream.good()) {
                throw unwritable_file(filename);
            }
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (_logging) {
                _accessing_event_stream.clear(std::memory_order_release);
                throw std::runtime_error("Already logging");
            }
            write_header(_event_stream, event_stream_type::color, width, height);
            _logging = true;
            _accessing_event_stream.clear(std::memory_order_release);
        }

        /// close is a thread-safe method to stop logging the events and close the file.
        virtual void close() {
            while (_accessing_event_stream.test_and_set(std::memory_order_acquire)) {
            }
            if (!_logging) {
                _accessing_event_stream.clear(std::memory_order_release);
                throw std::runtime_error("Was not logging");
            }
            _logging = false;
            _event_stream.close();
            _previous_t = 0;
            _accessing_event_stream.clear(std::memory_order_release);
        }

        protected:
        std::ofstream _event_stream;
        bool _logging;
        std::atomic_flag _accessing_event_stream;
        uint64_t _previous_t;
    };

    /// handle_color_byte implements the event stream state machine for color events.
    class handle_color_byte {
        public:
        handle_color_byte() : _state(state::idle) {}
        handle_color_byte(const handle_color_byte&) = default;
        handle_color_byte(handle_color_byte&&) = default;
        handle_color_byte& operator=(const handle_color_byte&) = default;
        handle_color_byte& operator=(handle_color_byte&&) = default;
        virtual ~handle_color_byte() {}

        /// operator() handles a byte.
        virtual bool operator()(uint8_t byte, color_event& color_event) {
            switch (_state) {
                case state::idle: {
                    if (byte == 0b11111111) {
                        color_event.t += 0b11111110;
                    } else if (byte != 0b11111110) {
                        color_event.t += byte;
                        _state = state::byte0;
                    }
                    return false;
                }
                case state::byte0: {
                    color_event.x = byte;
                    _state = state::byte1;
                    return false;
                }
                case state::byte1: {
                    color_event.x |= (byte << 8);
                    _state = state::byte2;
                    return false;
                }
                case state::byte2: {
                    color_event.y = byte;
                    _state = state::byte3;
                    return false;
                }
                case state::byte3: {
                    color_event.y |= (byte << 8);
                    _state = state::byte4;
                    return false;
                }
                case state::byte4: {
                    color_event.r = byte;
                    _state = state::byte5;
                    return false;
                }
                case state::byte5: {
                    color_event.g = byte;
                    _state = state::byte6;
                    return false;
                }
                case state::byte6: {
                    color_event.b = byte;
                    _state = state::idle;
                    return true;
                }
            }
        }

        /// reset initializes the state machine.
        virtual void reset() {
            _state = state::idle;
        }

        protected:
        /// state represents the current state machine's state.
        enum class state {
            idle,
            byte0,
            byte1,
            byte2,
            byte3,
            byte4,
            byte5,
            byte6,
        };

        state _state;
    };

    /// color_event_stream_observable is a template-specialized event_stream_observable for color events.
    template <typename HandleEvent, typename HandleException, typename MustRestart>
    class color_event_stream_observable : public event_stream_observable {
        public:
        color_event_stream_observable(
            std::unique_ptr<std::istream> event_stream,
            HandleEvent handle_event,
            HandleException handle_exception,
            MustRestart must_restart,
            event_stream_observable::dispatch dispatch,
            std::size_t chunk_size) :
            _event_stream(std::move(event_stream)),
            _running(true) {
            const auto header = read_header(*_event_stream);
            if (header.event_stream_type != event_stream_type::color) {
                throw unsupported_event_type();
            }
            _loop = std::thread(
                read_and_dispatch<color_event, handle_color_byte, MustRestart, HandleEvent, HandleException>,
                std::ref(*_event_stream),
                std::ref(_running),
                dispatch,
                chunk_size,
                handle_color_byte(),
                std::forward<MustRestart>(must_restart),
                std::forward<HandleEvent>(handle_event),
                std::forward<HandleException>(handle_exception));
        }
        color_event_stream_observable(const color_event_stream_observable&) = delete;
        color_event_stream_observable(color_event_stream_observable&&) = default;
        color_event_stream_observable& operator=(const color_event_stream_observable&) = delete;
        color_event_stream_observable& operator=(color_event_stream_observable&&) = default;
        virtual ~color_event_stream_observable() {
            _running.store(false, std::memory_order_relaxed);
            _loop.join();
        }

        protected:
        std::unique_ptr<std::istream> _event_stream;
        std::atomic_bool _running;
        std::thread _loop;
    };

    /// make_color_event_stream_observable creates an event stream observable from functors.
    template <typename HandleEvent, typename HandleException, typename MustRestart = decltype(&false_function)>
    std::unique_ptr<color_event_stream_observable<HandleEvent, HandleException, MustRestart>>
    make_color_event_stream_observable(
        std::unique_ptr<std::istream> event_stream,
        HandleEvent handle_event,
        HandleException handle_exception,
        MustRestart must_restart = &false_function,
        event_stream_observable::dispatch dispatch = event_stream_observable::dispatch::synchronously_but_skip_offset,
        std::size_t chunk_size = 1 << 10) {
        return sepia::make_unique<color_event_stream_observable<HandleEvent, HandleException, MustRestart>>(
            std::move(event_stream),
            std::forward<HandleEvent>(handle_event),
            std::forward<HandleException>(handle_exception),
            std::forward<MustRestart>(must_restart),
            dispatch,
            chunk_size);
    }

    /// join_color_event_stream_observable creates an event stream observable from functors and blocks until the end of
    /// the input file is reached.
    template <typename HandleEvent>
    void join_color_event_stream_observable(
        std::unique_ptr<std::istream> event_stream,
        HandleEvent handle_event,
        std::size_t chunk_size = 1 << 10) {
        capture_exception capture_observable_exception;
        auto color_event_stream_observable = make_color_event_stream_observable(
            std::move(event_stream),
            std::forward<HandleEvent>(handle_event),
            std::ref(capture_observable_exception),
            &false_function,
            event_stream_observable::dispatch::as_fast_as_possible,
            chunk_size);
        capture_observable_exception.wait();
        capture_observable_exception.rethrow_unless<end_of_file>();
    }

    /// forward-declare parameter for referencing in unvalidated_parameter.
    class parameter;

    /// forward-declare object_parameter and array_parameter for referencing in parameter.
    class object_parameter;
    class array_parameter;

    /// parameter corresponds to a setting or a group of settings.
    /// This class is used to validate the JSON parameters file, which is used to set the biases and other camera
    /// parameters.
    class parameter {
        public:
        parameter() = default;
        parameter(const parameter&) = delete;
        parameter(parameter&&) = default;
        parameter& operator=(const parameter&) = delete;
        parameter& operator=(parameter&&) = default;
        virtual ~parameter(){};

        /// get_array_parameter is used to retrieve a list parameter.
        /// It must be given a vector of strings which contains the parameter key (or keys for nested object
        /// parameters). An error is raised at runtime if the parameter is not a list.
        virtual const array_parameter& get_array_parameter(const std::vector<std::string>& keys = {}) const {
            return get_array_parameter(keys.begin(), keys.end());
        }

        /// get_boolean is used to retrieve a boolean parameter.
        /// It must be given a vector of strings which contains the parameter key (or keys for nested object
        /// parameters). An error is raised at runtime if the parameter is not a boolean.
        virtual bool get_boolean(const std::vector<std::string>& keys = {}) const {
            return get_boolean(keys.begin(), keys.end());
        }

        /// get_number is used to retrieve a numeric parameter.
        /// It must be given a vector of strings which contains the parameter key (or keys for nested object
        /// parameters). An error is raised at runtime if the parameter is not a number.
        virtual double get_number(const std::vector<std::string>& keys = {}) const {
            return get_number(keys.begin(), keys.end());
        }

        /// get_string is used to retrieve a string parameter.
        /// It must be given a vector of strings which contains the parameter key (or keys for nested object
        /// parameters). An error is raised at runtime if the parameter is not a string.
        virtual std::string get_string(const std::vector<std::string>& keys = {}) const {
            return get_string(keys.begin(), keys.end());
        }

        /// parse sets the parameter value from a JSON stream owned by the caller.
        /// The given data is validated in the process.
        /// The stream must be reset afterwards to be used again:
        ///     json_stream.clear();
        ///     json_stream.seekg(0, std::istream::beg);
        virtual void parse(std::istream& json_stream) {
            std::size_t character_count = 1;
            std::size_t line_count = 1;
            load(json_stream, character_count, line_count);
        }

        /// parse sets the parameter value from a JSON stream.
        /// The given data is validated in the process.
        virtual void parse(std::unique_ptr<std::istream> json_stream) {
            parse(*json_stream);
        }

        /// get_array_parameter is called by a parent parameter when accessing a list value.
        virtual const array_parameter& get_array_parameter(
            const std::vector<std::string>::const_iterator,
            const std::vector<std::string>::const_iterator) const {
            throw parameter_error("The parameter is not a list");
        }

        /// get_boolean is called by a parent parameter when accessing a boolean value.
        virtual bool get_boolean(
            const std::vector<std::string>::const_iterator,
            const std::vector<std::string>::const_iterator) const {
            throw parameter_error("The parameter is not a boolean");
        }

        /// get_number is called by a parent parameter when accessing a numeric value.
        virtual double get_number(
            const std::vector<std::string>::const_iterator,
            const std::vector<std::string>::const_iterator) const {
            throw parameter_error("The parameter is not a number");
        }

        /// get_string is called by a parent parameter when accessing a string value.
        virtual std::string get_string(
            const std::vector<std::string>::const_iterator,
            const std::vector<std::string>::const_iterator) const {
            throw parameter_error("The parameter is not a string");
        }

        /// clone generates a copy of the parameter.
        virtual std::unique_ptr<parameter> clone() const = 0;

        /// load is called by a parent parameter when loading JSON data.
        virtual void load(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) = 0;

        /// load sets the parameter value from an other parameter.
        /// The other parameter must a subset of this parameter, and is validated in the process.
        virtual void load(const parameter& parameter) = 0;

        protected:
        /// has_character determines wheter a character is in a string of characters.
        static constexpr bool has_character(const char* characters, const char character) {
            return *characters != '\0' && (*characters == character || has_character(characters + 1, character));
        }

        /// trim removes white space and line break characters.
        static void trim(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) {
            for (;;) {
                const auto character = json_stream.peek();
                if (character == '\n') {
                    json_stream.get();
                    ++line_count;
                    character_count = 1;
                } else if (std::isspace(character)) {
                    json_stream.get();
                    ++character_count;
                } else {
                    break;
                }
            }
        }

        /// string reads a string from a stream.
        static std::string string(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) {
            {
                const auto character = json_stream.get();
                if (character != '"') {
                    throw parse_error("the key does not start with quotes", character_count, line_count);
                }
                ++character_count;
            }
            auto escaped = false;
            std::string characters;
            for (;;) {
                const auto character = json_stream.get();
                if (json_stream.eof()) {
                    throw parse_error("unexpected end of file", character_count, line_count);
                }
                if (escaped) {
                    characters.push_back(character);
                    escaped = false;
                    if (character == '\n') {
                        character_count = 1;
                        ++line_count;
                    } else {
                        ++character_count;
                    }
                } else {
                    if (character == '\\') {
                        escaped = true;
                        ++character_count;
                    } else if (character == '"') {
                        ++character_count;
                        break;
                    } else if (has_character(control_characters, character)) {
                        throw parse_error("unexpected control character", character_count, line_count);
                    } else {
                        characters.push_back(character);
                        if (character == '\n') {
                            character_count = 1;
                            ++line_count;
                        } else {
                            ++character_count;
                        }
                    }
                }
            }
            return characters;
        }

        /// number reads a number from a stream.
        static double number(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) {
            std::string characters;
            uint8_t state = 0;
            auto done = false;
            for (;;) {
                const auto character = json_stream.peek();
                switch (state) {
                    case 0:
                        if (character == '-') {
                            state = 1;
                        } else if (character == '0') {
                            characters.push_back(character);
                            state = 2;
                        } else if (std::isdigit(character)) {
                            characters.push_back(character);
                            state = 3;
                        } else {
                            throw parse_error("unexpected character in a number", character_count, line_count);
                        }
                        break;
                    case 1:
                        if (character == '0') {
                            characters.push_back(character);
                            state = 2;
                        } else if (std::isdigit(character)) {
                            characters.push_back(character);
                            state = 3;
                        } else {
                            throw parse_error("unexpected character in a number", character_count, line_count);
                        }
                        break;
                    case 2:
                        if (character == '.') {
                            state = 4;
                        } else if (character == 'e' || character == 'E') {
                            characters.push_back(character);
                            state = 6;
                        } else {
                            done = true;
                        }
                        break;
                    case 3:
                        if (character == '.') {
                            state = 4;
                        } else if (character == 'e' || character == 'E') {
                            characters.push_back(character);
                            state = 6;
                        } else if (std::isdigit(character)) {
                            characters.push_back(character);
                        } else {
                            done = true;
                        }
                        break;
                    case 4:
                        if (std::isdigit(character)) {
                            characters.push_back(character);
                            state = 5;
                        } else {
                            throw parse_error("unexpected character in a number", character_count, line_count);
                        }
                        break;
                    case 5:
                        if (std::isdigit(character)) {
                            characters.push_back(character);
                        } else if (character == 'e' || character == 'E') {
                            characters.push_back(character);
                            state = 6;
                        } else {
                            done = true;
                        }
                        break;
                    case 6:
                        if (character == '+' || character == '-' || std::isdigit(character)) {
                            characters.push_back(character);
                            state = 7;
                        } else {
                            throw parse_error("unexpected character in a number", character_count, line_count);
                        }
                        break;
                    case 7:
                        if (std::isdigit(character)) {
                            characters.push_back(character);
                        } else {
                            done = true;
                        }
                        break;
                    default:
                        throw std::logic_error("unexpected state");
                }
                if (done) {
                    break;
                }
                json_stream.get();
            }
            return std::stod(characters);
        }

        static constexpr const char* control_characters = "/\b\f\n\r\t\0";
        static constexpr const char* separation_characters = ",}]\0";
    };

    /// object_parameter is a specialized parameter which contains other parameters.
    class object_parameter : public parameter {
        public:
        object_parameter() : parameter() {}
        template <typename String, typename Parameter, typename... Rest>
        object_parameter(String&& key, Parameter&& parameter, Rest&&... rest) :
            object_parameter(std::forward<Rest>(rest)...) {
            _parameter_by_key.insert(
                std::make_pair(std::forward<String>(key), std::move(std::forward<Parameter>(parameter))));
        }
        object_parameter(std::unordered_map<std::string, std::unique_ptr<parameter>> parameter_by_key) :
            object_parameter() {
            _parameter_by_key = std::move(parameter_by_key);
        }
        object_parameter(const object_parameter&) = delete;
        object_parameter(object_parameter&&) = default;
        object_parameter& operator=(const object_parameter&) = delete;
        object_parameter& operator=(object_parameter&&) = default;
        virtual ~object_parameter() {}
        const array_parameter& get_array_parameter(
            const std::vector<std::string>::const_iterator key,
            const std::vector<std::string>::const_iterator end) const override {
            if (key == end) {
                throw parameter_error("not enough keys");
            }
            return _parameter_by_key.at(*key)->get_array_parameter(key + 1, end);
        }
        bool get_boolean(
            const std::vector<std::string>::const_iterator key,
            const std::vector<std::string>::const_iterator end) const override {
            if (key == end) {
                throw parameter_error("not enough keys");
            }
            return _parameter_by_key.at(*key)->get_boolean(key + 1, end);
        }
        double get_number(
            const std::vector<std::string>::const_iterator key,
            const std::vector<std::string>::const_iterator end) const override {
            if (key == end) {
                throw parameter_error("not enough keys");
            }
            return _parameter_by_key.at(*key)->get_number(key + 1, end);
        }
        virtual std::string get_string(
            const std::vector<std::string>::const_iterator key,
            const std::vector<std::string>::const_iterator end) const override {
            if (key == end) {
                throw parameter_error("not enough keys");
            }
            return _parameter_by_key.at(*key)->get_string(key + 1, end);
        }
        virtual std::unique_ptr<parameter> clone() const override {
            std::unordered_map<std::string, std::unique_ptr<parameter>> new_parameter_by_key;
            for (const auto& key_and_parameter : _parameter_by_key) {
                new_parameter_by_key.insert(std::make_pair(key_and_parameter.first, key_and_parameter.second->clone()));
            }
            return sepia::make_unique<object_parameter>(std::move(new_parameter_by_key));
        }
        virtual void load(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) override {
            uint8_t state = 0;
            std::string key;
            auto done = false;
            while (!done) {
                trim(json_stream, character_count, line_count);
                switch (state) {
                    case 0:
                        if (json_stream.get() == '{') {
                            ++character_count;
                            state = 1;
                        } else {
                            throw parse_error("the object does not begin with a brace", character_count, line_count);
                        }
                        break;
                    case 1:
                        if (json_stream.peek() == '}') {
                            json_stream.get();
                            ++character_count;
                            done = true;
                        } else {
                            key = string(json_stream, character_count, line_count);
                            if (key.empty()) {
                                throw parse_error("the key is an empty string", character_count, line_count);
                            }
                            if (_parameter_by_key.find(key) == _parameter_by_key.end()) {
                                throw parse_error("unexpected key '" + key + "'", character_count, line_count);
                            }
                            state = 2;
                        }
                        break;
                    case 2:
                        if (json_stream.get() == ':') {
                            ++character_count;
                            state = 3;
                        } else {
                            throw parse_error("missing key separator ':'", character_count, line_count);
                        }
                        break;
                    case 3:
                        _parameter_by_key[key]->load(json_stream, character_count, line_count);
                        state = 4;
                        break;
                    case 4: {
                        const auto character = json_stream.get();
                        if (character == '}') {
                            ++character_count;
                            done = true;
                        } else if (character == ',') {
                            ++character_count;
                            state = 5;
                        } else {
                            throw parse_error("expected '}' or ','", character_count, line_count);
                        }
                        break;
                    }
                    case 5: {
                        key = string(json_stream, character_count, line_count);
                        if (key.empty()) {
                            throw parse_error("the key is an empty string", character_count, line_count);
                        }
                        if (_parameter_by_key.find(key) == _parameter_by_key.end()) {
                            throw parse_error("unexpected key '" + key + "'", character_count, line_count);
                        }
                        state = 2;
                        break;
                    }
                    default:
                        throw std::logic_error("unexpected state");
                }
            }
        }
        virtual void load(const parameter& parameter) override {
            try {
                const auto& casted_object_parameter = dynamic_cast<const object_parameter&>(parameter);
                for (const auto& key_and_parameter : casted_object_parameter) {
                    if (_parameter_by_key.find(key_and_parameter.first) == _parameter_by_key.end()) {
                        throw std::runtime_error("Unexpected key " + key_and_parameter.first);
                    }
                    _parameter_by_key[key_and_parameter.first]->load(*key_and_parameter.second);
                }
            } catch (const std::bad_cast& exception) {
                throw std::logic_error("Expected an object_parameter, got a " + std::string(typeid(parameter).name()));
            }
        }

        /// begin returns an iterator to the beginning of the contained parameters map.
        virtual std::unordered_map<std::string, std::unique_ptr<parameter>>::const_iterator begin() const {
            return _parameter_by_key.begin();
        }

        /// end returns an iterator to the end of the contained parameters map.
        virtual std::unordered_map<std::string, std::unique_ptr<parameter>>::const_iterator end() const {
            return _parameter_by_key.end();
        }

        protected:
        std::unordered_map<std::string, std::unique_ptr<parameter>> _parameter_by_key;
    };

    /// array_parameter is a specialized parameter which contains other parameters.
    class array_parameter : public parameter {
        public:
        /// make_empty generates an empty list parameter with the given value as template.
        static std::unique_ptr<array_parameter> make_empty(std::unique_ptr<parameter> template_parameter) {
            return sepia::make_unique<array_parameter>(
                std::vector<std::unique_ptr<parameter>>(), std::move(template_parameter));
        }

        template <typename Parameter>
        array_parameter(Parameter&& template_parameter) :
            parameter(),
            _template_parameter(template_parameter->clone()) {
            _parameters.push_back(std::move(template_parameter));
        }
        template <typename Parameter, typename... Rest>
        array_parameter(Parameter&& parameter, Rest&&... rest) : array_parameter(std::forward<Rest>(rest)...) {
            _parameters.insert(_parameters.begin(), std::move(std::forward<Parameter>(parameter)));
        }
        array_parameter(
            std::vector<std::unique_ptr<parameter>> parameters,
            std::unique_ptr<parameter> template_parameter) :
            parameter(),
            _parameters(std::move(parameters)),
            _template_parameter(std::move(template_parameter)) {}
        array_parameter(const array_parameter&) = delete;
        array_parameter(array_parameter&&) = default;
        array_parameter& operator=(const array_parameter&) = delete;
        array_parameter& operator=(array_parameter&&) = default;
        virtual ~array_parameter() {}
        virtual const array_parameter& get_array_parameter(
            const std::vector<std::string>::const_iterator key,
            const std::vector<std::string>::const_iterator end) const override {
            if (key != end) {
                throw parameter_error("too many keys");
            }
            return *this;
        }
        virtual std::unique_ptr<parameter> clone() const override {
            std::vector<std::unique_ptr<parameter>> new_parameters;
            for (const auto& parameter : _parameters) {
                new_parameters.push_back(parameter->clone());
            }
            return sepia::make_unique<array_parameter>(std::move(new_parameters), _template_parameter->clone());
        }
        virtual void load(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) override {
            _parameters.clear();
            uint8_t state = 0;
            std::string key;
            auto done = false;
            while (!done) {
                trim(json_stream, character_count, line_count);
                switch (state) {
                    case 0:
                        if (json_stream.get() == '[') {
                            ++character_count;
                            state = 1;
                        } else {
                            throw parse_error("the array does not begin with a bracket", character_count, line_count);
                        }
                        break;
                    case 1:
                        if (json_stream.peek() == ']') {
                            json_stream.get();
                            ++character_count;
                            done = true;
                        } else {
                            _parameters.push_back(_template_parameter->clone());
                            _parameters.back()->load(json_stream, character_count, line_count);
                            state = 2;
                        }
                        break;
                    case 2: {
                        const auto character = json_stream.get();
                        if (character == ']') {
                            ++character_count;
                            done = true;
                        } else if (character == ',') {
                            ++character_count;
                            state = 3;
                        } else {
                            throw parse_error("expected ']' or ','", character_count, line_count);
                        }
                        break;
                    }
                    case 3:
                        _parameters.push_back(_template_parameter->clone());
                        _parameters.back()->load(json_stream, character_count, line_count);
                        state = 2;
                        break;
                    default:
                        throw std::logic_error("unexpected state");
                }
            }
        }
        virtual void load(const parameter& parameter) override {
            try {
                const array_parameter& casted_array_parameter = dynamic_cast<const array_parameter&>(parameter);
                _parameters.clear();
                for (const auto& stored_parameter : casted_array_parameter) {
                    auto new_parameter = _template_parameter->clone();
                    new_parameter->load(*stored_parameter);
                    _parameters.push_back(std::move(new_parameter));
                }
            } catch (const std::bad_cast& exception) {
                throw std::logic_error("Expected an array_parameter, got a " + std::string(typeid(parameter).name()));
            }
        }

        /// size returns the list number of elements
        virtual std::size_t size() const {
            return _parameters.size();
        }

        /// begin returns an iterator to the beginning of the contained parameters vector.
        virtual std::vector<std::unique_ptr<parameter>>::const_iterator begin() const {
            return _parameters.begin();
        }

        /// end returns an iterator to the end of the contained parameters vector.
        virtual std::vector<std::unique_ptr<parameter>>::const_iterator end() const {
            return _parameters.end();
        }

        protected:
        std::vector<std::unique_ptr<parameter>> _parameters;
        std::unique_ptr<parameter> _template_parameter;
    };

    /// boolean_parameter is a specialized parameter for boolean values.
    class boolean_parameter : public parameter {
        public:
        boolean_parameter(bool value) : parameter(), _value(value) {}
        boolean_parameter(const boolean_parameter&) = delete;
        boolean_parameter(boolean_parameter&&) = default;
        boolean_parameter& operator=(const boolean_parameter&) = delete;
        boolean_parameter& operator=(boolean_parameter&&) = default;
        virtual ~boolean_parameter() {}
        virtual bool get_boolean(
            const std::vector<std::string>::const_iterator key,
            const std::vector<std::string>::const_iterator end) const override {
            if (key != end) {
                throw parameter_error("too many keys");
            }
            return _value;
        }
        virtual std::unique_ptr<parameter> clone() const override {
            return sepia::make_unique<boolean_parameter>(_value);
        }
        virtual void load(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) override {
            trim(json_stream, character_count, line_count);
            std::string characters;
            for (;;) {
                const auto character = json_stream.peek();
                if (std::isspace(character) || has_character(separation_characters, character) || json_stream.eof()) {
                    break;
                }
                characters.push_back(character);
                json_stream.get();
            }
            if (characters == "true") {
                _value = true;
            } else if (characters == "false") {
                _value = false;
            } else {
                throw parse_error("expected a boolean", character_count, line_count);
            }
        }
        virtual void load(const parameter& parameter) override {
            try {
                _value = parameter.get_boolean({});
            } catch (const parameter_error& exception) {
                throw std::logic_error("Expected a boolean_parameter, got a " + std::string(typeid(parameter).name()));
            }
        }

        protected:
        bool _value;
    };

    /// number_parameter is a specialized parameter for numeric values.
    class number_parameter : public parameter {
        public:
        number_parameter(double value, double minimum, double maximum, bool is_integer) :
            parameter(),
            _value(value),
            _minimum(minimum),
            _maximum(maximum),
            _is_integer(is_integer) {
            validate();
        }
        number_parameter(const number_parameter&) = delete;
        number_parameter(number_parameter&&) = default;
        number_parameter& operator=(const number_parameter&) = delete;
        number_parameter& operator=(number_parameter&&) = default;
        virtual ~number_parameter() {}
        virtual double get_number(
            const std::vector<std::string>::const_iterator key,
            const std::vector<std::string>::const_iterator end) const override {
            if (key != end) {
                throw parameter_error("too many keys");
            }
            return _value;
        }
        virtual std::unique_ptr<parameter> clone() const override {
            return sepia::make_unique<number_parameter>(_value, _minimum, _maximum, _is_integer);
        }
        virtual void load(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) override {
            trim(json_stream, character_count, line_count);
            _value = number(json_stream, character_count, line_count);
            try {
                validate();
            } catch (const parameter_error& exception) {
                throw parse_error(exception.what(), character_count, line_count);
            }
        }
        virtual void load(const parameter& parameter) override {
            try {
                _value = parameter.get_number();
            } catch (const parameter_error& exception) {
                throw std::logic_error("Expected a number_parameter, got a " + std::string(typeid(parameter).name()));
            }
            validate();
        }

        protected:
        /// validate determines if the number is valid regarding the given constraints.
        void validate() {
            if (std::isnan(_value)) {
                throw parameter_error("expected a number");
            }
            if (_value >= _maximum) {
                throw parameter_error(std::string("larger than maximum ") + std::to_string(_maximum));
            }
            if (_value < _minimum) {
                throw parameter_error(std::string("smaller than minimum ") + std::to_string(_minimum));
            }
            auto integer_part = 0.0;
            if (_is_integer && std::modf(_value, &integer_part) != 0.0) {
                throw parameter_error("expected an integer");
            }
        }

        double _value;
        double _minimum;
        double _maximum;
        bool _is_integer;
    };

    /// char_parameter is a specialized number parameter for char numeric values.
    class char_parameter : public number_parameter {
        public:
        char_parameter(double value) : number_parameter(value, 0, 256, true) {}
        char_parameter(const char_parameter&) = delete;
        char_parameter(char_parameter&&) = default;
        char_parameter& operator=(const char_parameter&) = delete;
        char_parameter& operator=(char_parameter&&) = default;
        virtual ~char_parameter() {}
        virtual std::unique_ptr<parameter> clone() const override {
            return sepia::make_unique<char_parameter>(_value);
        }
    };

    /// string_parameter is a specialized parameter for string values.
    class string_parameter : public parameter {
        public:
        string_parameter(const std::string& value) : parameter(), _value(value) {}
        string_parameter(const string_parameter&) = delete;
        string_parameter(string_parameter&&) = default;
        string_parameter& operator=(const string_parameter&) = delete;
        string_parameter& operator=(string_parameter&&) = default;
        virtual ~string_parameter() {}
        virtual std::string get_string(
            const std::vector<std::string>::const_iterator key,
            const std::vector<std::string>::const_iterator end) const override {
            if (key != end) {
                throw parameter_error("too many keys");
            }
            return _value;
        }
        virtual std::unique_ptr<parameter> clone() const override {
            return sepia::make_unique<string_parameter>(_value);
        }
        virtual void load(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) override {
            trim(json_stream, character_count, line_count);
            _value = string(json_stream, character_count, line_count);
        }
        virtual void load(const parameter& parameter) override {
            try {
                _value = parameter.get_string({});
            } catch (const parameter_error& exception) {
                throw std::logic_error("Expected a string_parameter, got a " + std::string(typeid(parameter).name()));
            }
        }

        protected:
        std::string _value;
    };

    /// enum_parameter is a specialized parameter for string values with a given set of possible values.
    class enum_parameter : public string_parameter {
        public:
        enum_parameter(const std::string& value, const std::unordered_set<std::string>& available_values) :
            string_parameter(value),
            _available_values(available_values) {
            if (_available_values.size() == 0) {
                throw parameter_error("an enum parameter needs at least one available value");
            }
            validate();
        }
        enum_parameter(const enum_parameter&) = delete;
        enum_parameter(enum_parameter&&) = default;
        enum_parameter& operator=(const enum_parameter&) = delete;
        enum_parameter& operator=(enum_parameter&&) = default;
        virtual ~enum_parameter() {}
        virtual std::unique_ptr<parameter> clone() const override {
            return sepia::make_unique<enum_parameter>(_value, _available_values);
        }
        virtual void load(std::istream& json_stream, std::size_t& character_count, std::size_t& line_count) override {
            trim(json_stream, character_count, line_count);
            _value = string(json_stream, character_count, line_count);
            try {
                validate();
            } catch (const parameter_error& exception) {
                throw parse_error(exception.what(), character_count, line_count);
            }
        }
        virtual void load(const parameter& parameter) override {
            try {
                _value = parameter.get_string({});
            } catch (const parameter_error& exception) {
                throw std::logic_error("Expected an enum_parameter, got a " + std::string(typeid(parameter).name()));
            }
            validate();
        }

        protected:
        /// validate determines if the enum is valid regarding the given constraints.
        void validate() {
            if (_available_values.find(_value) == _available_values.end()) {
                auto available_values_string = std::string("{");
                for (const auto& available_value : _available_values) {
                    if (available_values_string != "{") {
                        available_values_string += ", ";
                    }
                    available_values_string += available_value;
                }
                available_values_string += "}";
                throw parameter_error("The value " + _value + " should be one of " + available_values_string);
            }
        }

        std::unordered_set<std::string> _available_values;
    };

    /// unvalidated_parameter represents either a parameter subset or a JSON stream to be validated.
    /// It mimics a union behavior, with properly managed attributes' lifecycles.
    class unvalidated_parameter {
        public:
        unvalidated_parameter(std::unique_ptr<std::istream> json_stream) :
            _is_json_stream(true),
            _json_stream(std::move(json_stream)) {}
        unvalidated_parameter(std::unique_ptr<parameter> parameter) :
            _is_json_stream(false),
            _parameter(std::move(parameter)) {}
        unvalidated_parameter(const unvalidated_parameter&) = delete;
        unvalidated_parameter(unvalidated_parameter&&) = default;
        unvalidated_parameter& operator=(const unvalidated_parameter&) = delete;
        unvalidated_parameter& operator=(unvalidated_parameter&&) = default;
        virtual ~unvalidated_parameter() {}

        /// is_json_stream returns true if the object was constructed as a stream.
        virtual bool is_json_stream() const {
            return _is_json_stream;
        }

        /// to_json_stream returns the provided json_stream.
        /// An error is thrown if the object was constructed with a parameter.
        virtual std::istream& to_json_stream() {
            if (!_is_json_stream) {
                throw parameter_error("The unvalidated parameter is not a string");
            }
            return *_json_stream;
        }

        /// to_parameter returns the provided parameter.
        /// An error is thrown if the object was constructed with a stream.
        virtual const parameter& to_parameter() const {
            if (_is_json_stream) {
                throw parameter_error("The unvalidated parameter is not a parameter");
            }
            return *_parameter;
        }

        protected:
        bool _is_json_stream;
        std::unique_ptr<std::istream> _json_stream;
        std::unique_ptr<parameter> _parameter;
    };

    /// specialized_camera represents a template-specialized generic event-based camera.
    template <typename Event, typename HandleEvent, typename HandleException>
    class specialized_camera {
        public:
        specialized_camera(
            HandleEvent handle_event,
            HandleException handle_exception,
            std::size_t fifo_size,
            const std::chrono::milliseconds& sleep_duration) :
            _handle_event(std::forward<HandleEvent>(handle_event)),
            _handle_exception(std::forward<HandleException>(handle_exception)),
            _buffer_running(true),
            _sleep_duration(sleep_duration),
            _head(0),
            _tail(0),
            _events(fifo_size) {
            _buffer_loop = std::thread([this]() -> void {
                try {
                    Event event;
                    while (_buffer_running.load(std::memory_order_relaxed)) {
                        const auto current_head = _head.load(std::memory_order_relaxed);
                        if (current_head == _tail.load(std::memory_order_acquire)) {
                            std::this_thread::sleep_for(_sleep_duration);
                        } else {
                            event = _events[current_head];
                            _head.store((current_head + 1) % _events.size(), std::memory_order_release);
                            this->_handle_event(event);
                        }
                    }
                } catch (...) {
                    this->_handle_exception(std::current_exception());
                }
            });
        }
        specialized_camera(const specialized_camera&) = delete;
        specialized_camera(specialized_camera&&) = default;
        specialized_camera& operator=(const specialized_camera&) = delete;
        specialized_camera& operator=(specialized_camera&&) = default;
        virtual ~specialized_camera() {
            _buffer_running.store(false, std::memory_order_relaxed);
            _buffer_loop.join();
        }

        /// push adds an event to the circular FIFO in a thread-safe manner, as long as one thread only is writting.
        /// If the event could not be inserted (FIFO full), false is returned.
        virtual bool push(Event event) {
            const auto current_tail = _tail.load(std::memory_order_relaxed);
            const auto next_tail = (current_tail + 1) % _events.size();
            if (next_tail != _head.load(std::memory_order_acquire)) {
                _events[current_tail] = event;
                _tail.store(next_tail, std::memory_order_release);
                return true;
            }
            return false;
        }

        protected:
        HandleEvent _handle_event;
        HandleException _handle_exception;
        std::thread _buffer_loop;
        std::atomic_bool _buffer_running;
        const std::chrono::milliseconds _sleep_duration;
        std::atomic<std::size_t> _head;
        std::atomic<std::size_t> _tail;
        std::vector<Event> _events;
    };
}
