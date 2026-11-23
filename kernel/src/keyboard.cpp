#include "keyboard.hpp"

#include <stddef.h>
#include <stdint.h>

#include "interrupts.hpp"
#include "io.hpp"

namespace gamma
{

    namespace
    {

        constexpr uint16_t kKbdDataPort = 0x60;
        constexpr size_t kBufSize = 256;

        char g_buf[kBufSize];
        volatile size_t g_head = 0;
        volatile size_t g_tail = 0;
        bool g_shift = false;

        constexpr char kScancodeNormal[128] = {
            0,
            27,
            '1',
            '2',
            '3',
            '4',
            '5',
            '6',
            '7',
            '8',
            '9',
            '0',
            '-',
            '=',
            '\b',
            '\t',
            'q',
            'w',
            'e',
            'r',
            't',
            'y',
            'u',
            'i',
            'o',
            'p',
            '[',
            ']',
            '\n',
            0,
            'a',
            's',
            'd',
            'f',
            'g',
            'h',
            'j',
            'k',
            'l',
            ';',
            '\'',
            '`',
            0,
            '\\',
            'z',
            'x',
            'c',
            'v',
            'b',
            'n',
            'm',
            ',',
            '.',
            '/',
            0,
            '*',
            0,
            ' ',
            0,
        };

        constexpr char kScancodeShift[128] = {
            0,
            27,
            '!',
            '@',
            '#',
            '$',
            '%',
            '^',
            '&',
            '*',
            '(',
            ')',
            '_',
            '+',
            '\b',
            '\t',
            'Q',
            'W',
            'E',
            'R',
            'T',
            'Y',
            'U',
            'I',
            'O',
            'P',
            '{',
            '}',
            '\n',
            0,
            'A',
            'S',
            'D',
            'F',
            'G',
            'H',
            'J',
            'K',
            'L',
            ':',
            '"',
            '~',
            0,
            '|',
            'Z',
            'X',
            'C',
            'V',
            'B',
            'N',
            'M',
            '<',
            '>',
            '?',
            0,
            '*',
            0,
            ' ',
            0,
        };

        void push_char(char c)
        {
            const size_t next = (g_head + 1) % kBufSize;
            if (next != g_tail)
            {
                g_buf[g_head] = c;
                g_head = next;
            }
        }

        void on_keyboard_irq(InterruptFrame *)
        {
            const uint8_t sc = inb(kKbdDataPort);
            if (sc == 0x2A || sc == 0x36)
            {
                g_shift = true;
                return;
            }
            if (sc == 0xAA || sc == 0xB6)
            {
                g_shift = false;
                return;
            }
            if ((sc & 0x80) != 0)
            {
                return; // ігноруємо відпускання клавіш
            }
            const char c = g_shift ? kScancodeShift[sc] : kScancodeNormal[sc];
            if (c != 0)
            {
                push_char(c);
            }
        }

    } // namespace

    void keyboard_init()
    {
        while ((inb(0x64) & 1) != 0)
        {
            (void)inb(kKbdDataPort);
        }
        irq_register(1, on_keyboard_irq);
        pic_unmask(1);
    }

    char keyboard_getchar_nonblock()
    {
        if (g_head == g_tail)
        {
            return 0;
        }
        const char c = g_buf[g_tail];
        g_tail = (g_tail + 1) % kBufSize;
        return c;
    }

    char keyboard_getchar()
    {
        while (true)
        {
            const char c = keyboard_getchar_nonblock();
            if (c != 0)
            {
                return c;
            }
            asm volatile("hlt");
        }
    }

} // namespace gamma
