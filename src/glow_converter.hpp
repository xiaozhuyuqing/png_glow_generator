#pragma once
#include <png++/png.hpp>

namespace yuqing {
    class GlowConverter {
    public:
        GlowConverter();
        GlowConverter& SetInputFile(const std::string& input);
        GlowConverter& SetOutputFile(const std::string& output);
        GlowConverter& SetGlowColor(const png::rgb_pixel& glow_color);
        GlowConverter& SetGlowColor(double h, double s, double v);
        GlowConverter& SimpleGlow();
        void Convert();
    private:
        png::image<png::rgba_pixel> img_input;
        std::string output_file_;
        bool simple_glow{false};
        png::rgb_pixel glow_color_;
        std::vector<double> core_;
    };
} // namespace yuqing