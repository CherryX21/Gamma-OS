#include "serial.hpp"

#include "io.hpp"

namespace gamma {

Serial g_serial;

namespace {
constexpr uint16_t kCom1 = 0x3F8;
}

void Serial::init() {
    outb(kCom1 + 1, 0x00);  // вимкнути переривання UART
    outb(kCom1 + 3, 0x80);  // DLAB = 1
    outb(kCom1 + 0, 0x03);  // дільник = 3 (38400 бод)
    outb(kCom1 + 1, 0x00);
    outb(kCom1 + 3, 0x03);  // 8 біт, без парності, 1 стоп-біт
    outb(kCom1 + 2, 0xC7);  // FIFO
    outb(kCom1 + 4, 0x0B);  // DTR, RTS, OUT2
}

void Serial::put_char(char c) {
    if (c == '\n') {
        put_char('\r');
    }
    while ((inb(kCom1 + 5) & 0x20) == 0) {
    }
    outb(kCom1, static_cast<uint8_t>(c));
}

}  // namespace gamma
