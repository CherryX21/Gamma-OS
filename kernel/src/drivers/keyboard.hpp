#pragma once

namespace gamma
{

    // Ініціалізує обробник клавіатури PS/2 (IRQ1) та кільцевий буфер символів.
    void keyboard_init();

    // Неблокуюче читання символу з буфера (повертає 0, якщо буфер порожній).
    char keyboard_getchar_nonblock();

    // Блокуюче очікування наступного символу (використовує hlt для економії CPU).
    char keyboard_getchar();

} // namespace gamma
