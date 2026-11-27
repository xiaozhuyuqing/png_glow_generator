#include "glow_converter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace yuqing {
namespace {

    using Image = png::image<png::rgba_pixel>;

    // 卷积场固定四个通道：预乘的红、绿、蓝，以及不透明度
    constexpr int kChannels = 4;

    png::byte ToByte(float value)
    {
        return static_cast<png::byte>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    }

    /**
     * @brief 把图像转成预乘的浮点场，卷积和叠加都直接在这个场上做
     */
    std::vector<float> ExtractPremultiplied(const Image &image)
    {
        const int width = static_cast<int>(image.get_width());
        const int height = static_cast<int>(image.get_height());
        std::vector<float> field(static_cast<std::size_t>(width) * height * kChannels, 0.0f);

        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                const png::rgba_pixel &pixel = image[row][col];
                const float alpha = pixel.alpha / 255.0f;
                const std::size_t index = (static_cast<std::size_t>(row) * width + col) * kChannels;
                field[index + 0] = pixel.red / 255.0f * alpha;
                field[index + 1] = pixel.green / 255.0f * alpha;
                field[index + 2] = pixel.blue / 255.0f * alpha;
                field[index + 3] = alpha;
            }
        }
        return field;
    }

    /**
     * @brief 一维核的可分离卷积，越界部分按无色处理
     */
    std::vector<float> Blur(const std::vector<float> &source, int width, int height,
                            const std::vector<double> &kernel, bool horizontal)
    {
        std::vector<float> result(source.size(), 0.0f);
        const int radius = static_cast<int>(kernel.size() / 2);

        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                const std::size_t index = (static_cast<std::size_t>(row) * width + col) * kChannels;

                for (int offset = -radius; offset <= radius; ++offset) {
                    const int sampleRow = horizontal ? row : row + offset;
                    const int sampleCol = horizontal ? col + offset : col;
                    if (sampleRow < 0 || sampleRow >= height || sampleCol < 0 || sampleCol >= width)
                        continue;

                    const float weight = static_cast<float>(kernel[offset + radius]);
                    const std::size_t sampleIndex =
                        (static_cast<std::size_t>(sampleRow) * width + sampleCol) * kChannels;
                    for (int channel = 0; channel < kChannels; ++channel)
                        result[index + channel] += source[sampleIndex + channel] * weight;
                }
            }
        }
        return result;
    }

} // namespace

    GlowConverter::GlowConverter()
    {
        core_ = { 0.004433, 0.054006, 0.242036, 0.399050, 0.242036, 0.054006, 0.004433 };
    }

    GlowConverter &GlowConverter::SetInputFile(const std::string &input)
    {
        img_input = png::image<png::rgba_pixel>(input);
        return *this;
    }
    GlowConverter &GlowConverter::SetOutputFile(const std::string &output)
    {
        output_file_ = output;
        return *this;
    }

    /**
     * @brief 设置单色泛光使用的颜色，仅在 SimpleGlow 启用时生效
     */
    GlowConverter &GlowConverter::SetGlowColor(const png::rgb_pixel &glow_color)
    {
        glow_color_ = glow_color;
        return *this;
    }
    GlowConverter &GlowConverter::SetGlowColor(double h, double s, double v)
    {
        h = std::fmod(h, 360.0) / 60.0;
        const double c = v * s;
        const double x = c * (1.0 - std::fabs(std::fmod(h, 2.0) - 1.0));
        const double m = v - c;

        double r = 0, g = 0, b = 0;
        if (h < 1) {
            r = c; g = x;
        } else if (h < 2) {
            r = x; g = c;
        } else if (h < 3) {
            g = c; b = x;
        } else if (h < 4) {
            g = x; b = c;
        } else if (h < 5) {
            r = x; b = c;
        } else {
            r = c; b = x;
        }
        glow_color_ = png::rgb_pixel(static_cast<png::byte>(std::lround((r + m) * 255)),
                           static_cast<png::byte>(std::lround((g + m) * 255)),
                           static_cast<png::byte>(std::lround((b + m) * 255)));
        return *this;
    }

    /**
     * @brief 启用后泛光使用 SetGlowColor 设置的单色，否则使用原图色彩泛光
     */
    GlowConverter &GlowConverter::SimpleGlow()
    {
        simple_glow = true;
        return *this;
    }

    /**
     * @brief 卷积后将原图叠加上去，输出到目标文件（有则覆盖）
     * @brief - 假设原图留出了3个像素的边缘
     * @brief - 卷积假设边缘无色参与
     */
    void GlowConverter::Convert()
    {
        const int width = static_cast<int>(img_input.get_width());
        const int height = static_cast<int>(img_input.get_height());
        if (width <= 0 || height <= 0)
            return;

        constexpr float kGlowStrength = 2.0f;

        std::vector<float> glow = ExtractPremultiplied(img_input);
        glow = Blur(glow, width, height, core_, true);
        glow = Blur(glow, width, height, core_, false);

        const float glowRed = glow_color_.red / 255.0f;
        const float glowGreen = glow_color_.green / 255.0f;
        const float glowBlue = glow_color_.blue / 255.0f;

        png::image<png::rgba_pixel> output(width, height);
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                const png::rgba_pixel &pixel = img_input[row][col];
                const std::size_t index = (static_cast<std::size_t>(row) * width + col) * kChannels;

                const float sourceAlpha = pixel.alpha / 255.0f;
                const float sourceRed = pixel.red / 255.0f * sourceAlpha;
                const float sourceGreen = pixel.green / 255.0f * sourceAlpha;
                const float sourceBlue = pixel.blue / 255.0f * sourceAlpha;

                float glowAlpha = std::min(glow[index + 3] * kGlowStrength, 1.0f);
                float glowRedTerm = glow[index + 0] * kGlowStrength;
                float glowGreenTerm = glow[index + 1] * kGlowStrength;
                float glowBlueTerm = glow[index + 2] * kGlowStrength;
                if (simple_glow) {
                    glowRedTerm = glowRed * glowAlpha;
                    glowGreenTerm = glowGreen * glowAlpha;
                    glowBlueTerm = glowBlue * glowAlpha;
                }

                const float outAlpha = sourceAlpha + glowAlpha * (1.0f - sourceAlpha);
                if (outAlpha <= 0.0f) {
                    output[row][col] = png::rgba_pixel(0, 0, 0, 0);
                    continue;
                }

                const float outRed = (sourceRed + glowRedTerm * (1.0f - sourceAlpha)) / outAlpha;
                const float outGreen = (sourceGreen + glowGreenTerm * (1.0f - sourceAlpha)) / outAlpha;
                const float outBlue = (sourceBlue + glowBlueTerm * (1.0f - sourceAlpha)) / outAlpha;
                output[row][col] = png::rgba_pixel(ToByte(outRed), ToByte(outGreen), ToByte(outBlue),
                                                   ToByte(outAlpha));
            }
        }
        output.write(output_file_);
    }
} // namespace yuqing
