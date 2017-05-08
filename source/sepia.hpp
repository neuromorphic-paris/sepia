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

    /// falseFunction is a function returning false.
    inline bool falseFunction() {
        return false;
    }

    /// DvsEvent represents the parameters of a change detection.
    struct DvsEvent {

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
    } __attribute__((packed));

    /// AtisEvent represents the parameters of a change detection or an exposure measurement.
    struct AtisEvent {

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
    } __attribute__((packed));

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
    } __attribute__((packed));

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
    } __attribute__((packed));

    /// GenericEvent represents the parameters of a generic event.
    struct GenericEvent {

        /// timestamp represents the event's timestamp.
        uint64_t timestamp;

        /// data represents untagged data associated with the event.
        uint64_t data;

        /// extraBit is an extra data bit associated with the event.
        bool extraBit;
    } __attribute__((packed));

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
            virtual void operator()(AtisEvent atisEvent) {
                if (atisEvent.isThresholdCrossing) {
                    _handleThresholdCrossing(ThresholdCrossing{atisEvent.x, atisEvent.y, atisEvent.timestamp, atisEvent.polarity});
                } else {
                    _handleChangeDetection(DvsEvent{atisEvent.x, atisEvent.y, atisEvent.timestamp, atisEvent.polarity});
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

    /// EventStreamObservable is a base class for event stream observables.
    class EventStreamObservable {
        public:

            /// Dispatch specifies when the events are dispatched.
            enum class Dispatch {
                synchronouslyButSkipOffset,
                synchronously,
                asFastAsPossible,
            };

            EventStreamObservable() {}
            EventStreamObservable(const EventStreamObservable&) = delete;
            EventStreamObservable(EventStreamObservable&&) = default;
            EventStreamObservable& operator=(const EventStreamObservable&) = delete;
            EventStreamObservable& operator=(EventStreamObservable&&) = default;
            virtual ~EventStreamObservable() {}

        protected:

            /// readAndDispatch implements a generic dispatch mechanism for event stream files.
            template <typename Event, typename HandleByte, typename MustRestart, typename HandleEvent, typename HandleException>
            static void readAndDispatch(
                std::ifstream& eventStream,
                std::atomic_bool& running,
                EventStreamObservable::Dispatch dispatch,
                std::size_t chunkSize,
                HandleByte handleByte,
                MustRestart mustRestart,
                HandleEvent handleEvent,
                HandleException handleException
            ) {
                try {
                    Event event;
                    auto bytes = std::vector<uint8_t>(chunkSize);
                    switch (dispatch) {
                        case EventStreamObservable::Dispatch::synchronouslyButSkipOffset: {
                            auto offsetSkipped = false;
                            auto timeReference = std::chrono::system_clock::now();
                            uint64_t initialTimestamp = 0;
                            uint64_t previousTimestamp = 0;
                            while (running.load(std::memory_order_relaxed)) {
                                eventStream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                if (eventStream.eof()) {
                                    for (auto byteIterator = bytes.begin(); byteIterator != std::next(bytes.begin(), eventStream.gcount()); ++byteIterator) {
                                        if (handleByte(*byteIterator, event)) {
                                            if (offsetSkipped) {
                                                if (event.timestamp > previousTimestamp) {
                                                    previousTimestamp = event.timestamp;
                                                    std::this_thread::sleep_until(timeReference + std::chrono::microseconds(event.timestamp - initialTimestamp));
                                                }
                                            } else {
                                                offsetSkipped = true;
                                                initialTimestamp = event.timestamp;
                                                previousTimestamp = event.timestamp;
                                            }
                                            handleEvent(event);
                                        }
                                    }
                                    if (mustRestart()) {
                                        eventStream.clear();
                                        eventStream.seekg(15);
                                        offsetSkipped = false;
                                        handleByte.reset();
                                        timeReference = std::chrono::system_clock::now();
                                        continue;
                                    }
                                    throw EndOfFile();
                                }
                                for (auto&& byte : bytes) {
                                    if (handleByte(byte, event)) {
                                        if (offsetSkipped) {
                                            if (event.timestamp > previousTimestamp) {
                                                previousTimestamp = event.timestamp;
                                                std::this_thread::sleep_until(timeReference + std::chrono::microseconds(event.timestamp - initialTimestamp));
                                            }
                                        } else {
                                            offsetSkipped = true;
                                            initialTimestamp = event.timestamp;
                                            previousTimestamp = event.timestamp;
                                        }
                                        handleEvent(event);
                                    }
                                }
                            }
                        }
                        case EventStreamObservable::Dispatch::synchronously: {
                            auto timeReference = std::chrono::system_clock::now();
                            uint64_t previousTimestamp = 0;
                            while (running.load(std::memory_order_relaxed)) {
                                eventStream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                if (eventStream.eof()) {
                                    for (auto byteIterator = bytes.begin(); byteIterator != std::next(bytes.begin(), eventStream.gcount()); ++byteIterator) {
                                        if (handleByte(*byteIterator, event)) {
                                            if (event.timestamp > previousTimestamp) {
                                                std::this_thread::sleep_until(timeReference + std::chrono::microseconds(event.timestamp));
                                            }
                                            previousTimestamp = event.timestamp;
                                            handleEvent(event);
                                        }
                                    }
                                    if (mustRestart()) {
                                        eventStream.clear();
                                        eventStream.seekg(15);
                                        handleByte.reset();
                                        timeReference = std::chrono::system_clock::now();
                                        continue;
                                    }
                                    throw EndOfFile();
                                }
                                for (auto&& byte : bytes) {
                                    if (handleByte(byte, event)) {
                                        if (event.timestamp > previousTimestamp) {
                                            std::this_thread::sleep_until(timeReference + std::chrono::microseconds(event.timestamp));
                                        }
                                        previousTimestamp = event.timestamp;
                                        handleEvent(event);
                                    }
                                }
                            }
                        }
                        case EventStreamObservable::Dispatch::asFastAsPossible: {
                            while (running.load(std::memory_order_relaxed)) {
                                eventStream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
                                if (eventStream.eof()) {
                                    for (auto byteIterator = bytes.begin(); byteIterator != std::next(bytes.begin(), eventStream.gcount()); ++byteIterator) {
                                        if (handleByte(*byteIterator, event)) {
                                            handleEvent(event);
                                        }
                                    }
                                    if (mustRestart()) {
                                        eventStream.clear();
                                        eventStream.seekg(15);
                                        handleByte.reset();
                                        continue;
                                    }
                                    throw EndOfFile();
                                }
                                for (auto&& byte : bytes) {
                                    if (handleByte(byte, event)) {
                                        handleEvent(event);
                                    }
                                }
                            }
                        }
                    }
                } catch (...) {
                    handleException(std::current_exception());
                }
            }
    };

    /// DvsEventStreamWriter writes events to an Event Stream file.
    class DvsEventStreamWriter {
        public:
            DvsEventStreamWriter():
                _logging(false),
                _previousTimestamp(0)
            {
                _accessingEventStream.clear(std::memory_order_release);
            }
            DvsEventStreamWriter(const DvsEventStreamWriter&) = delete;
            DvsEventStreamWriter(DvsEventStreamWriter&&) = default;
            DvsEventStreamWriter& operator=(const DvsEventStreamWriter&) = delete;
            DvsEventStreamWriter& operator=(DvsEventStreamWriter&&) = default;
            virtual ~DvsEventStreamWriter() {}

            /// operator() handles an event.
            virtual void operator()(DvsEvent dvsEvent) {
                while (_accessingEventStream.test_and_set(std::memory_order_acquire)) {}
                if (_logging) {
                    auto relativeTimestamp = dvsEvent.timestamp - _previousTimestamp;
                    if (relativeTimestamp >= 15) {
                        const auto numberOfOverflows = relativeTimestamp / 15;
                        for (std::size_t index = 0; index < numberOfOverflows / 15; ++index) {
                            _eventStream.put(static_cast<uint8_t>(0b11111111));
                        }
                        const auto numberOfOverflowsLeft = numberOfOverflows % 15;
                        if (numberOfOverflowsLeft > 0) {
                            _eventStream.put(static_cast<uint8_t>(0b1111) | static_cast<uint8_t>(numberOfOverflowsLeft << 4));
                        }
                        relativeTimestamp -= numberOfOverflows * 15;
                    }
                    _eventStream.put(static_cast<uint8_t>(relativeTimestamp) | static_cast<uint8_t>((dvsEvent.x & 0b1111) << 4));
                    _eventStream.put(static_cast<uint8_t>((dvsEvent.x & 0b1111110000) >> 4) | static_cast<uint8_t>((dvsEvent.y & 0b11) << 6));
                    _eventStream.put(static_cast<uint8_t>((dvsEvent.y & 0b111111100) >> 2) | static_cast<uint8_t>(dvsEvent.isIncrease ? 0b10000000 : 0));
                    _previousTimestamp = dvsEvent.timestamp;
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
                _eventStream.write("Event Stream", 12);
                _eventStream.put(1);
                _eventStream.put(0);
                _eventStream.put(0);
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

    /// HandleDvsByte implements the event stream state machine for DVS events.
    class HandleDvsByte {
        public:
            HandleDvsByte() :
                _state(State::idle),
                _dvsEvent(DvsEvent{0, 0, 0, false})
            {
            }
            HandleDvsByte(const HandleDvsByte&) = default;
            HandleDvsByte(HandleDvsByte&&) = default;
            HandleDvsByte& operator=(const HandleDvsByte&) = default;
            HandleDvsByte& operator=(HandleDvsByte&&) = default;
            virtual ~HandleDvsByte() {}

            /// operator() handles a byte.
            virtual bool operator()(uint8_t byte, DvsEvent& dvsEvent) {
                switch (_state) {
                    case State::idle: {
                        if ((byte & 0b1111) == 0b1111) {
                            _dvsEvent.timestamp += ((byte & 0b11110000) >> 4) * 15;
                        } else {
                            _dvsEvent.timestamp = _dvsEvent.timestamp + (byte & 0b1111);
                            _dvsEvent.x = ((byte & 0b11110000) >> 4);
                            _state = State::byte0;
                        }
                        return false;
                    }
                    case State::byte0: {
                        _dvsEvent.x |= (static_cast<uint16_t>(byte & 0b111111) << 4);
                        _dvsEvent.y = ((byte & 0b11000000) >> 6);
                        _state = State::byte1;
                        return false;
                    }
                    case State::byte1: {
                        _dvsEvent.y |= (static_cast<uint16_t>(byte & 0b1111111) << 2);
                        _dvsEvent.isIncrease = (((byte & 0b10000000) >> 7) == 1);
                        _state = State::idle;
                        dvsEvent = _dvsEvent;
                        return true;
                    }
                }
            }

            /// reset initialises the state machine.
            virtual void reset() {
                _state = State::idle;
                _dvsEvent.timestamp = 0;
            }

        protected:

            /// State represents the current state machine's state.
            enum class State {
                idle,
                byte0,
                byte1,
            };

            State _state;
            DvsEvent _dvsEvent;
    };

    /// DvsEventStreamObservable is a template-specialised EventStreamObservable for DVS events.
    template <typename HandleEvent, typename HandleException, typename MustRestart>
    class DvsEventStreamObservable : public EventStreamObservable {
        public:
            DvsEventStreamObservable(
                const std::string& filename,
                HandleEvent handleEvent,
                HandleException handleException,
                MustRestart mustRestart,
                EventStreamObservable::Dispatch dispatch,
                const std::size_t& chunkSize
            ) :
                _eventStream(filename, std::ifstream::binary),
                _running(true)
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
                    if (versionMajor != 1 || versionMinor != 0) {
                        throw UnsupportedVersion(filename);
                    }
                    const auto mode = _eventStream.get();
                    if (_eventStream.eof() || mode != 0) {
                        throw UnsupportedMode(filename);
                    }
                }
                _loop = std::thread(
                    readAndDispatch<DvsEvent, HandleDvsByte, MustRestart, HandleEvent, HandleException>,
                    std::ref(_eventStream),
                    std::ref(_running),
                    dispatch,
                    chunkSize,
                    HandleDvsByte(),
                    std::forward<MustRestart>(mustRestart),
                    std::forward<HandleEvent>(handleEvent),
                    std::forward<HandleException>(handleException)
                );
            }
            DvsEventStreamObservable(const DvsEventStreamObservable&) = delete;
            DvsEventStreamObservable(DvsEventStreamObservable&&) = default;
            DvsEventStreamObservable& operator=(const DvsEventStreamObservable&) = delete;
            DvsEventStreamObservable& operator=(DvsEventStreamObservable&&) = default;
            virtual ~DvsEventStreamObservable() {
                _running.store(false, std::memory_order_relaxed);
                _loop.join();
            }

        protected:
            std::ifstream _eventStream;
            std::atomic_bool _running;
            std::thread _loop;
    };

    /// make_dvsEventStreamObservable creates an event stream observable from functors.
    template<typename HandleEvent, typename HandleException, typename MustRestart = decltype(&falseFunction)>
    std::unique_ptr<DvsEventStreamObservable<HandleEvent, HandleException, MustRestart>> make_dvsEventStreamObservable(
        const std::string& filename,
        HandleEvent handleEvent,
        HandleException handleException,
        MustRestart mustRestart = &falseFunction,
        EventStreamObservable::Dispatch dispatch = EventStreamObservable::Dispatch::synchronouslyButSkipOffset,
        std::size_t chunkSize = 1 << 10
    ) {
        return sepia::make_unique<DvsEventStreamObservable<HandleEvent, HandleException, MustRestart>>(
            filename,
            std::forward<HandleEvent>(handleEvent),
            std::forward<HandleException>(handleException),
            std::forward<MustRestart>(mustRestart),
            dispatch,
            chunkSize
        );
    }

    /// AtisEventStreamWriter writes events to an Event Stream file.
    class AtisEventStreamWriter {
        public:
            AtisEventStreamWriter():
                _logging(false),
                _previousTimestamp(0)
            {
                _accessingEventStream.clear(std::memory_order_release);
            }
            AtisEventStreamWriter(const AtisEventStreamWriter&) = delete;
            AtisEventStreamWriter(AtisEventStreamWriter&&) = default;
            AtisEventStreamWriter& operator=(const AtisEventStreamWriter&) = delete;
            AtisEventStreamWriter& operator=(AtisEventStreamWriter&&) = default;
            virtual ~AtisEventStreamWriter() {}

            /// operator() handles an event.
            virtual void operator()(AtisEvent atisEvent) {
                while (_accessingEventStream.test_and_set(std::memory_order_acquire)) {}
                if (_logging) {
                    auto relativeTimestamp = atisEvent.timestamp - _previousTimestamp;
                    if (relativeTimestamp >= 31) {
                        const auto numberOfOverflows = relativeTimestamp / 31;
                        for (std::size_t index = 0; index < numberOfOverflows / 7; ++index) {
                            _eventStream.put(static_cast<uint8_t>(0b11111111));
                        }
                        const auto numberOfOverflowsLeft = numberOfOverflows % 7;
                        if (numberOfOverflowsLeft > 0) {
                            _eventStream.put(static_cast<uint8_t>(0b11111) | static_cast<uint8_t>(numberOfOverflowsLeft << 5));
                        }
                        relativeTimestamp -= numberOfOverflows * 31;
                    }
                    _eventStream.put(static_cast<uint8_t>(relativeTimestamp) | static_cast<uint8_t>((atisEvent.x & 0b111) << 5));
                    _eventStream.put(static_cast<uint8_t>((atisEvent.x & 0b111111000) >> 3) | static_cast<uint8_t>((atisEvent.y & 0b11) << 6));
                    _eventStream.put(
                        static_cast<uint8_t>((atisEvent.y & 0b11111100) >> 2)
                        | static_cast<uint8_t>(atisEvent.isThresholdCrossing ? 0b1000000 : 0)
                        | static_cast<uint8_t>(atisEvent.polarity ? 0b10000000 : 0)
                    );
                    _previousTimestamp = atisEvent.timestamp;
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
                _eventStream.write("Event Stream", 12);
                _eventStream.put(1);
                _eventStream.put(0);
                _eventStream.put(1);
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

    /// HandleAtisByte implements the event stream state machine for ATIS events.
    class HandleAtisByte {
        public:
            HandleAtisByte() :
                _state(State::idle),
                _atisEvent(AtisEvent{0, 0, 0, false, false})
            {
            }
            HandleAtisByte(const HandleAtisByte&) = default;
            HandleAtisByte(HandleAtisByte&&) = default;
            HandleAtisByte& operator=(const HandleAtisByte&) = default;
            HandleAtisByte& operator=(HandleAtisByte&&) = default;
            virtual ~HandleAtisByte() {}

            /// operator() handles a byte.
            virtual bool operator()(uint8_t byte, AtisEvent& atisEvent) {
                switch (_state) {
                    case State::idle: {
                        if ((byte & 0b11111) == 0b11111) {
                            _atisEvent.timestamp += ((byte & 0b11100000) >> 5) * 31;
                        } else {
                            _atisEvent.timestamp = _atisEvent.timestamp + (byte & 0b11111);
                            _atisEvent.x = ((byte & 0b11100000) >> 5);
                            _state = State::byte0;
                        }
                        return false;
                    }
                    case State::byte0: {
                        _atisEvent.x |= (static_cast<uint16_t>(byte & 0b111111) << 3);
                        _atisEvent.y = ((byte & 0b11000000) >> 6);
                        _state = State::byte1;
                        return false;
                    }
                    case State::byte1: {
                        _atisEvent.y |= (static_cast<uint16_t>(byte & 0b111111) << 2);
                        _atisEvent.isThresholdCrossing = (((byte & 0b1000000) >> 6) == 1);
                        _atisEvent.polarity = (((byte & 0b10000000) >> 7) == 1);
                        _state = State::idle;
                        atisEvent = _atisEvent;
                        return true;
                    }
                }
            }

            /// reset initialises the state machine.
            virtual void reset() {
                _state = State::idle;
                _atisEvent.timestamp = 0;
            }

        protected:

            /// State represents the current state machine's state.
            enum class State {
                idle,
                byte0,
                byte1,
            };

            State _state;
            AtisEvent _atisEvent;
    };

    /// AtisEventStreamObservable is a template-specialised EventStreamObservable for ATIS events.
    template <typename HandleEvent, typename HandleException, typename MustRestart>
    class AtisEventStreamObservable : public EventStreamObservable {
        public:
            AtisEventStreamObservable(
                const std::string& filename,
                HandleEvent handleEvent,
                HandleException handleException,
                MustRestart mustRestart,
                EventStreamObservable::Dispatch dispatch,
                const std::size_t& chunkSize
            ) :
                _eventStream(filename, std::ifstream::binary),
                _running(true)
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
                    if (versionMajor != 1 || versionMinor != 0) {
                        throw UnsupportedVersion(filename);
                    }
                    const auto mode = _eventStream.get();
                    if (_eventStream.eof() || mode != 1) {
                        throw UnsupportedMode(filename);
                    }
                }
                _loop = std::thread(
                    readAndDispatch<AtisEvent, HandleAtisByte, MustRestart, HandleEvent, HandleException>,
                    std::ref(_eventStream),
                    std::ref(_running),
                    dispatch,
                    chunkSize,
                    HandleAtisByte(),
                    std::forward<MustRestart>(mustRestart),
                    std::forward<HandleEvent>(handleEvent),
                    std::forward<HandleException>(handleException)
                );
            }
            AtisEventStreamObservable(const AtisEventStreamObservable&) = delete;
            AtisEventStreamObservable(AtisEventStreamObservable&&) = default;
            AtisEventStreamObservable& operator=(const AtisEventStreamObservable&) = delete;
            AtisEventStreamObservable& operator=(AtisEventStreamObservable&&) = default;
            virtual ~AtisEventStreamObservable() {
                _running.store(false, std::memory_order_relaxed);
                _loop.join();
            }

        protected:
            std::ifstream _eventStream;
            std::atomic_bool _running;
            std::thread _loop;
    };

    /// make_atisEventStreamObservable creates an event stream observable from functors.
    template<typename HandleEvent, typename HandleException, typename MustRestart = decltype(&falseFunction)>
    std::unique_ptr<AtisEventStreamObservable<HandleEvent, HandleException, MustRestart>> make_atisEventStreamObservable(
        const std::string& filename,
        HandleEvent handleEvent,
        HandleException handleException,
        MustRestart mustRestart = &falseFunction,
        EventStreamObservable::Dispatch dispatch = EventStreamObservable::Dispatch::synchronouslyButSkipOffset,
        std::size_t chunkSize = 1 << 10
    ) {
        return sepia::make_unique<AtisEventStreamObservable<HandleEvent, HandleException, MustRestart>>(
            filename,
            std::forward<HandleEvent>(handleEvent),
            std::forward<HandleException>(handleException),
            std::forward<MustRestart>(mustRestart),
            dispatch,
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
                    if (relativeTimestamp >= 127) {
                        const auto numberOfOverflows = relativeTimestamp / 127;
                        for (std::size_t index = 0; index < numberOfOverflows; ++index) {
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
                _eventStream.write("Event Stream", 12);
                _eventStream.put(1);
                _eventStream.put(0);
                _eventStream.put(3);
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

    /// HandleColorByte implements the event stream state machine for color events.
    class HandleColorByte {
        public:
            HandleColorByte() :
                _state(State::idle),
                _colorEvent(ColorEvent{0, 0, 0, 0, 0, 0})
            {
            }
            HandleColorByte(const HandleColorByte&) = default;
            HandleColorByte(HandleColorByte&&) = default;
            HandleColorByte& operator=(const HandleColorByte&) = default;
            HandleColorByte& operator=(HandleColorByte&&) = default;
            virtual ~HandleColorByte() {}

            /// operator() handles a byte.
            virtual bool operator()(uint8_t byte, ColorEvent& colorEvent) {
                switch (_state) {
                    case State::idle: {
                        if ((byte & 0b1111111) == 0b1111111) {
                            _colorEvent.timestamp += ((byte & 0b10000000) >> 7) * 127;
                        } else {
                            _colorEvent.timestamp = _colorEvent.timestamp + (byte & 0b1111111);
                            _colorEvent.x = ((byte & 0b10000000) >> 7);
                            _state = State::byte0;
                        }
                        return false;
                    }
                    case State::byte0: {
                        _colorEvent.x |= (static_cast<uint16_t>(byte) << 1);
                        _state = State::byte1;
                        return false;
                    }
                    case State::byte1: {
                        _colorEvent.y = byte;
                        _state = State::byte2;
                        return false;
                    }
                    case State::byte2: {
                        _colorEvent.r = byte;
                        _state = State::byte3;
                        return false;
                    }
                    case State::byte3: {
                        _colorEvent.g = byte;
                        _state = State::byte4;
                        return false;
                    }
                    case State::byte4: {
                        _colorEvent.b = byte;
                        _state = State::idle;
                        colorEvent = _colorEvent;
                        return true;
                    }
                }
            }

            /// reset initialises the state machine.
            virtual void reset() {
                _state = State::idle;
                _colorEvent.timestamp = 0;
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

            State _state;
            ColorEvent _colorEvent;
    };

    /// ColorEventStreamObservable is a template-specialised EventStreamObservable for color events.
    template <typename HandleEvent, typename HandleException, typename MustRestart>
    class ColorEventStreamObservable : public EventStreamObservable {
        public:
            ColorEventStreamObservable(
                const std::string& filename,
                HandleEvent handleEvent,
                HandleException handleException,
                MustRestart mustRestart,
                EventStreamObservable::Dispatch dispatch,
                const std::size_t& chunkSize
            ) :
                _eventStream(filename, std::ifstream::binary),
                _running(true)
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
                    if (versionMajor != 1 || versionMinor != 0) {
                        throw UnsupportedVersion(filename);
                    }
                    const auto mode = _eventStream.get();
                    if (_eventStream.eof() || mode != 3) {
                        throw UnsupportedMode(filename);
                    }
                }
                _loop = std::thread(
                    readAndDispatch<ColorEvent, HandleColorByte, MustRestart, HandleEvent, HandleException>,
                    std::ref(_eventStream),
                    std::ref(_running),
                    dispatch,
                    chunkSize,
                    HandleColorByte(),
                    std::forward<MustRestart>(mustRestart),
                    std::forward<HandleEvent>(handleEvent),
                    std::forward<HandleException>(handleException)
                );
            }
            ColorEventStreamObservable(const ColorEventStreamObservable&) = delete;
            ColorEventStreamObservable(ColorEventStreamObservable&&) = default;
            ColorEventStreamObservable& operator=(const ColorEventStreamObservable&) = delete;
            ColorEventStreamObservable& operator=(ColorEventStreamObservable&&) = default;
            virtual ~ColorEventStreamObservable() {
                _running.store(false, std::memory_order_relaxed);
                _loop.join();
            }

        protected:
            std::ifstream _eventStream;
            std::atomic_bool _running;
            std::thread _loop;
    };

    /// make_colorEventStreamObservable creates an event stream observable from functors.
    template<typename HandleEvent, typename HandleException, typename MustRestart = decltype(&falseFunction)>
    std::unique_ptr<ColorEventStreamObservable<HandleEvent, HandleException, MustRestart>> make_colorEventStreamObservable(
        const std::string& filename,
        HandleEvent handleEvent,
        HandleException handleException,
        MustRestart mustRestart = &falseFunction,
        EventStreamObservable::Dispatch dispatch = EventStreamObservable::Dispatch::synchronouslyButSkipOffset,
        std::size_t chunkSize = 1 << 10
    ) {
        return sepia::make_unique<ColorEventStreamObservable<HandleEvent, HandleException, MustRestart>>(
            filename,
            std::forward<HandleEvent>(handleEvent),
            std::forward<HandleException>(handleException),
            std::forward<MustRestart>(mustRestart),
            dispatch,
            chunkSize
        );
    }

    /// GenericEventStreamWriter writes events to a generic Event Stream file.
    class GenericEventStreamWriter {
        public:
            GenericEventStreamWriter():
                _logging(false),
                _previousTimestamp(0)
            {
                _accessingEventStream.clear(std::memory_order_release);
            }
            GenericEventStreamWriter(const GenericEventStreamWriter&) = delete;
            GenericEventStreamWriter(GenericEventStreamWriter&&) = default;
            GenericEventStreamWriter& operator=(const GenericEventStreamWriter&) = delete;
            GenericEventStreamWriter& operator=(GenericEventStreamWriter&&) = default;
            virtual ~GenericEventStreamWriter() {}

            /// operator() handles an event.
            virtual void operator()(GenericEvent genericEvent) {
                while (_accessingEventStream.test_and_set(std::memory_order_acquire)) {}
                if (_logging) {
                    auto relativeTimestamp = genericEvent.timestamp - _previousTimestamp;
                    if (relativeTimestamp >= 127) {
                        const auto numberOfOverflows = relativeTimestamp / 127;
                        for (std::size_t index = 0; index < numberOfOverflows; ++index) {
                            _eventStream.put(static_cast<uint8_t>(0b11111111));
                        }
                        relativeTimestamp -= numberOfOverflows * 127;
                    }
                    _eventStream.put(static_cast<uint8_t>(relativeTimestamp) | static_cast<uint8_t>((genericEvent.extraBit ? 0b1 : 0b0) << 7));
                    _eventStream.put(static_cast<uint8_t>(genericEvent.data & 0x00000000000000ff));
                    _eventStream.put(static_cast<uint8_t>((genericEvent.data & 0x000000000000ff00) >> 8));
                    _eventStream.put(static_cast<uint8_t>((genericEvent.data & 0x0000000000ff0000) >> 16));
                    _eventStream.put(static_cast<uint8_t>((genericEvent.data & 0x00000000ff000000) >> 24));
                    _eventStream.put(static_cast<uint8_t>((genericEvent.data & 0x0000000ff0000000) >> 32));
                    _eventStream.put(static_cast<uint8_t>((genericEvent.data & 0x0000ff0000000000) >> 40));
                    _eventStream.put(static_cast<uint8_t>((genericEvent.data & 0x00ff000000000000) >> 48));
                    _eventStream.put(static_cast<uint8_t>((genericEvent.data & 0xff00000000000000) >> 56));
                    _previousTimestamp = genericEvent.timestamp;
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
                _eventStream.write("Event Stream", 12);
                _eventStream.put(1);
                _eventStream.put(0);
                _eventStream.put(4);
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

    /// HandleGenericByte implements the event stream state machine for generic events.
    class HandleGenericByte {
        public:
            HandleGenericByte() :
                _state(State::idle),
                _genericEvent(GenericEvent{0, 0, false})
            {
            }
            HandleGenericByte(const HandleGenericByte&) = default;
            HandleGenericByte(HandleGenericByte&&) = default;
            HandleGenericByte& operator=(const HandleGenericByte&) = default;
            HandleGenericByte& operator=(HandleGenericByte&&) = default;
            virtual ~HandleGenericByte() {}

            /// operator() handles a byte.
            virtual bool operator()(uint8_t byte, GenericEvent& genericEvent) {
                switch (_state) {
                    case State::idle: {
                        if ((byte & 0b1111111) == 0b1111111) {
                            _genericEvent.timestamp += ((byte & 0b10000000) >> 7) * 127;
                        } else {
                            _genericEvent.timestamp = _genericEvent.timestamp + (byte & 0b1111111);
                            _genericEvent.extraBit = ((byte & 0b10000000) >> 7) == 1;
                            _state = State::byte0;
                        }
                        return false;
                    }
                    case State::byte0: {
                        _genericEvent.data = byte;
                        _state = State::byte1;
                        return false;
                    }
                    case State::byte1: {
                        _genericEvent.data |= static_cast<uint64_t>(byte) << 8;
                        _state = State::byte2;
                        return false;
                    }
                    case State::byte2: {
                        _genericEvent.data |= static_cast<uint64_t>(byte) << 16;
                        _state = State::byte3;
                        return false;
                    }
                    case State::byte3: {
                        _genericEvent.data |= static_cast<uint64_t>(byte) << 24;
                        _state = State::byte4;
                        return false;
                    }
                    case State::byte4: {
                        _genericEvent.data |= static_cast<uint64_t>(byte) << 24;
                        _state = State::byte5;
                        return false;
                    }
                    case State::byte5: {
                        _genericEvent.data |= static_cast<uint64_t>(byte) << 24;
                        _state = State::byte6;
                        return false;
                    }
                    case State::byte6: {
                        _genericEvent.data |= static_cast<uint64_t>(byte) << 24;
                        _state = State::byte7;
                        return false;
                    }
                    case State::byte7: {
                        _genericEvent.data |= static_cast<uint64_t>(byte) << 24;
                        _state = State::byte8;
                        return false;
                    }
                    case State::byte8: {
                        _genericEvent.data |= static_cast<uint64_t>(byte) << 56;
                        _state = State::idle;
                        genericEvent = _genericEvent;
                        return true;
                    }
                }
            }

            /// reset initialises the state machine.
            virtual void reset() {
                _state = State::idle;
                _genericEvent.timestamp = 0;
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
                byte5,
                byte6,
                byte7,
                byte8,
            };

            State _state;
            GenericEvent _genericEvent;
    };

    /// GenericEventStreamObservable is a template-specialised EventStreamObservable for generic events.
    template <typename HandleEvent, typename HandleException, typename MustRestart>
    class GenericEventStreamObservable : public EventStreamObservable {
        public:
            GenericEventStreamObservable(
                const std::string& filename,
                HandleEvent handleEvent,
                HandleException handleException,
                MustRestart mustRestart,
                EventStreamObservable::Dispatch dispatch,
                const std::size_t& chunkSize
            ) :
                _eventStream(filename, std::ifstream::binary),
                _running(true)
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
                    if (versionMajor != 1 || versionMinor != 0) {
                        throw UnsupportedVersion(filename);
                    }
                    const auto mode = _eventStream.get();
                    if (_eventStream.eof() || mode != 4) {
                        throw UnsupportedMode(filename);
                    }
                }
                _loop = std::thread(
                    readAndDispatch<GenericEvent, HandleAtisByte, MustRestart, HandleEvent, HandleException>,
                    std::ref(_eventStream),
                    std::ref(_running),
                    dispatch,
                    chunkSize,
                    HandleGenericByte(),
                    std::forward<MustRestart>(mustRestart),
                    std::forward<HandleEvent>(handleEvent),
                    std::forward<HandleException>(handleException)
                );
            }
            GenericEventStreamObservable(const GenericEventStreamObservable&) = delete;
            GenericEventStreamObservable(GenericEventStreamObservable&&) = default;
            GenericEventStreamObservable& operator=(const GenericEventStreamObservable&) = delete;
            GenericEventStreamObservable& operator=(GenericEventStreamObservable&&) = default;
            virtual ~GenericEventStreamObservable() {
                _running.store(false, std::memory_order_relaxed);
                _loop.join();
            }

        protected:
            std::ifstream _eventStream;
            std::atomic_bool _running;
            std::thread _loop;
    };

    /// make_genericEventStreamObservable creates an event stream observable from functors.
    template<typename HandleEvent, typename HandleException, typename MustRestart = decltype(&falseFunction)>
    std::unique_ptr<GenericEventStreamObservable<HandleEvent, HandleException, MustRestart>> make_genericEventStreamObservable(
        const std::string& filename,
        HandleEvent handleEvent,
        HandleException handleException,
        MustRestart mustRestart = &falseFunction,
        EventStreamObservable::Dispatch dispatch = EventStreamObservable::Dispatch::synchronouslyButSkipOffset,
        std::size_t chunkSize = 1 << 10
    ) {
        return sepia::make_unique<AtisEventStreamObservable<HandleEvent, HandleException, MustRestart>>(
            filename,
            std::forward<HandleEvent>(handleEvent),
            std::forward<HandleException>(handleException),
            std::forward<MustRestart>(mustRestart),
            dispatch,
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
                auto integerPart = 0.0;
                if (_isInteger && std::modf(_value, &integerPart) != 0.0) {
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
            EnumParameter(const std::string& value, const std::unordered_set<std::string>& availableValues) :
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
                const std::size_t& fifoSize,
                const std::chrono::milliseconds& sleepDuration
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
            const std::chrono::milliseconds _sleepDuration;
            std::atomic<std::size_t> _head;
            std::atomic<std::size_t> _tail;
            std::vector<EventType> _events;
    };
}
