#pragma once

#include <stdint.h>

#include "limine.h"
#include "text_sink.hpp"

namespace gamma {

// Текстова консоль поверх лінійного framebuffer (32 біти на піксель, шрифт 8x8 із масштабуванням).
class Console : public TextSink {
public:
    constexpr Console() = default;

    // Повертає false, якщо формат framebuffer не підтримується.
    bool init(const limine_framebuffer* fb, unsigned scale = 2);
    void put_char(char c) override;
    void clear();
    // Кольори задаються у форматі 0xRRGGBB.
    void set_colors(uint32_t fg_rgb, uint32_t bg_rgb);

    unsigned columns() const { return cols_; }
    unsigned rows() const { return rows_; }

private:
    uint32_t pack(uint32_t rgb) const;
    void draw_glyph(unsigned col, unsigned row, char c);
    void fill_rows(unsigned first_pixel_row, unsigned pixel_rows);
    void new_line();
    void scroll_up();

    uint8_t* base_ = nullptr;
    uint64_t pitch_ = 0;
    unsigned scale_ = 1;
    unsigned cell_w_ = 8;
    unsigned cell_h_ = 8;
    unsigned cols_ = 0;
    unsigned rows_ = 0;
    unsigned cur_col_ = 0;
    unsigned cur_row_ = 0;
    uint8_t red_size_ = 8, red_shift_ = 16;
    uint8_t green_size_ = 8, green_shift_ = 8;
    uint8_t blue_size_ = 8, blue_shift_ = 0;
    uint32_t fg_ = 0xD0D0D0;
    uint32_t bg_ = 0x000000;
};

extern Console g_console;

}  // namespace gamma
