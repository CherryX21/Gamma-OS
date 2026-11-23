#include "interrupts.hpp"
#include "io.hpp"

namespace gamma
{

    namespace
    {
        constexpr uint16_t kPic1Command = 0x20;
        constexpr uint16_t kPic1Data = 0x21;
        constexpr uint16_t kPic2Command = 0xA0;
        constexpr uint16_t kPic2Data = 0xA1;
        constexpr uint8_t kPicEoi = 0x20;
    } // namespace

    void pic_init_masked()
    {
        outb(kPic1Command, 0x11);
        outb(kPic2Command, 0x11);
        outb(kPic1Data, 0x20);
        outb(kPic2Data, 0x28);
        outb(kPic1Data, 0x04);
        outb(kPic2Data, 0x02);
        outb(kPic1Data, 0x01);
        outb(kPic2Data, 0x01);
        outb(kPic1Data, 0xFF);
        outb(kPic2Data, 0xFF);
    }

    void pic_unmask(uint8_t irq)
    {
        uint16_t port = (irq < 8) ? kPic1Data : kPic2Data;
        uint8_t line = (irq < 8) ? irq : static_cast<uint8_t>(irq - 8);
        uint8_t mask = inb(port) & static_cast<uint8_t>(~(1u << line));
        outb(port, mask);
    }

    void pic_send_eoi(uint8_t irq)
    {
        if (irq >= 8)
        {
            outb(kPic2Command, kPicEoi);
        }
        outb(kPic1Command, kPicEoi);
    }

} // namespace gamma
