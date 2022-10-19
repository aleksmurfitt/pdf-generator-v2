#define POINTERHOLDER_TRANSITION 3
#include "face.hpp"

#include "font.hpp"
#include "fontManager.hpp"
#include "pdf.hpp"

#include <qpdf/QUtil.hh>

#include <iostream>
using namespace PDFLib;
Face::Face(FaceHolder face, double scale, std::string handle)
    : glyphSet{subsetInput},
      charSet{},
      face{std::move(face)},
      scale{scale},
      handle{handle} {
    hb_subset_input_set_flags(subsetInput, HB_SUBSET_FLAGS_RETAIN_GIDS);
};

double Face::getAscender() {
    hb_font_extents_t extents;
    hb_font_get_h_extents(face, &extents);
    return static_cast<double>(extents.ascender) * scale;
}

double Face::getDescender() {
    hb_font_extents_t extents;
    hb_font_get_h_extents(face, &extents);
    return static_cast<double>(extents.descender) * scale;
}
double Face::getLineGap() {
    hb_font_extents_t extents;
    hb_font_get_h_extents(face, &extents);
    return static_cast<double>(extents.line_gap) * scale;
}

float Face::getItalicAngle() {
    return hb_style_get_value(face, HB_STYLE_TAG_SLANT_ANGLE);
}

int Face::getCapHeight() {
    int32_t out;
    hb_ot_metrics_get_position(face, HB_OT_METRICS_TAG_CAP_HEIGHT, &out);
    return out * scale;
}

std::pair<float, std::string> Face::shape(std::string_view text, float points, hb_script_t script,
                                          hb_direction_t direction, std::string_view language) {
    BufferHolder buffer;
    hb_buffer_add_utf8(buffer, text.begin(), -1, 0, -1);
    hb_buffer_set_direction(buffer, direction);
    hb_buffer_set_script(buffer, script);
    hb_buffer_set_language(buffer, hb_language_from_string(language.begin(), -1));
    hb_shape(face, buffer, NULL, 0);

    auto toHex = [](uint32_t w, size_t hex_len = 4) -> std::string {
        static const char *digits = "0123456789ABCDEF";
        std::string rc(hex_len, '0');
        for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
            rc[i] = digits[(w >> j) & 0x0f];
        return rc;
    };

    unsigned int glyph_count;
    hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
    for (unsigned int i = 0; i < glyph_count; i++) {
        if (glyph_info[i].codepoint == 0)
            throw std::runtime_error("Required glyph not defined");
    }
    hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buffer, &glyph_count);
    float width = 0;
    float height = getAscender() - getDescender();
    std::string out;
    out.reserve(text.length() * 3);
    out.push_back('[');
    out.push_back('<');
    for (unsigned int i = 0; i < glyph_count; i++) {
        hb_codepoint_t glyphid = glyph_info[i].codepoint;
        hb_position_t x_advance = glyph_pos[i].x_advance;
        hb_position_t error = hb_font_get_glyph_h_advance(face, glyphid) - x_advance;
        hb_set_add(glyphSet, glyphid);
        width += x_advance;
        out += toHex(glyphid);
        if (error) {
            out.push_back('>');
            out += std::to_string((int32_t)(error * scale));
            out.push_back('<');
        }
    }
    out += ">]";
    return std::make_pair(width * scale * points / 1000.0, out);
};
