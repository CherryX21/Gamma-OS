#include "interrupts.hpp"
#include "io.hpp"

namespace gamma {

namespace {
constexpr uint16_t kPic1Command = 0x20;
constexpr uint16_t kPic1Data = 0x21;
constexpr uint16_t kPic2Command = 0xA0;
constexpr uint16_t kPic2Data = 0xA1;
}  // namespace

void pic_init_masked() {
    outb(kPic1Command, 0x11);  // ICW1: ініціалізація, потрібен ICW4
    outb(kPic2Command, 0x11);
    outb(kPic1Data, 0x20);     // ICW2: зміщення векторів ведучого PIC
    outb(kPic2Data, 0x28);     // ICW2: зміщення векторів веденого PIC
    outb(kPic1Data, 0x04);     // ICW3: ведений PIC підключено до лінії IRQ2
    outb(kPic2Data, 0x02);
    outb(kPic1Data, 0x01);     // ICW4: режим 8086
    outb(kPic2Data, 0x01);
    outb(kPic1Data, 0xFF);     // маскувати всі лінії
    outb(kPic2Data, 0xFF);
}

}  // namespace gamma
