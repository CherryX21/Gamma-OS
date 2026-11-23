#include "timer.hpp"

#include "interrupts.hpp"
#include "io.hpp"

namespace gamma
{

    namespace
    {

        constexpr uint32_t kPitBaseFreq = 1193182;
        constexpr uint16_t kPitChannel0 = 0x40;
        constexpr uint16_t kPitCommand = 0x43;

        volatile uint64_t g_ticks = 0;
        uint32_t g_freq_hz = 100;

        void on_timer_irq(InterruptFrame *)
        {
            g_ticks++;
        }

    } // namespace

    void timer_init(uint32_t frequency_hz)
    {
        if (frequency_hz == 0)
        {
            frequency_hz = 100;
        }
        g_freq_hz = frequency_hz;
        const uint16_t divisor = static_cast<uint16_t>(kPitBaseFreq / frequency_hz);

        outb(kPitCommand, 0x36); // канал 0, lobyte/hibyte, режим 3 (square wave)
        outb(kPitChannel0, static_cast<uint8_t>(divisor & 0xFF));
        outb(kPitChannel0, static_cast<uint8_t>((divisor >> 8) & 0xFF));

        irq_register(0, on_timer_irq);
        pic_unmask(0);
    }

    uint64_t timer_ticks()
    {
        return g_ticks;
    }

    uint64_t timer_uptime_seconds()
    {
        return g_ticks / g_freq_hz;
    }

} // namespace gamma
