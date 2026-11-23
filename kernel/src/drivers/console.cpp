#include "console.hpp"

#include <stddef.h>
#include "kstring.hpp"

#include "font8x8_basic.h"

namespace gamma {

Console g_console;

bool Console::init(const limine_framebuffer* fb, unsigned scale) {
    if (fb == nullptr || fb->address == nullptr || fb->bpp != 32 || scale == 0) {
        return false;
    }
    base_ = static_cast<uint8_t*>(fb->address);
    pitch_ = fb->pitch;
    scale_ = scale;
    cell_w_ = 8 * scale;
    cell_h_ = 8 * scale;
    cols_ = static_cast<unsigned>(fb->width / cell_w_);
    rows_ = static_cast<unsigned>(fb->height / cell_h_);
    red_size_ = fb->red_mask_size;
    red_shift_ = fb->red_mask_shift;
    green_size_ = fb->green_mask_size;
    green_shift_ = fb->green_mask_shift;
    blue_size_ = fb->blue_mask_size;
    blue_shift_ = fb->blue_mask_shift;
    if (cols_ == 0 || rows_ == 0) {
        return false;
    }
    clear();
    return true;
}

uint32_t Console::pack(uint32_t rgb) const {
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8) & 0xFF;
    uint32_t b = rgb & 0xFF;
    return ((r >> (8 - red_size_)) << red_shift_) | ((g >> (8 - green_size_)) << green_shift_) |
           ((b >> (8 - blue_size_)) << blue_shift_);
}

void Console::set_colors(uint32_t fg_rgb, uint32_t bg_rgb) {
    fg_ = fg_rgb;
    bg_ = bg_rgb;
}

void Console::fill_rows(unsigned first_pixel_row, unsigned pixel_rows) {
    const uint32_t color = pack(bg_);
    const unsigned width = cols_ * cell_w_;
    for (unsigned y = 0; y < pixel_rows; y++) {
        uint32_t* line = reinterpret_cast<uint32_t*>(base_ + (first_pixel_row + y) * pitch_);
        for (unsigned x = 0; x < width; x++) {
            line[x] = color;
        }
    }
}

void Console::clear() {
    fill_rows(0, rows_ * cell_h_);
    cur_col_ = 0;
    cur_row_ = 0;
}

void Console::draw_glyph(unsigned col, unsigned row, char c) {
    unsigned char code = static_cast<unsigned char>(c);
    if (code >= 128) {
        code = '?';
    }
    const uint32_t fg = pack(fg_);
    const uint32_t bg = pack(bg_);
    const unsigned x0 = col * cell_w_;
    const unsigned y0 = row * cell_h_;

    for (unsigned gy = 0; gy < 8; gy++) {
        const uint8_t bits = static_cast<uint8_t>(font8x8_basic[code][gy]);
        for (unsigned sy = 0; sy < scale_; sy++) {
            uint32_t* line =
                reinterpret_cast<uint32_t*>(base_ + (y0 + gy * scale_ + sy) * pitch_) + x0;
            for (unsigned gx = 0; gx < 8; gx++) {
                const uint32_t color = ((bits >> gx) & 1) ? fg : bg;
                for (unsigned sx = 0; sx < scale_; sx++) {
                    line[gx * scale_ + sx] = color;
                }
            }
        }
    }
}

void Console::scroll_up() {
    const size_t band = static_cast<size_t>(cell_h_) * pitch_;
    memmove(base_, base_ + band, band * (rows_ - 1));
    fill_rows((rows_ - 1) * cell_h_, cell_h_);
}

void Console::new_line() {
    cur_col_ = 0;
    if (cur_row_ + 1 >= rows_) {
        scroll_up();
    } else {
        cur_row_++;
    }
}

void Console::put_char(char c) {
    if (base_ == nullptr) {
        return;
    }
    switch (c) {
        case '\n':
            new_line();
            return;
        case '\r':
            cur_col_ = 0;
            return;
        case '\b':
            if (cur_col_ > 0) {
                cur_col_--;
                draw_glyph(cur_col_, cur_row_, ' ');
            }
            return;
        case '\t': {
            unsigned next = (cur_col_ + 8) & ~7u;
            while (cur_col_ < next && cur_col_ < cols_) {
                draw_glyph(cur_col_++, cur_row_, ' ');
            }
            if (cur_col_ >= cols_) {
                new_line();
            }
            return;
        }
        default:
            break;
    }
    if (cur_col_ >= cols_) {
        new_line();
    }
    draw_glyph(cur_col_++, cur_row_, c);
}

}  // namespace gamma
