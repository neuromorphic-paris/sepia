#pragma once

#include <memory>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

/// sepia bundles functions and classes to represent a camera and handle its raw stream of events.
namespace sepia {

    /// make_unique creates a unique_ptr.
    template<typename T, typename ...Args>
    std::unique_ptr<T> make_unique(Args&& ...args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    /// Event represents the parameters of a change detection or an exposure measurement.
    struct Event {

        /// x represents the coordinate of the event on the sensor grid alongside the horizontal axis.
        /// x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the vertical axis.
        /// y is 0 on the bottom, and increases bottom to top.
        uint16_t y;

        /// timestamp represents the event's timestamp.
        uint64_t timestamp;

        /// isThresholdCrossing is false if the event is a change detection, and true if it is a threshold crossing.
        bool isThresholdCrossing;

        /// change detection: polarity is false if the light is decreasing.
        /// exposure measurement: polarity is false for a first threshold crossing.
        bool polarity;
    };

    /// ChangeDetection represents the parameters of a change detection.
    struct ChangeDetection {

        /// x represents the coordinate of the event on the sensor grid alongside the horizontal axis.
        /// x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the vertical axis.
        /// y is 0 on the bottom, and increases from bottom to top.
        uint16_t y;

        /// timestamp represents the event's timestamp.
        uint64_t timestamp;

        /// isIncrease is false if the light is decreasing.
        bool isIncrease;
    };

    /// ThresholdCrossing represent the parameters of a threshold crossing.
    struct ThresholdCrossing {

        /// x represents the coordinate of the event on the sensor grid alongside the horizontal axis.
        /// x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the vertical axis.
        /// y is 0 on the bottom, and increases from bottom to top.
        uint16_t y;

        /// timestamp represents the event's timestamp.
        uint64_t timestamp;

        /// isSecond is false if the event is a first threshold crossing.
        bool isSecond;
    };

    /// ColorEvent represents the parameters of a color event.
    struct ColorEvent {

        /// x represents the coordinate of the event on the sensor grid alongside the horizontal axis.
        /// x is 0 on the left, and increases from left to right.
        uint16_t x;

        /// y represents the coordinate of the event on the sensor grid alongside the vertical axis.
        /// y is 0 on the bottom, and increases bottom to top.
        uint16_t y;

        /// timestamp represents the event's timestamp.
        uint64_t timestamp;

        /// r represents the red component of the color.
        uint8_t r;

        /// g represents the green component of the color.
        uint8_t g;

        /// b represents the blue component of the color.
        uint8_t b;
    };

    /// UnreadableFile is thrown when an input file does not exist or is not readable.
    class UnreadableFile : public std::runtime_error {
        public:
            UnreadableFile(const std::string& filename) : std::runtime_error("The file '" + filename + "' could not be open for reading") {}
    };

    /// UnwritableFile is thrown whenan output file is not writable.
    class UnwritableFile : public std::runtime_error {
        public:
            UnwritableFile(const std::string& filename) : std::runtime_error("The file '" + filename + "'' could not be open for writing") {}
    };

    /// WrongSignature is thrown when an input file does not have the expected signature.
    class WrongSignature : public std::runtime_error {
        public:
            WrongSignature(const std::string& filename) : std::runtime_error("The file '" + filename + "' does not have the expected signature") {}
    };

    /// UnsupportedVersion is thrown when an Event Stream file uses an unsupported version.
    class UnsupportedVersion : public std::runtime_error {
        public:
            UnsupportedVersion(const std::string& filename) : std::runtime_error("The Event Stream file '" + filename + "' uses an unsupported version") {}
    };

    /// UnsupportedMode is thrown when an Event Stream file uses an unsupported mode.
    class UnsupportedMode : public std::runtime_error {
        public:
            UnsupportedMode(const std::string& filename) : std::runtime_error("The Event Stream file '" + filename + "' uses an unsupported mode") {}
    };

    /// EndOfFile is thrown when the end of an input file is reached.
    class EndOfFile : public std::runtime_error {
        public:
            EndOfFile() : std::runtime_error("End of file reached") {}
    };

    /// NoDeviceConnected is thrown when device auto-select is called without devices connected.
    class NoDeviceConnected : public std::runtime_error {
        public:
            NoDeviceConnected(const std::string& deviceFamily) : std::runtime_error("No " + deviceFamily + " is connected") {}
    };

    /// DeviceDisconnected is thrown when an active device is disonnected.
    class DeviceDisconnected : public std::runtime_error {
        public:
            DeviceDisconnected(const std::string& deviceName) : std::runtime_error(deviceName + " disconnected") {}
    };

    /// ParseError is thrown when a JSON parse error occurs.
    class ParseError : public std::runtime_error {
        public:
            ParseError(const std::string& what, uint32_t line) : std::runtime_error("JSON parse error: " + what + " (line " + std::to_string(line) + ")") {}
    };

    /// ParameterError is a logical error regarding a parameter.
    class ParameterError : public std::logic_error {
        public:
            ParameterError(const std::string& what) : std::logic_error(what) {}
    };

    /// Split separates a stream of events into a stream of change detections and a stream of theshold crossings.
    template <typename HandleChangeDetection, typename HandleThresholdCrossing>
    class Split {
        public:
            Split(HandleChangeDetection handleChangeDetection, HandleThresholdCrossing handleThresholdCrossing) :
                _handleChangeDetection(std::forward<HandleChangeDetection>(handleChangeDetection)),
                _handleThresholdCrossing(std::forward<HandleThresholdCrossing>(handleThresholdCrossing))
            {
            }
            Split(const Split&) = delete;
            Split(Split&&) = default;
            Split& operator=(const Split&) = delete;
            Split& operator=(Split&&) = default;
            virtual ~Split() {}

            /// operator() handles an event.
            virtual void operator()(Event event) {
                if (event.isThresholdCrossing) {
                    _handleThresholdCrossing(ThresholdCrossing{event.x, event.y, event.timestamp, event.polarity});
                } else {
                    _handleChangeDetection(ChangeDetection{event.x, event.y, event.timestamp, event.polarity});
                }
            }

        protected:
            HandleChangeDetection _handleChangeDetection;
            HandleThresholdCrossing _handleThresholdCrossing;
    };

    /// make_split creates a Split from functors.
    template <typename HandleChangeDetection, typename HandleThresholdCrossing>
    Split<HandleChangeDetection, HandleThresholdCrossing> make_split(
        HandleChangeDetection handleChangeDetection,
        HandleThresholdCrossing handleThresholdCrossing
    ) {
        return Split<HandleChangeDetection, HandleThresholdCrossing>(
            std::forward<HandleChangeDetection>(handleChangeDetection),
            std::forward<HandleThresholdCrossing>(handleThresholdCrossing)
        );
    }

    /// EventStreamWriter writes events to an Event Stream file.
    class EventStreamWriter {
        public:
            EventStreamWriter():
                _logging(false),
                _previousTimestamp(0)
            {
                _accessingEventStream.clear(std::memory_order_release);
            }
            EventStreamWriter(const EventStreamWriter&) = delete;
            EventStreamWriter(EventStreamWriter&&) = default;
            EventStreamWriter& operator=(const EventStreamWriter&) = delete;
            EventStreamWriter& operator=(EventStreamWriter&&) = default;
            virtual ~EventStreamWriter() {}

            /// operator() handles an event.
            virtual void operator()(Event event) {
                while (_accessingEventStream.test_and_set(std::memory_order_acquire)) {}
                if (_logging) {
                    auto relativeTimestamp = event.timestamp - _previousTimestamp;
                    if (relativeTimestamp > 30) {
                        const auto numberOfOverflows = relativeTimestamp / 31;
                        for (auto index = static_cast<std::size_t>(0); index < numberOfOverflows / 8; ++index) {
                            _eventStream.put(static_cast<uint8_t>(0b11111111));
                        }
                        const auto numberOfOverflowsLeft = numberOfOverflows % 8;
                        if (numberOfOverflowsLeft > 0) {
                            _eventStream.put(static_cast<uint8_t>(0b11111) | static_cast<uint8_t>(numberOfOverflowsLeft << 5));
                        }
                        relativeTimestamp -= numberOfOverflows * 31;
                    }
                    _eventStream.put(static_cast<uint8_t>(relativeTimestamp) | static_cast<uint8_t>((event.x & 0b111) << 5));
                    _eventStream.put(static_cast<uint8_t>((event.x & 0b111111000) >> 3) | static_cast<uint8_t>((event.y & 0b11) << 6));
                    _eventStream.put(
                        static_cast<uint8_t>((event.y & 0b11111100) >> 2)
                        | static_cast<uint8_t>(event.isThresholdCrossing ? 0b1000000 : 0)
                        | static_cast<uint8_t>(event.polarity ? 0b10000000 : 0)
                    );
                    _previousTimestamp = event.timestamp;
                }
                _accessingEventStream.clear(std::memory_order_release);
            }

            /// open is a thread-safe method to start logging events to the given file.
            virtual void open(const std::string& filename) {
                _eventStream.open(filename, std::ifstream::binary);
                if (!_eventStream.good()) {
                    throw UnwritableFile(filename);
                }
                while (_accessingEventStream.test_and_set(std::memory_order_acquire)) {}
                if (_logging) {
                    _accessingEventStream.clear(std::memory_order_release);
                    throw std::runtime_error("Already logging");
                }
                _eventStream.write("Event Stream010", 15);
                _logging = true;
                _accessingEventStream.clear(std::memory_order_release);
            }

            /// close is a thread-safe method to stop logging the events and close the file.
            virtual void close() {
                while (_accessingEventStream.test_and_set(std::memory_order_acquire)) {}
                if (!_logging) {
                    _accessingEventStream.clear(std::memory_order_release);
                    throw std::runtime_error("Was not logging");
                }
                _logging = false;
                _eventStream.close();
                _previousTimestamp = 0;
                _accessingEventStream.clear(std::memory_order_release);
            }

        protected:
            std::ofstream _eventStream;
            bool _logging;
            std::atomic_flag _accessingEventStream;
            uint64_t _previousTimestamp;
    };


    /// EventStreamStateMachine implements the event stream state machine for use in more high-level components.
    template <typename HandleEvent>
    class EventStreamStateMachine {
        public:
            EventStreamStateMachine(HandleEvent handleEvent) :
                _handleEvent(std::forward<HandleEvent>(handleEvent)),
                _state(State::idle),
                _event(Event{0, 0, 0, false, false})
            {
            }
            EventStreamStateMachine(const EventStreamStateMachine&) = default;
            EventStreamStateMachine(EventStreamStateMachine&&) = default;
            EventStreamStateMachine& operator=(const EventStreamStateMachine&) = default;
            EventStreamStateMachine& operator=(EventStreamStateMachine&&) = default;
            virtual ~EventStreamStateMachine() {}

            /// operator() handles a byte.
            virtual void operator()(uint8_t byte) {
                switch (_state) {
                    case State::idle: {
                        if ((byte & 0b11111) == 0b11111) {
                            _event.timestamp += ((byte & 0b11100000) >> 5) * 31;
                        } else {
                            _event.timestamp = _event.timestamp + (byte & 0b11111);
                            _event.x = ((byte & 0b11100000) >> 5);
                            _state = State::byte0;
                        }
                        break;
                    }
                    case State::byte0: {
                        _event.x |= (static_cast<uint16_t>(byte & 0b111111) << 3);
                        _event.y = ((byte & 0b11000000) >> 6);
                        _state = State::byte1;
                        break;
                    }
                    case State::byte1: {
                        _event.y |= (static_cast<uint16_t>(byte & 0b111111) << 2);
                        _event.isThresholdCrossing = (((byte & 0b1000000) >> 6) == 1);
                        _event.polarity = (((byte & 0b10000000) >> 7) == 1);
                        _handleEvent(_event);
                        _state = State::idle;
                        break;
                    }
                }
            }

            /// reset initialises the state machine.
            virtual void reset() {
                _state = State::idle;
                _event.timestamp = 0;
            }

        protected:

            /// State represents the current state machine's state.
            enum class State {
                idle,
                byte0,
                byte1,
            };

            HandleEvent _handleEvent;
            State _state;
            Event _event;
    };

    /// make_eventStreamStateMachine creates an event stream state machine from functors.
    template<typename HandleEvent>
    EventStreamStateMachine<HandleEvent> make_eventStreamStateMachine(HandleEvent handleEvent) {
        return EventStreamStateMachine<HandleEvent>(std::forward<HandleEvent>(handleEvent));
    }

    /// EventStreamObservable dispatches events from an event stream file.
    class EventStreamObservable {
        public:

            /// Dispatch specifies when the events are dispatched.
            enum class Dispatch {
                synchronouslyAndSkipOffset,
                synchronously,
                asFastAsPossible,
            };

            EventStreamObservable() {}
            EventStreamObservable(const EventStreamObservable&) = delete;
            EventStreamObservable(EventStreamObservable&&) = default;
            EventStreamObservable& operator=(const EventStreamObservable&) = delete;
            EventStreamObservable& operator=(EventStreamObservable&&) = default;
            virtual ~EventStreamObservable() {}
    };

    /// SpecialisedEventStreamObservable represents a template-specialised Event Stream observable.
    template <typename HandleEvent, typename HandleException>
    class SpecialisedEventStreamObservable : public EventStreamObservable {
        public:
            SpecialisedEventStreamObservable(
                const std::string& filename,
                HandleEvent handleEvent,
                HandleException handleException,
                EventStreamObservable::Dispatch dispatch,
                std::function<bool()> mustRestart,
                std::size_t chunkSize
            ) :
                _handleEvent(std::forward<HandleEvent>(handleEvent)),
                _handleException(std::forward<HandleException>(handleException)),
                _eventStream(filename, std::ifstream::binary),
                _running(true),
                _mustRestart(std::move(mustRestart))
            {
                if (!_eventStream.good()) {
                    throw UnreadableFile(filename);
                }
                {
                    const auto readSignature = std::string("Event Stream");
                    _eventStream.read(const_cast<char*>(readSignature.data()), readSignature.length());
                    const auto versionMajor = _eventStream.get();
                    const auto versionMinor = _eventStream.get();
                    if (_eventStream.eof() || readSignature != "Event Stream") {
                        throw WrongSignature(filename);
                    }
                    if (versionMajor != '0' || versionMinor != '1') {
                        throw UnsupportedVersion(filename);
                    }
                    const auto mode = _eventStream.get();
                    if (_eventStream.eof() || mode != '0') {
                        throw UnsupportedMode(filename);
                    }
                }
                _loop = std::thread([this, dispatch, chunkSize]() -> void {
                    try {
                        auto bytes = std::vector<uint8_t>(chunkSize);
                        switch (dispatch) {
                            case EventStreamObservable::Dispatch::synchronouslyAndSkipOffset: {
                                auto offsetSkipped = false;
                                auto timeReference = std::chrono::system_clock::now();
                                auto initialTimestamp = static_cast<uint64_t>(0);
                                auto previousTimestamp = static_cast<uint64_t>(0);
                                auto eventStreamStateMachine = make_eventStreamStateMachine(
                                    [this, &offsetSkipped, &timeReference, &initialTimestamp, &previousTimestamp](Event event) -> void {
                                        if (offsetSkipped && event.timestamp > previousTimestamp) {
                                            std::this_thread::sleep_until(timeReference + std::chrono::microseconds(event.timestamp - initialTimestamp));
                                        } else {
                                            offsetSkipped = true;
                                            initialTimestamp = event.timestamp;
                                        }
                                        previousTimestamp = event.timestamp;
                                        this->_handleEvent(event);
                                    }
                                );
                                while (_running.load(std::memory_order_relaxed)) {
                                    _eventStream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                    if (_eventStream.eof()) {
                                        for (auto byteIterator = bytes.begin(); byteIterator != std::next(bytes.begin(), _eventStream.gcount()); ++byteIterator) {
                                            eventStreamStateMachine(*byteIterator);
                                        }
                                        if (_mustRestart()) {
                                            _eventStream.clear();
                                            _eventStream.seekg(15);
                                            offsetSkipped = false;
                                            eventStreamStateMachine.reset();
                                            timeReference = std::chrono::system_clock::now();
                                            continue;
                                        }
                                        throw EndOfFile();
                                    }
                                    for (auto&& byte : bytes) {
                                        eventStreamStateMachine(byte);
                                    }
                                }
                            }
                            case EventStreamObservable::Dispatch::synchronously: {
                                auto timeReference = std::chrono::system_clock::now();
                                auto previousTimestamp = static_cast<uint64_t>(0);
                                auto eventStreamStateMachine = make_eventStreamStateMachine(
                                    [this, &timeReference, &previousTimestamp](Event event) -> void {
                                        if (event.timestamp > previousTimestamp) {
                                            std::this_thread::sleep_until(timeReference + std::chrono::microseconds(event.timestamp));
                                        }
                                        previousTimestamp = event.timestamp;
                                        this->_handleEvent(event);
                                    }
                                );
                                while (_running.load(std::memory_order_relaxed)) {
                                    _eventStream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                    if (_eventStream.eof()) {
                                        for (auto byteIterator = bytes.begin(); byteIterator != std::next(bytes.begin(), _eventStream.gcount()); ++byteIterator) {
                                            eventStreamStateMachine(*byteIterator);
                                        }
                                        if (_mustRestart()) {
                                            _eventStream.clear();
                                            _eventStream.seekg(15);
                                            eventStreamStateMachine.reset();
                                            timeReference = std::chrono::system_clock::now();
                                            continue;
                                        }
                                        throw EndOfFile();
                                    }
                                    for (auto&& byte : bytes) {
                                        eventStreamStateMachine(byte);
                                    }
                                }
                            }
                            case EventStreamObservable::Dispatch::asFastAsPossible: {
                                auto eventStreamStateMachine = make_eventStreamStateMachine(
                                    [this](Event event) -> void {
                                        this->_handleEvent(event);
                                    }
                                );
                                while (_running.load(std::memory_order_relaxed)) {
                                    _eventStream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                    if (_eventStream.eof()) {
                                        for (auto byteIterator = bytes.begin(); byteIterator != std::next(bytes.begin(), _eventStream.gcount()); ++byteIterator) {
                                            eventStreamStateMachine(*byteIterator);
                                        }
                                        if (_mustRestart()) {
                                            _eventStream.clear();
                                            _eventStream.seekg(15);
                                            eventStreamStateMachine.reset();
                                            continue;
                                        }
                                        throw EndOfFile();
                                    }
                                    for (auto&& byte : bytes) {
                                        eventStreamStateMachine(byte);
                                    }
                                }
                            }
                        }
                    } catch (...) {
                        this->_handleException(std::current_exception());
                    }
                });
            }
            SpecialisedEventStreamObservable(const SpecialisedEventStreamObservable&) = delete;
            SpecialisedEventStreamObservable(SpecialisedEventStreamObservable&&) = default;
            SpecialisedEventStreamObservable& operator=(const SpecialisedEventStreamObservable&) = delete;
            SpecialisedEventStreamObservable& operator=(SpecialisedEventStreamObservable&&) = default;
            virtual ~SpecialisedEventStreamObservable() {
                _running.store(false, std::memory_order_relaxed);
                _loop.join();
            }

        protected:
            HandleEvent _handleEvent;
            HandleException _handleException;
            std::ifstream _eventStream;
            std::atomic_bool _running;
            std::thread _loop;
            std::function<bool()> _mustRestart;
    };

    /// make_eventStreamObservable creates an event stream observable from functors.
    template<typename HandleEvent, typename HandleException>
    std::unique_ptr<SpecialisedEventStreamObservable<HandleEvent, HandleException>> make_eventStreamObservable(
        const std::string& filename,
        HandleEvent handleEvent,
        HandleException handleException,
        EventStreamObservable::Dispatch dispatch = EventStreamObservable::Dispatch::synchronouslyAndSkipOffset,
        std::function<bool()> mustRestart = []() -> bool {
            return false;
        },
        std::size_t chunkSize = 1 << 10
    ) {
        return sepia::make_unique<SpecialisedEventStreamObservable<HandleEvent, HandleException>>(
            filename,
            std::forward<HandleEvent>(handleEvent),
            std::forward<HandleException>(handleException),
            dispatch,
            std::move(mustRestart),
            chunkSize
        );
    }

    /// ColorEventStreamWriter writes events to a color Event Stream file.
    class ColorEventStreamWriter {
        public:
            ColorEventStreamWriter():
                _logging(false),
                _previousTimestamp(0)
            {
                _accessingEventStream.clear(std::memory_order_release);
            }
            ColorEventStreamWriter(const ColorEventStreamWriter&) = delete;
            ColorEventStreamWriter(ColorEventStreamWriter&&) = default;
            ColorEventStreamWriter& operator=(const ColorEventStreamWriter&) = delete;
            ColorEventStreamWriter& operator=(ColorEventStreamWriter&&) = default;
            virtual ~ColorEventStreamWriter() {}

            /// operator() handles an event.
            virtual void operator()(ColorEvent colorEvent) {
                while (_accessingEventStream.test_and_set(std::memory_order_acquire)) {}
                if (_logging) {
                    auto relativeTimestamp = colorEvent.timestamp - _previousTimestamp;
                    if (relativeTimestamp > 126) {
                        const auto numberOfOverflows = relativeTimestamp / 127;
                        for (auto index = static_cast<std::size_t>(0); index < numberOfOverflows; ++index) {
                            _eventStream.put(static_cast<uint8_t>(0b11111111));
                        }
                        relativeTimestamp -= numberOfOverflows * 127;
                    }
                    _eventStream.put(static_cast<uint8_t>(relativeTimestamp) | static_cast<uint8_t>((colorEvent.x & 0b1) << 7));
                    _eventStream.put(static_cast<uint8_t>((colorEvent.x & 0b111111110) >> 1));
                    _eventStream.put(static_cast<uint8_t>(colorEvent.y));
                    _eventStream.put(colorEvent.r);
                    _eventStream.put(colorEvent.g);
                    _eventStream.put(colorEvent.b);
                    _previousTimestamp = colorEvent.timestamp;
                }
                _accessingEventStream.clear(std::memory_order_release);
            }

            /// open is a thread-safe method to start logging events to the given file.
            virtual void open(const std::string& filename) {
                _eventStream.open(filename, std::ifstream::binary);
                if (!_eventStream.good()) {
                    throw UnwritableFile(filename);
                }
                while (_accessingEventStream.test_and_set(std::memory_order_acquire)) {}
                if (_logging) {
                    _accessingEventStream.clear(std::memory_order_release);
                    throw std::runtime_error("Already logging");
                }
                _eventStream.write("Event Stream012", 15);
                _logging = true;
                _accessingEventStream.clear(std::memory_order_release);
            }

            /// close is a thread-safe method to stop logging the events and close the file.
            virtual void close() {
                while (_accessingEventStream.test_and_set(std::memory_order_acquire)) {}
                if (!_logging) {
                    _accessingEventStream.clear(std::memory_order_release);
                    throw std::runtime_error("Was not logging");
                }
                _logging = false;
                _eventStream.close();
                _previousTimestamp = 0;
                _accessingEventStream.clear(std::memory_order_release);
            }

        protected:
            std::ofstream _eventStream;
            bool _logging;
            std::atomic_flag _accessingEventStream;
            uint64_t _previousTimestamp;
    };

    /// ColorEventStreamStateMachine implements the event stream state machine for use in more high-level components.
    template <typename HandleEvent>
    class ColorEventStreamStateMachine {
        public:
            ColorEventStreamStateMachine(HandleEvent handleEvent) :
                _handleEvent(std::forward<HandleEvent>(handleEvent)),
                _state(State::idle),
                _colorEvent(ColorEvent{0, 0, 0, 0, 0, 0})
            {
            }
            ColorEventStreamStateMachine(const ColorEventStreamStateMachine&) = default;
            ColorEventStreamStateMachine(ColorEventStreamStateMachine&&) = default;
            ColorEventStreamStateMachine& operator=(const ColorEventStreamStateMachine&) = default;
            ColorEventStreamStateMachine& operator=(ColorEventStreamStateMachine&&) = default;
            virtual ~ColorEventStreamStateMachine() {}

            /// operator() handles a byte.
            virtual void operator()(uint8_t byte) {
                switch (_state) {
                    case State::idle: {
                        if ((byte & 0b1111111) == 0b1111111) {
                            _colorEvent.timestamp += ((byte & 0b10000000) >> 7) * 127;
                        } else {
                            _colorEvent.timestamp = _colorEvent.timestamp+ (byte & 0b1111111);
                            _colorEvent.x = ((byte & 0b10000000) >> 7);
                            _state = State::byte0;
                        }
                        break;
                    }
                    case State::byte0: {
                        _colorEvent.x |= (static_cast<uint16_t>(byte) << 1);
                        _state = State::byte1;
                        break;
                    }
                    case State::byte1: {
                        _colorEvent.y = byte;
                        _state = State::byte2;
                        break;
                    }
                    case State::byte2: {
                        _colorEvent.r = byte;
                        _state = State::byte3;
                        break;
                    }
                    case State::byte3: {
                        _colorEvent.g = byte;
                        _state = State::byte4;
                        break;
                    }
                    case State::byte4: {
                        _colorEvent.b = byte;
                        _handleEvent(_colorEvent);
                        _state = State::idle;
                        break;
                    }
                }
            }

            /// reset initialises the state machine.
            virtual void reset() {
                _colorEvent.timestamp = 0;
                _state = State::idle;
            }

        protected:

            /// State represents the current state machine's state.
            enum class State {
                idle,
                byte0,
                byte1,
                byte2,
                byte3,
                byte4,
            };

            HandleEvent _handleEvent;
            State _state;
            ColorEvent _colorEvent;
    };

    /// make_colorEventStreamStateMachine creates a color event stream state machine from functors.
    template<typename HandleEvent>
    ColorEventStreamStateMachine<HandleEvent> make_colorEventStreamStateMachine(HandleEvent handleEvent) {
        return ColorEventStreamStateMachine<HandleEvent>(std::forward<HandleEvent>(handleEvent));
    }

    /// SpecialisedColorEventStreamObservable represents a template-specialised Event Stream observable.
    template <typename HandleEvent, typename HandleException>
    class SpecialisedColorEventStreamObservable : public EventStreamObservable {
        public:
            SpecialisedColorEventStreamObservable(
                const std::string& filename,
                HandleEvent handleEvent,
                HandleException handleException,
                EventStreamObservable::Dispatch dispatch,
                std::function<bool()> mustRestart,
                std::size_t chunkSize
            ) :
                _handleEvent(std::forward<HandleEvent>(handleEvent)),
                _handleException(std::forward<HandleException>(handleException)),
                _eventStream(filename, std::ifstream::binary),
                _running(true),
                _mustRestart(std::move(mustRestart))
            {
                if (!_eventStream.good()) {
                    throw UnreadableFile(filename);
                }
                {
                    const auto readSignature = std::string("Event Stream");
                    _eventStream.read(const_cast<char*>(readSignature.data()), readSignature.length());
                    const auto versionMajor = _eventStream.get();
                    const auto versionMinor = _eventStream.get();
                    if (_eventStream.eof() || readSignature != "Event Stream") {
                        throw WrongSignature(filename);
                    }
                    if (versionMajor != '0' || versionMinor != '1') {
                        throw UnsupportedVersion(filename);
                    }
                    const auto mode = _eventStream.get();
                    if (_eventStream.eof() || mode != '2') {
                        throw UnsupportedMode(filename);
                    }
                }
                _loop = std::thread([this, dispatch, chunkSize]() -> void {
                    try {
                        auto bytes = std::vector<uint8_t>(chunkSize);
                        switch (dispatch) {
                            case EventStreamObservable::Dispatch::synchronouslyAndSkipOffset: {
                                auto offsetSkipped = false;
                                auto timeReference = std::chrono::system_clock::now();
                                auto initialTimestamp = static_cast<uint64_t>(0);
                                auto colorEventStreamStateMachine = make_colorEventStreamStateMachine(
                                    [this, &offsetSkipped, &timeReference, &initialTimestamp](ColorEvent colorEvent) -> void {
                                        if (offsetSkipped) {
                                            std::this_thread::sleep_until(timeReference + std::chrono::microseconds(colorEvent.timestamp - initialTimestamp));
                                        } else {
                                            offsetSkipped = true;
                                            initialTimestamp = colorEvent.timestamp;
                                        }
                                        this->_handleEvent(colorEvent);
                                    }
                                );
                                while (_running.load(std::memory_order_relaxed)) {
                                    _eventStream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                    if (_eventStream.eof()) {
                                        for (auto byteIterator = bytes.begin(); byteIterator != std::next(bytes.begin(), _eventStream.gcount()); ++byteIterator) {
                                            colorEventStreamStateMachine(*byteIterator);
                                        }
                                        if (_mustRestart()) {
                                            _eventStream.clear();
                                            _eventStream.seekg(15);
                                            offsetSkipped = false;
                                            colorEventStreamStateMachine.reset();
                                            timeReference = std::chrono::system_clock::now();
                                            continue;
                                        }
                                        throw EndOfFile();
                                    }
                                    for (auto&& byte : bytes) {
                                        colorEventStreamStateMachine(byte);
                                    }
                                }
                            }
                            case EventStreamObservable::Dispatch::synchronously: {
                                auto timeReference = std::chrono::system_clock::now();
                                auto colorEventStreamStateMachine = make_colorEventStreamStateMachine(
                                    [this, &timeReference](ColorEvent colorEvent) -> void {
                                        std::this_thread::sleep_until(timeReference + std::chrono::microseconds(colorEvent.timestamp));
                                        this->_handleEvent(colorEvent);
                                    }
                                );
                                while (_running.load(std::memory_order_relaxed)) {
                                    _eventStream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                    if (_eventStream.eof()) {
                                        for (auto byteIterator = bytes.begin(); byteIterator != std::next(bytes.begin(), _eventStream.gcount()); ++byteIterator) {
                                            colorEventStreamStateMachine(*byteIterator);
                                        }
                                        if (_mustRestart()) {
                                            _eventStream.clear();
                                            _eventStream.seekg(15);
                                            colorEventStreamStateMachine.reset();
                                            timeReference = std::chrono::system_clock::now();
                                            continue;
                                        }
                                        throw EndOfFile();
                                    }
                                    for (auto&& byte : bytes) {
                                        colorEventStreamStateMachine(byte);
                                    }
                                }
                            }
                            case EventStreamObservable::Dispatch::asFastAsPossible: {
                                auto colorEventStreamStateMachine = make_colorEventStreamStateMachine(
                                    [this](ColorEvent colorEvent) -> void {
                                        this->_handleEvent(colorEvent);
                                    }
                                );
                                while (_running.load(std::memory_order_relaxed)) {
                                    _eventStream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                    if (_eventStream.eof()) {
                                        for (auto byteIterator = bytes.begin(); byteIterator != std::next(bytes.begin(), _eventStream.gcount()); ++byteIterator) {
                                            colorEventStreamStateMachine(*byteIterator);
                                        }
                                        if (_mustRestart()) {
                                            _eventStream.clear();
                                            _eventStream.seekg(15);
                                            colorEventStreamStateMachine.reset();
                                            continue;
                                        }
                                        throw EndOfFile();
                                    }
                                    for (auto&& byte : bytes) {
                                        colorEventStreamStateMachine(byte);
                                    }
                                }
                            }
                        }
                    } catch (...) {
                        this->_handleException(std::current_exception());
                    }
                });
            }
            SpecialisedColorEventStreamObservable(const SpecialisedColorEventStreamObservable&) = delete;
            SpecialisedColorEventStreamObservable(SpecialisedColorEventStreamObservable&&) = default;
            SpecialisedColorEventStreamObservable& operator=(const SpecialisedColorEventStreamObservable&) = delete;
            SpecialisedColorEventStreamObservable& operator=(SpecialisedColorEventStreamObservable&&) = default;
            virtual ~SpecialisedColorEventStreamObservable() {
                _running.store(false, std::memory_order_relaxed);
                _loop.join();
            }

        protected:
            HandleEvent _handleEvent;
            HandleException _handleException;
            std::ifstream _eventStream;
            std::atomic_bool _running;
            std::thread _loop;
            std::function<bool()> _mustRestart;
    };

    /// make_colorEventStreamObservable creates a color event stream observable from functors.
    template<typename HandleEvent, typename HandleException>
    std::unique_ptr<SpecialisedColorEventStreamObservable<HandleEvent, HandleException>> make_colorEventStreamObservable(
        const std::string& filename,
        HandleEvent handleEvent,
        HandleException handleException,
        EventStreamObservable::Dispatch dispatch = EventStreamObservable::Dispatch::synchronouslyAndSkipOffset,
        std::function<bool()> mustRestart = []() -> bool {
            return false;
        },
        std::size_t chunkSize = 1 << 10
    ) {
        return sepia::make_unique<SpecialisedColorEventStreamObservable<HandleEvent, HandleException>>(
            filename,
            std::forward<HandleEvent>(handleEvent),
            std::forward<HandleException>(handleException),
            dispatch,
            std::move(mustRestart),
            chunkSize
        );
    }

    /// Forward-declare parameter for referencing in UnvalidatedParameter.
    class Parameter;

    /// Forward-declare ObjectParameter and ListParameter for referencing in Parameter.
    class ObjectParameter;
    class ListParameter;

    /// Parameter corresponds to a setting or a group of settings.
    /// This class is used to validate the JSON parameters file, which is used to set the biases and other camera parameters.
    class Parameter {
        public:
            Parameter() = default;
            Parameter(const Parameter&) = delete;
            Parameter(Parameter&&) = default;
            Parameter& operator=(const Parameter&) = delete;
            Parameter& operator=(Parameter&&) = default;
            virtual ~Parameter() {};

            /// getListParameter is used to retrieve a list parameter.
            /// It must be given a vector of strings which contains the parameter key (or keys for nested object parameters).
            /// An error is raised at runtime if the parameter is not a list.
            virtual const ListParameter& getListParameter(const std::vector<std::string>& keys) const {
                return getListParameter(keys.begin(), keys.end());
            }

            /// getBoolean is used to retrieve a boolean parameter.
            /// It must be given a vector of strings which contains the parameter key (or keys for nested object parameters).
            /// An error is raised at runtime if the parameter is not a boolean.
            virtual bool getBoolean(const std::vector<std::string>& keys) const {
                return getBoolean(keys.begin(), keys.end());
            }

            /// getNumber is used to retrieve a numeric parameter.
            /// It must be given a vector of strings which contains the parameter key (or keys for nested object parameters).
            /// An error is raised at runtime if the parameter is not a number.
            virtual double getNumber(const std::vector<std::string>& keys) const {
                return getNumber(keys.begin(), keys.end());
            }

            /// getString is used to retrieve a string parameter.
            /// It must be given a vector of strings which contains the parameter key (or keys for nested object parameters).
            /// An error is raised at runtime if the parameter is not a string.
            virtual std::string getString(const std::vector<std::string>& keys) const {
                return getString(keys.begin(), keys.end());
            }

            /// load sets the parameter value from a string which contains JSON data.
            /// The given data is validated in the process.
            virtual void load(const std::string& jsonData) {
                if (jsonData != std::string()) {
                    uint32_t line = 1;
                    load(jsonData.begin(), jsonData.end(), line);
                }
            }

            /// getListParameter is called by a parent parameter when accessing a list value.
            virtual const ListParameter& getListParameter(
                const std::vector<std::string>::const_iterator,
                const std::vector<std::string>::const_iterator
            ) const {
                throw ParameterError("The parameter is not a list");
            }

            /// getBoolean is called by a parent parameter when accessing a boolean value.
            virtual bool getBoolean(const std::vector<std::string>::const_iterator, const std::vector<std::string>::const_iterator) const {
                throw ParameterError("The parameter is not a boolean");
            }

            /// getNumber is called by a parent parameter when accessing a numeric value.
            virtual double getNumber(const std::vector<std::string>::const_iterator, const std::vector<std::string>::const_iterator) const {
                throw ParameterError("The parameter is not a number");
            }

            /// getString is called by a parent parameter when accessing a string value.
            virtual std::string getString(const std::vector<std::string>::const_iterator, const std::vector<std::string>::const_iterator) const {
                throw ParameterError("The parameter is not a string");
            }

            /// clone generates a copy of the parameter.
            virtual std::unique_ptr<Parameter> clone() const = 0;

            /// load is called by a parent parameter when loading JSON data.
            virtual std::string::const_iterator load(std::string::const_iterator begin, std::string::const_iterator fileEnd, uint32_t& line) = 0;

            /// load sets the parameter value from an other parameter.
            /// The other parameter must a subset of this parameter, and is validated in the process.
            virtual void load(const Parameter& parameter) = 0;

        protected:

            /// trim removes white space and line break characters.
            static const std::string::const_iterator trim(std::string::const_iterator begin, std::string::const_iterator fileEnd, uint32_t& lineCount) {
                auto trimIterator = begin;
                for (; trimIterator != fileEnd && hasCharacter(whitespaceCharacters, *trimIterator); ++trimIterator) {
                    if (*trimIterator == '\n') {
                        ++lineCount;
                    }
                }
                return trimIterator;
            }

            /// hasCharacter determines wheter a character is in a string of characters.
            static constexpr bool hasCharacter(const char* characters, const char character) {
                return *characters != '\0' && (*characters == character || hasCharacter(characters + 1, character));
            }

            static constexpr const char* whitespaceCharacters = " \n\t\v\f\r\0";
            static constexpr const char* separationCharacters = ",}]\0";
    };

    /// ObjectParameter is a specialised parameter which contains other parameters.
    class ObjectParameter : public Parameter {
        public:
            ObjectParameter() : Parameter() {}
            template <typename StringType, typename ParameterUniquePtrType, typename... Rest>
            ObjectParameter(StringType&& key, ParameterUniquePtrType&& parameter, Rest&&... rest) :
                ObjectParameter(std::forward<Rest>(rest)...)
            {
                _parameterByKey.insert(std::make_pair(std::forward<StringType>(key), std::move(std::forward<ParameterUniquePtrType>(parameter))));
            }
            ObjectParameter(std::unordered_map<std::string, std::unique_ptr<Parameter>> parameterByKey) :
                ObjectParameter()
            {
                _parameterByKey = std::move(parameterByKey);
            }
            ObjectParameter(const ObjectParameter&) = delete;
            ObjectParameter(ObjectParameter&&) = default;
            ObjectParameter& operator=(const ObjectParameter&) = delete;
            ObjectParameter& operator=(ObjectParameter&&) = default;
            virtual ~ObjectParameter() {}
            const ListParameter& getListParameter(
                const std::vector<std::string>::const_iterator key,
                const std::vector<std::string>::const_iterator end
            ) const override {
                if (key == end) {
                    throw ParameterError("not enough keys");
                }
                return _parameterByKey.at(*key)->getListParameter(key + 1, end);
            }
            bool getBoolean(const std::vector<std::string>::const_iterator key, const std::vector<std::string>::const_iterator end) const override {
                if (key == end) {
                    throw ParameterError("not enough keys");
                }
                return _parameterByKey.at(*key)->getBoolean(key + 1, end);
            }
            double getNumber(const std::vector<std::string>::const_iterator key, const std::vector<std::string>::const_iterator end) const override {
                if (key == end) {
                    throw ParameterError("not enough keys");
                }
                return _parameterByKey.at(*key)->getNumber(key + 1, end);
            }
            virtual std::string getString(const std::vector<std::string>::const_iterator key, const std::vector<std::string>::const_iterator end) const override {
                if (key == end) {
                    throw ParameterError("not enough keys");
                }
                return _parameterByKey.at(*key)->getString(key + 1, end);
            }
            virtual std::unique_ptr<Parameter> clone() const override {
                std::unordered_map<std::string, std::unique_ptr<Parameter>> newParameterByKey;
                for (auto&& keyAndParameter : _parameterByKey) {
                    newParameterByKey.insert(std::make_pair(keyAndParameter.first, keyAndParameter.second->clone()));
                }
                return make_unique<ObjectParameter>(std::move(newParameterByKey));
            }
            virtual std::string::const_iterator load(std::string::const_iterator begin, std::string::const_iterator fileEnd, uint32_t& line) override {
                begin = trim(begin, fileEnd, line);
                if (*begin != '{') {
                    throw ParseError("the object does not begin with a brace", line);
                }
                ++begin;
                auto status = ObjectExpecting::whitespace;
                auto end = begin;
                auto key = std::string();
                for (; *end != '}';) {
                    if (end == fileEnd) {
                        throw ParseError("unexpected end of file (a closing brace might be missing)", line);
                    }
                    switch (status) {
                        case ObjectExpecting::whitespace:
                            end = trim(end, fileEnd, line);
                            status = ObjectExpecting::key;
                            break;
                        case ObjectExpecting::key:
                            if (*end != '"') {
                                throw ParseError("the key does not start with quotes", line);
                            }
                            ++end;
                            status = ObjectExpecting::firstKeyLetter;
                            break;
                        case ObjectExpecting::firstKeyLetter:
                            if (*end == '"') {
                                throw ParseError("the key is an empty string", line);
                            }
                            begin = end;
                            ++end;
                            status = ObjectExpecting::keyLetter;
                            break;
                        case ObjectExpecting::keyLetter:
                            if (*end == '"') {
                                key = std::string(begin, end);
                                ++end;
                                end = trim(end, fileEnd, line);
                                status = ObjectExpecting::keySeparator;
                            } else {
                                ++end;
                            }
                            break;
                        case ObjectExpecting::keySeparator:
                            if (*end != ':') {
                                throw ParseError("key separator ':' not found", line);
                            }
                            ++end;
                            status = ObjectExpecting::field;
                            break;
                        case ObjectExpecting::field:
                            if (_parameterByKey.find(key) == _parameterByKey.end()) {
                                throw ParseError("unexpected key " + key, line);
                            }
                            end = _parameterByKey[key]->load(end, fileEnd, line);

                            status = ObjectExpecting::fieldSeparator;
                            break;
                        case ObjectExpecting::fieldSeparator:
                            if (*end != ',') {
                                throw ParseError("field separator ',' not found", line);
                            }
                            ++end;
                            status = ObjectExpecting::whitespace;
                            break;
                    }
                }
                ++end;
                return trim(end, fileEnd, line);
            }
            virtual void load(const Parameter& parameter) override {
                try {
                    const auto& objectParameter = dynamic_cast<const ObjectParameter&>(parameter);
                    for (auto&& keyAndParameter : objectParameter) {
                        if (_parameterByKey.find(keyAndParameter.first) == _parameterByKey.end()) {
                            throw std::runtime_error("Unexpected key " + keyAndParameter.first);
                        }
                        _parameterByKey[keyAndParameter.first]->load(*keyAndParameter.second);
                    }
                } catch (const std::bad_cast& exception) {
                    throw std::logic_error("Expected an ObjectParameter, got a " + std::string(typeid(parameter).name()));
                }
            }

            /// begin returns an iterator to the beginning of the contained parameters map.
            virtual std::unordered_map<std::string, std::unique_ptr<Parameter>>::const_iterator begin() const {
                return _parameterByKey.begin();
            }

            /// end returns an iterator to the end of the contained parameters map.
            virtual std::unordered_map<std::string, std::unique_ptr<Parameter>>::const_iterator end() const {
                return _parameterByKey.end();
            }

        protected:
            /// ObjectExpecting describes the next character expected by the parser.
            enum class ObjectExpecting {
                whitespace,
                key,
                firstKeyLetter,
                keyLetter,
                keySeparator,
                field,
                fieldSeparator,
            };

            std::unordered_map<std::string, std::unique_ptr<Parameter>> _parameterByKey;
    };

    /// ListParameter is a specialised parameter which contains other parameters.
    class ListParameter : public Parameter {
        public:
            /// makeEmpty generates an empty list parameter with the given value as template.
            static std::unique_ptr<ListParameter> makeEmpty(std::unique_ptr<Parameter> templateParameter) {
                return make_unique<ListParameter>(std::vector<std::unique_ptr<Parameter>>(), std::move(templateParameter));
            }

            template <typename ParameterUniquePtrType>
            ListParameter(ParameterUniquePtrType&& parameter) :
                Parameter(),
                _templateParameter(parameter->clone())
            {
                _parameters.push_back(std::move(parameter));
            }
            template <typename ParameterUniquePtrType, typename... Rest>
            ListParameter(ParameterUniquePtrType&& parameter, Rest&&... rest) :
                ListParameter(std::forward<Rest>(rest)...)
            {
                _parameters.insert(_parameters.begin(), std::move(std::forward<ParameterUniquePtrType>(parameter)));
            }
            ListParameter(std::vector<std::unique_ptr<Parameter>> parameters, std::unique_ptr<Parameter> templateParameter) :
                Parameter(),
                _parameters(std::move(parameters)),
                _templateParameter(std::move(templateParameter))
            {
            }
            ListParameter(const ListParameter&) = delete;
            ListParameter(ListParameter&&) = default;
            ListParameter& operator=(const ListParameter&) = delete;
            ListParameter& operator=(ListParameter&&) = default;
            virtual ~ListParameter() {}
            virtual const ListParameter& getListParameter(
                const std::vector<std::string>::const_iterator key,
                const std::vector<std::string>::const_iterator end
            ) const override {
                if (key != end) {
                    throw ParameterError("too many keys");
                }
                return *this;
            }
            virtual std::unique_ptr<Parameter> clone() const override {
                std::vector<std::unique_ptr<Parameter>> newParameters;
                for (auto&& parameter : _parameters) {
                    newParameters.push_back(parameter->clone());
                }
                return make_unique<ListParameter>(std::move(newParameters), _templateParameter->clone());
            }
            virtual std::string::const_iterator load(std::string::const_iterator begin, std::string::const_iterator fileEnd, uint32_t& line) override {
                _parameters.clear();
                begin = trim(begin, fileEnd, line);
                if (*begin != '[') {
                    throw ParseError("the list does not begin with a bracket", line);
                }
                ++begin;
                auto status = ListExpecting::whitespace;
                auto end = begin;
                auto key = std::string();
                for (; *end != ']';) {
                    if (end == fileEnd) {
                        throw ParseError("unexpected end of file (a closing brace might be missing)", line);
                    }
                    switch (status) {
                        case ListExpecting::whitespace:
                            end = trim(end, fileEnd, line);
                            status = ListExpecting::field;
                            break;
                        case ListExpecting::field:
                            _parameters.push_back(_templateParameter->clone());
                            end = _parameters.back()->load(end, fileEnd, line);
                            status = ListExpecting::fieldSeparator;
                            break;
                        case ListExpecting::fieldSeparator:
                            if (*end != ',') {
                                throw ParseError("field separator ',' not found", line);
                            }
                            ++end;
                            status = ListExpecting::whitespace;
                            break;
                    }
                }
                ++end;
                return trim(end, fileEnd, line);
            }
            virtual void load(const Parameter& parameter) override {
                try {
                    const ListParameter& listParameter = dynamic_cast<const ListParameter&>(parameter);
                    _parameters.clear();
                    for (auto&& storedParameter : listParameter) {
                        auto newParameter = _templateParameter->clone();
                        newParameter->load(*storedParameter);
                        _parameters.push_back(std::move(newParameter));
                    }
                } catch (const std::bad_cast& exception) {
                    throw std::logic_error("Expected an ObjectParameter, got a " + std::string(typeid(parameter).name()));
                }
            }

            /// size returns the list number of elements
            virtual std::size_t size() const {
                return _parameters.size();
            }

            /// begin returns an iterator to the beginning of the contained parameters vector.
            virtual std::vector<std::unique_ptr<Parameter>>::const_iterator begin() const {
                return _parameters.begin();
            }

            /// end returns an iterator to the end of the contained parameters vector.
            virtual std::vector<std::unique_ptr<Parameter>>::const_iterator end() const {
                return _parameters.end();
            }

        protected:

            /// ListExpecting describes the next character expected by the parser.
            enum class ListExpecting {
                whitespace,
                field,
                fieldSeparator,
            };

            std::vector<std::unique_ptr<Parameter>> _parameters;
            std::unique_ptr<Parameter> _templateParameter;
    };

    /// BooleanParameter is a specialised parameter for boolean values.
    class BooleanParameter : public Parameter {
        public:
            BooleanParameter(bool value) :
                Parameter(),
                _value(value)
            {
            }
            BooleanParameter(const BooleanParameter&) = delete;
            BooleanParameter(BooleanParameter&&) = default;
            BooleanParameter& operator=(const BooleanParameter&) = delete;
            BooleanParameter& operator=(BooleanParameter&&) = default;
            virtual ~BooleanParameter() {}
            virtual bool getBoolean(const std::vector<std::string>::const_iterator key, const std::vector<std::string>::const_iterator end) const override {
                if (key != end) {
                    throw ParameterError("too many keys");
                }
                return _value;
            }
            virtual std::unique_ptr<Parameter> clone() const override {
                return make_unique<BooleanParameter>(_value);
            }
            virtual std::string::const_iterator load(std::string::const_iterator begin, std::string::const_iterator fileEnd, uint32_t& line) override {
                begin = trim(begin, fileEnd, line);
                auto end = begin;
                for (; !hasCharacter(separationCharacters, *end) && !hasCharacter(whitespaceCharacters, *end); ++end) {
                    if (end == fileEnd) {
                        throw ParseError("unexpected end of file", line);
                    }
                }
                const auto trimmedJsonData = std::string(begin, end);
                if (trimmedJsonData == "true") {
                    _value = true;
                } else if (trimmedJsonData == "false") {
                    _value = false;
                } else {
                    throw ParseError("expected a boolean", line);
                }
                return trim(end, fileEnd, line);
            }
            virtual void load(const Parameter& parameter) override {
                try {
                    _value = parameter.getBoolean({});
                } catch (const ParameterError& exception) {
                    throw std::logic_error("Expected a BooleanParameter, got a " + std::string(typeid(parameter).name()));
                }
            }

        protected:
            bool _value;
    };

    /// NumberParameter is a specialised parameter for numeric values.
    class NumberParameter : public Parameter {
        public:
            NumberParameter(double value, double minimum, double maximum, bool isInteger) :
                Parameter(),
                _value(value),
                _minimum(minimum),
                _maximum(maximum),
                _isInteger(isInteger)
            {
                validate();
            }
            NumberParameter(const NumberParameter&) = delete;
            NumberParameter(NumberParameter&&) = default;
            NumberParameter& operator=(const NumberParameter&) = delete;
            NumberParameter& operator=(NumberParameter&&) = default;
            virtual ~NumberParameter() {}
            virtual double getNumber(const std::vector<std::string>::const_iterator key, const std::vector<std::string>::const_iterator end) const override {
                if (key != end) {
                    throw ParameterError("too many keys");
                }
                return _value;
            }
            virtual std::unique_ptr<Parameter> clone() const override {
                return make_unique<NumberParameter>(_value, _minimum, _maximum, _isInteger);
            }
            virtual std::string::const_iterator load(std::string::const_iterator begin, std::string::const_iterator fileEnd, uint32_t& line) override {
                begin = trim(begin, fileEnd, line);
                auto end = begin;
                for (; !hasCharacter(separationCharacters, *end) && !hasCharacter(whitespaceCharacters, *end); ++end) {
                    if (end == fileEnd) {
                        throw ParseError("unexpected end of file", line);
                    }
                }
                try {
                    _value = std::stod(std::string(begin, end));
                } catch (const std::invalid_argument&) {
                    throw ParseError("expected a number", line);
                }
                try {
                    validate();
                } catch (const ParameterError& exception) {
                    throw ParseError(exception.what(), line);
                }
                return trim(end, fileEnd, line);
            }
            virtual void load(const Parameter& parameter) override {
                try {
                    _value = parameter.getNumber({});
                } catch (const ParameterError& exception) {
                    throw std::logic_error("Expected a NumberParameter, got a " + std::string(typeid(parameter).name()));
                }
                validate();
            }

        protected:

            /// validate determines if the number is valid regarding the given constraints.
            void validate() {
                if (std::isnan(_value)) {
                    throw ParameterError("expected a number");
                }
                if (_value >= _maximum) {
                    throw ParameterError(std::string("larger than maximum ") + std::to_string(_maximum));
                }
                if (_value < _minimum) {
                    throw ParameterError(std::string("smaller than minimum ") + std::to_string(_minimum));
                }
                auto integerPart = static_cast<double>(0);
                if (_isInteger && std::modf(_value, &integerPart) != static_cast<double>(0)) {
                    throw ParameterError("expected an integer");
                }
            }

            double _value;
            double _minimum;
            double _maximum;
            bool _isInteger;
    };

    /// CharParameter is a specialised number parameter for char numeric values.
    class CharParameter : public NumberParameter {
        public:
            CharParameter(double value) :
                NumberParameter(value, 0, 256, true)
            {
            }
            CharParameter(const CharParameter&) = delete;
            CharParameter(CharParameter&&) = default;
            CharParameter& operator=(const CharParameter&) = delete;
            CharParameter& operator=(CharParameter&&) = default;
            virtual ~CharParameter() {}
            virtual std::unique_ptr<Parameter> clone() const override {
                return make_unique<CharParameter>(_value);
            }
    };

    /// StringParameter is a specialised parameter for string values.
    class StringParameter : public Parameter {
        public:
            StringParameter(const std::string& value) :
                Parameter(),
                _value(value)
            {
            }
            StringParameter(const StringParameter&) = delete;
            StringParameter(StringParameter&&) = default;
            StringParameter& operator=(const StringParameter&) = delete;
            StringParameter& operator=(StringParameter&&) = default;
            virtual ~StringParameter() {}
            virtual std::string getString(const std::vector<std::string>::const_iterator key, const std::vector<std::string>::const_iterator end) const override {
                if (key != end) {
                    throw ParameterError("too many keys");
                }
                return _value;
            }
            virtual std::unique_ptr<Parameter> clone() const override {
                return make_unique<StringParameter>(_value);
            }
            virtual std::string::const_iterator load(std::string::const_iterator begin, std::string::const_iterator fileEnd, uint32_t& line) override {
                begin = trim(begin, fileEnd, line);
                if (*begin != '"') {
                    throw ParseError("the string does not start with quotes", line);
                }
                ++begin;
                auto end = begin;
                for (; *end != '"'; ++end) {
                    if (end == fileEnd) {
                        throw ParseError("unexpected end of file", line);
                    }

                    if (*end == '\n') {
                        throw ParseError("strings cannot contain linebreaks", line);
                    }
                }
                _value = std::string(begin, end);
                ++end;
                return trim(end, fileEnd, line);
            }
            virtual void load(const Parameter& parameter) override {
                try {
                    _value = parameter.getString({});
                } catch (const ParameterError& exception) {
                    throw std::logic_error("Expected a StringParameter, got a " + std::string(typeid(parameter).name()));
                }
            }

        protected:
            std::string _value;
    };

    /// EnumParameter is a specialised parameter for string values with a given set of possible values.
    class EnumParameter : public StringParameter {
        public:
            EnumParameter(std::string value, std::unordered_set<std::string> availableValues) :
                StringParameter(value),
                _availableValues(availableValues)
            {
                if (_availableValues.size() == 0) {
                    throw ParameterError("an enum parameter needs at least one available value");
                }
                validate();
            }
            EnumParameter(const EnumParameter&) = delete;
            EnumParameter(EnumParameter&&) = default;
            EnumParameter& operator=(const EnumParameter&) = delete;
            EnumParameter& operator=(EnumParameter&&) = default;
            virtual ~EnumParameter() {}
            virtual std::unique_ptr<Parameter> clone() const override {
                return make_unique<EnumParameter>(_value, _availableValues);
            }
            virtual std::string::const_iterator load(std::string::const_iterator begin, std::string::const_iterator fileEnd, uint32_t& line) override {
                begin = trim(begin, fileEnd, line);
                if (*begin != '"') {
                    throw ParseError("the string does not start with quotes", line);
                }
                ++begin;
                auto end = begin;
                for (; *end != '"'; ++end) {
                    if (end == fileEnd) {
                        throw ParseError("unexpected end of file", line);
                    }

                    if (*end == '\n') {
                        throw ParseError("strings cannot contain linebreaks", line);
                    }
                }
                _value = std::string(begin, end);
                try {
                    validate();
                } catch (const ParameterError& exception) {
                    throw ParseError(exception.what(), line);
                }
                ++end;
                return trim(end, fileEnd, line);
            }
            virtual void load(const Parameter& parameter) override {
                try {
                    _value = parameter.getString({});
                } catch (const ParameterError& exception) {
                    throw std::logic_error("Expected an EnumParameter, got a " + std::string(typeid(parameter).name()));
                }
                validate();
            }

        protected:
            /// validate determines if the enum is valid regarding the given constraints.
            void validate() {
                if (_availableValues.find(_value) == _availableValues.end()) {
                    auto availableValuesString = std::string("{");
                    for (auto&& availableValue : _availableValues) {
                        if (availableValuesString != "{") {
                            availableValuesString += ", ";
                        }
                        availableValuesString += availableValue;
                    }
                    availableValuesString += "}";
                    throw ParameterError("The value " + _value + " should be one of " + availableValuesString);
                }
            }

            std::unordered_set<std::string> _availableValues;
    };

    /// UnvalidatedParameter represents either a parameter subset or a JSON filename to be validated against a complete parameter.
    /// It mimics a union behavior with poor memory management. However, the lifecycles of its attributes are properly managed.
    /// The class handles file reading when constructed with a JSON filename.
    class UnvalidatedParameter {
        public:
            UnvalidatedParameter(const std::string& jsonFilename) :
                _isString(true)
            {
                if (jsonFilename != "") {
                    std::ifstream file(jsonFilename);
                    if (!file.good()) {
                        throw UnreadableFile(jsonFilename);
                    }
                    file.seekg(0, std::fstream::end);
                    _jsonData.reserve(std::size_t(file.tellg()));
                    file.seekg(0, std::fstream::beg);
                    _jsonData.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                }
            }
            UnvalidatedParameter(std::unique_ptr<Parameter> parameter) :
                _isString(false),
                _parameter(std::move(parameter))
            {
            }
            UnvalidatedParameter(const UnvalidatedParameter&) = delete;
            UnvalidatedParameter(UnvalidatedParameter&&) = default;
            UnvalidatedParameter& operator=(const UnvalidatedParameter&) = delete;
            UnvalidatedParameter& operator=(UnvalidatedParameter&&) = default;
            virtual ~UnvalidatedParameter() {}

            /// isString returns true if the object was constructed as a string.
            virtual bool isString() const {
                return _isString;
            }

            /// jsonData returns the json data inside the given filename.
            /// An error is thrown if the object was constructed with a parameter.
            virtual const std::string& jsonData() const {
                if (!_isString) {
                    throw ParameterError("The unvalidated parameter is not a string");
                }
                return _jsonData;
            }

            /// parameter returns the provided parameter.
            /// An error is thrown if the object was consrtructed with a string.
            virtual const Parameter& parameter() const {
                if (_isString) {
                    throw ParameterError("The unvalidated parameter is not a parameter");
                }
                return *_parameter;
            }

        protected:
            bool _isString;
            std::string _jsonData;
            std::unique_ptr<Parameter> _parameter;
    };

    /// SpecialisedCamera represents a template-specialised generic event-based camera.
    template <typename EventType, typename HandleEvent, typename HandleException>
    class SpecialisedCamera {
        public:
            SpecialisedCamera(
                HandleEvent handleEvent,
                HandleException handleException,
                std::size_t fifoSize,
                std::chrono::milliseconds sleepDuration
            ) :
                _handleEvent(std::forward<HandleEvent>(handleEvent)),
                _handleException(std::forward<HandleException>(handleException)),
                _bufferRunning(true),
                _sleepDuration(sleepDuration),
                _head(0),
                _tail(0)
            {
                _events.resize(fifoSize);
                _bufferLoop = std::thread([this]() -> void {
                    try {
                        EventType event;
                        while (_bufferRunning.load(std::memory_order_relaxed)) {
                            const auto currentHead = _head.load(std::memory_order_relaxed);
                            if (currentHead == _tail.load(std::memory_order_acquire)) {
                                std::this_thread::sleep_for(_sleepDuration);
                            } else {
                                event = _events[currentHead];
                                _head.store((currentHead + 1) % _events.size(), std::memory_order_release);
                                this->_handleEvent(event);
                            }
                        }
                    } catch (...) {
                        this->_handleException(std::current_exception());
                    }
                });
            }
            SpecialisedCamera(const SpecialisedCamera&) = delete;
            SpecialisedCamera(SpecialisedCamera&&) = default;
            SpecialisedCamera& operator=(const SpecialisedCamera&) = delete;
            SpecialisedCamera& operator=(SpecialisedCamera&&) = default;
            virtual ~SpecialisedCamera() {
                _bufferRunning.store(false, std::memory_order_relaxed);
                _bufferLoop.join();
            }

            /// push adds an event to the circular FIFO in a thread-safe manner, as long as one thread only is writting.
            /// If the event could not be inserted (FIFO full), false is returned.
            virtual bool push(EventType event) {
                const auto currentTail = _tail.load(std::memory_order_relaxed);
                const auto nextTail = (currentTail + 1) % _events.size();
                if (nextTail != _head.load(std::memory_order_acquire)) {
                    _events[currentTail] = event;
                    _tail.store(nextTail, std::memory_order_release);
                    return true;
                }
                return false;
            }

        protected:
            HandleEvent _handleEvent;
            HandleException _handleException;
            std::thread _bufferLoop;
            std::atomic_bool _bufferRunning;
            std::chrono::milliseconds _sleepDuration;
            std::atomic<std::size_t> _head;
            std::atomic<std::size_t> _tail;
            std::vector<EventType> _events;
    };
}
