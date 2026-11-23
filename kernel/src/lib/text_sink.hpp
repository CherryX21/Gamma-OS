#pragma once

namespace gamma {

// Інтерфейс приймача текстового виводу. Реалізації: Serial, Console.
class TextSink {
public:
    virtual void put_char(char c) = 0;

protected:
    constexpr TextSink() = default;
    ~TextSink() = default;
};

}  // namespace gamma
