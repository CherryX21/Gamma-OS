#pragma once

#include "text_sink.hpp"

namespace gamma {

// Драйвер послідовного порту COM1 (UART 16550), використовується для журналу ядра.
class Serial : public TextSink {
public:
    constexpr Serial() = default;
    void init();
    void put_char(char c) override;
};

extern Serial g_serial;

}  // namespace gamma
