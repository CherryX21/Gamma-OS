#pragma once

#include <stdint.h>

namespace gamma
{

    // Ініціалізує PIT на задану частоту (наприклад, 100 Гц = 10 мс на тік) та розмасковує IRQ0.
    void timer_init(uint32_t frequency_hz);

    // Кількість тіків з моменту запуску таймера.
    uint64_t timer_ticks();

    // Кількість секунд роботи системи (для команди uptime).
    uint64_t timer_uptime_seconds();

} // namespace gamma
