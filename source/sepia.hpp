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
        int64_t timestamp;

        /// isExposureMeasurement is false if the event is a change detection, and true if it is an exposure measurement.
        bool isExposureMeasurement;

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
        int64_t timestamp;

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
        int64_t timestamp;

        /// isSecond is false if the event is a first threshold crossing.
        bool isSecond;
    };

    /// UnreadableFile is thrown when an input file does not exist or is not readable.
    class UnreadableFile : public std::runtime_error {
        public:
            UnreadableFile(std::string filename) : std::runtime_error("The file " + filename + " could not be opened for reading") {}
    };

    /// UnwritableFile is thrown whenan output file is not writable.
    class UnwritableFile : public std::runtime_error {
        public:
            UnwritableFile(std::string filename) : std::runtime_error("The file " + filename + " could not be opened for writing") {}
    };

    /// WrongSignature is thrown when an input file does not have the expected signature.
    class WrongSignature : public std::runtime_error {
        public:
            WrongSignature() : std::runtime_error("The input does not have the expected signature") {}
    };

    /// EndOfFile is thrown when the end of an input file is reached.
    class EndOfFile : public std::runtime_error {
        public:
            EndOfFile() : std::runtime_error("End of file reached") {}
    };

    /// NoDeviceConnected is thrown when device auto-select is called without devices connected.
    class NoDeviceConnected : public std::runtime_error {
        public:
            NoDeviceConnected(std::string deviceFamily) : std::runtime_error("No " + deviceFamily + " is connected") {}
    };

    /// DeviceDisconnected is thrown when an active device is disonnected.
    class DeviceDisconnected : public std::runtime_error {
        public:
            DeviceDisconnected(std::string deviceName) : std::runtime_error(deviceName + " disconnected") {}
    };

    /// ParseError is thrown when a JSON parse error occurs.
    class ParseError : public std::runtime_error {
        public:
            ParseError(std::string what, uint32_t line) : std::runtime_error("JSON parse error: " + what + " (line " + std::to_string(line) + ")") {}
    };

    /// ParameterError is a logical error regarding a parameter.
    class ParameterError : public std::logic_error {
        public:
            ParameterError(std::string what) : std::logic_error(what) {}
    };

    /// Log writes the events to a file.
    template <typename Compress>
    class Log {
        public:
            Log(Compress compress):
                _compress(std::forward<Compress>(compress)),
                _logging(false)
            {
                _accessingLog.clear(std::memory_order_release);
            }
            Log(const Log&) = delete;
            Log(Log&&) = default;
            Log& operator=(const Log&) = delete;
            Log& operator=(Log&&) = default;
            virtual ~Log() {}

            /// operator() handles an event.
            virtual void operator()(Event event) {
                const auto bytes = _compress(event);
                while (_accessingLog.test_and_set(std::memory_order_acquire)) {}
                if (_logging) {
                    _log.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                }
                _accessingLog.clear(std::memory_order_release);
            }

            /// writeTo is a thread-safe method to start logging the event stream to the given file.
            virtual void writeTo(std::string filename) {
                _log.open(filename);
                if (!_log.good()) {
                    throw UnwritableFile(filename);
                }
                while (_accessingLog.test_and_set(std::memory_order_acquire)) {}
                if (_logging) {
                    _accessingLog.clear(std::memory_order_release);
                    throw std::runtime_error("Already logging");
                }
                _log.write(reinterpret_cast<const char*>(getSignature().data()), getSignature().size());
                _logging = true;
                _accessingLog.clear(std::memory_order_release);
            }

            /// stop is a thread-safe method to stop logging the events and close the file.
            virtual void stop() {
                while (_accessingLog.test_and_set(std::memory_order_acquire)) {}
                if (!_logging) {
                    _accessingLog.clear(std::memory_order_release);
                    throw std::runtime_error("Was not logging");
                }
                _logging = false;
                _log.close();
                _accessingLog.clear(std::memory_order_release);
            }

            /// getSignature returns the log event handler signature.
            virtual std::string getSignature() const = 0;

        protected:
            Compress _compress;
            std::ofstream _log;
            bool _logging;
            std::atomic_flag _accessingLog;
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
                if (event.isExposureMeasurement) {
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

    /// Observable represents an entity that generates events.
    class Observable {
        public:
            Observable() = default;
            Observable(const Observable&) = delete;
            Observable(Observable&&) = default;
            Observable& operator=(const Observable&) = delete;
            Observable& operator=(Observable&&) = default;
            virtual ~Observable() {}
    };

    /// SpecialisedObservable represents a template-specialised entity that generates events.
    template <typename HandleEvent, typename HandleException>
    class SpecialisedObservable : public virtual Observable {
        public:
            SpecialisedObservable(HandleEvent handleEvent, HandleException handleException) :
                Observable(),
                _handleEvent(std::forward<HandleEvent>(handleEvent)),
                _handleException(std::forward<HandleException>(handleException))
            {
            }
            SpecialisedObservable(const SpecialisedObservable&) = delete;
            SpecialisedObservable(SpecialisedObservable&&) = default;
            SpecialisedObservable& operator=(const SpecialisedObservable&) = delete;
            SpecialisedObservable& operator=(SpecialisedObservable&&) = default;
            virtual ~SpecialisedObservable() {}

        protected:
            HandleEvent _handleEvent;
            HandleException _handleException;
    };

    /// LogObservable is a log file reader.
    /// The events are read as fast as possible.
    template <typename HandleEvent, typename HandleException, typename Expand>
    class LogObservable : public SpecialisedObservable<HandleEvent, HandleException> {
        public:
            LogObservable(
                HandleEvent handleEvent,
                HandleException handleException,
                std::string filename,
                Expand expand,
                std::string signature,
                std::size_t eventLength,
                bool slowDownToRealTime = false,
                std::function<bool()> mustRestart = []() -> bool {
                    return false;
                }
            ) :
                SpecialisedObservable<HandleEvent, HandleException>(
                    std::forward<HandleEvent>(handleEvent),
                    std::forward<HandleException>(handleException)
                ),
                _initialExpand(expand),
                _expand(std::forward<Expand>(expand)),
                _log(filename),
                _running(true),
                _mustRestart(std::move(mustRestart))
            {
                if (!_log.good()) {
                    throw UnreadableFile(filename);
                }

                const auto readSignature = std::string(signature);
                _log.read(const_cast<char*>(readSignature.data()), readSignature.length());
                if (_log.eof() || readSignature != signature) {
                    throw WrongSignature();
                }

                _loop = std::thread([this, eventLength, slowDownToRealTime, signature]() -> void {
                    auto readBytes = std::vector<unsigned char>(eventLength);
                    auto event = Event{};
                    try {
                        if (slowDownToRealTime) {
                            auto timeReference = std::chrono::system_clock::now();
                            while (_running.load(std::memory_order_relaxed)) {
                                _log.read(const_cast<char*>(reinterpret_cast<const char*>(readBytes.data())), readBytes.size());
                                if (_log.eof()) {
                                    if (_mustRestart()) {
                                        _log.clear();
                                        _log.seekg(signature.length(), _log.beg);
                                        _expand = _initialExpand;
                                        timeReference = std::chrono::system_clock::now();
                                        continue;
                                    } else {
                                        throw EndOfFile();
                                    }
                                }
                                if (_expand(readBytes, event)) {
                                    std::this_thread::sleep_until(timeReference + std::chrono::microseconds(event.timestamp));
                                    this->_handleEvent(event);
                                }
                            }
                        } else {
                            while (_running.load(std::memory_order_relaxed)) {
                                _log.read(const_cast<char*>(reinterpret_cast<const char*>(readBytes.data())), readBytes.size());
                                if (_log.eof()) {
                                    if (_mustRestart()) {
                                        _log.clear();
                                        _log.seekg(signature.length(), _log.beg);
                                        _expand = _initialExpand;
                                        continue;
                                    } else {
                                        throw EndOfFile();
                                    }
                                }
                                if (_expand(readBytes, event)) {
                                    this->_handleEvent(event);
                                }
                            }
                        }
                    } catch (...) {
                        SpecialisedObservable<HandleEvent, HandleException>::_handleException(std::current_exception());
                    }
                });
            }
            LogObservable(const LogObservable&) = delete;
            LogObservable(LogObservable&&) = default;
            LogObservable& operator=(const LogObservable&) = delete;
            LogObservable& operator=(LogObservable&&) = default;
            virtual ~LogObservable() {}

        protected:
            Expand _initialExpand;
            Expand _expand;
            std::ifstream _log;
            std::atomic_bool _running;
            std::thread _loop;
            std::function<bool()> _mustRestart;
    };

    /// CircularFifo is a thread-safe circular container.
    /// It is used to create a buffer stored on the computer's memory, which can therefore be larger than the camera's one.
    class CircularFifo {
        public:
            CircularFifo(std::size_t size) :
                _size(size),
                _head(0),
                _tail(0)
            {
                _events.resize(_size);
            }
            CircularFifo(const CircularFifo&) = delete;
            CircularFifo(CircularFifo&&) = default;
            CircularFifo& operator=(const CircularFifo&) = delete;
            CircularFifo& operator=(CircularFifo&&) = default;
            virtual ~CircularFifo() {}

            /// push adds an event to the circular FIFO in a thread-safe manner, as long as one thread only is writting.
            /// If the event could not be inserted (FIFO full), false is returned.
            virtual bool push(Event event) {
                const auto currentTail = _tail.load(std::memory_order_relaxed);
                const auto nextTail = increment(currentTail);
                if (nextTail != _head.load(std::memory_order_acquire)) {
                    _events[currentTail] = event;
                    _tail.store(nextTail, std::memory_order_release);
                    return true;
                }
                return false;
            }

            /// pop remove an event from the circular FIFO in a thread-safe manner, as long as one thread only is reading.
            /// If the event could not be retrieved (FIFO empty), false is returned.
            virtual bool pop(Event& event) {
                const auto currentHead = _head.load(std::memory_order_relaxed);
                if (currentHead == _tail.load(std::memory_order_acquire)) {
                    return false;
                }
                event = _events[currentHead];
                _head.store(increment(currentHead), std::memory_order_release);
                return true;
            }

        protected:
            /// increment returns the FIFO next index.
            std::size_t increment(const std::size_t& index) const {
                return (index + 1) % _size;
            }

            std::size_t _size;
            std::atomic<std::size_t> _head;
            std::atomic<std::size_t> _tail;
            std::vector<Event> _events;
    };

    /// Camera represents a generic event-based camera.
    class Camera : public virtual Observable {
        public:
            Camera() : Observable() {}
            Camera(const Camera&) = delete;
            Camera(Camera&&) = default;
            Camera& operator=(const Camera&) = delete;
            Camera& operator=(Camera&&) = default;
            virtual ~Camera() {}
    };

    /// SpecialisedCamera represents a template-specialised generic event-based camera.
    template <typename HandleEvent, typename HandleException>
    class SpecialisedCamera : public virtual Camera, public SpecialisedObservable<HandleEvent, HandleException>, public CircularFifo {
        public:
            SpecialisedCamera(
                HandleEvent handleEvent,
                HandleException handleException,
                std::size_t fifoSize = 1 << 24,
                std::chrono::milliseconds sleepDuration = std::chrono::milliseconds(10)
            ) :
                Camera(),
                SpecialisedObservable<HandleEvent, HandleException>(
                    std::forward<HandleEvent>(handleEvent),
                    std::forward<HandleException>(handleException)
                ),
                CircularFifo(fifoSize),
                _bufferRunning(true),
                _sleepDuration(sleepDuration)
            {
                _bufferLoop = std::thread([this]() -> void {
                    try {
                        auto event = Event{};
                        while (_bufferRunning.load(std::memory_order_relaxed)) {
                            if (pop(event)) {
                                this->_handleEvent(event);
                            } else {
                                std::this_thread::sleep_for(_sleepDuration);
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

        protected:
            std::thread _bufferLoop;
            std::atomic_bool _bufferRunning;
            std::chrono::milliseconds _sleepDuration;
    };

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

            /// hasCharacter determines wheter a character is a white space.
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
                std::string key = std::string();
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
                std::string key = std::string();
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
                for (; hasCharacter(separationCharacters, *end) && hasCharacter(whitespaceCharacters, *end); ++end) {
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
                for (; hasCharacter(separationCharacters, *end) && hasCharacter(whitespaceCharacters, *end); ++end) {
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
            StringParameter(std::string value) :
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
            UnvalidatedParameter(std::string jsonFilename) :
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
}
