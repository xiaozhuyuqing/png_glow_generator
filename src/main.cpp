#include "glow_converter.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

constexpr const char *kConfigPath = "config/config.yml";

std::string Trim(const std::string &text)
{
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string Unquote(const std::string &text)
{
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
        return text.substr(1, text.size() - 2);
    return text;
}

std::string ExpandHome(const std::string &path)
{
    if (path.rfind("～", 0) == 0)
        throw std::runtime_error("配置里的路径以全角波浪号开头，请改成半角 ~");
    if (path.rfind("~", 0) != 0)
        return path;

    const char *home = std::getenv("HOME");
    if (home == nullptr)
        throw std::runtime_error("环境变量 HOME 未设置，无法展开 ~");
    return std::string(home) + path.substr(1);
}

std::map<std::string, std::string> ReadPairs(const std::string &path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("无法打开配置文件：" + path);

    std::map<std::string, std::string> pairs;
    std::string section;
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t content = line.find_first_not_of(" \t");
        if (content == std::string::npos || line[content] == '#')
            continue;

        const std::string text = line.substr(content);
        const std::size_t colon = text.find(':');
        if (colon == std::string::npos)
            continue;

        const std::string key = Trim(text.substr(0, colon));
        const std::string value = Trim(text.substr(colon + 1));
        const bool nested = content > 0;
        if (!nested && value.empty()) {
            section = key;
            continue;
        }
        pairs[(nested ? section + "." : std::string()) + key] = value;
    }
    return pairs;
}

std::string Get(const std::map<std::string, std::string> &pairs, const std::string &key,
                const std::string &fallback)
{
    const auto found = pairs.find(key);
    return found == pairs.end() ? fallback : found->second;
}

struct Config {
    bool simple = false;
    double hue = 0.0;
    double saturation = 100.0;
    double value = 100.0;
    std::string input;
    std::string output;
};

Config LoadConfig()
{
    const auto pairs = ReadPairs(kConfigPath);

    Config config;
    config.simple = Get(pairs, "simple", "false") == "true";
    config.hue = std::stod(Get(pairs, "glow_color.h", "0"));
    config.saturation = std::stod(Get(pairs, "glow_color.s", "100"));
    config.value = std::stod(Get(pairs, "glow_color.v", "100"));
    config.input = ExpandHome(Unquote(Get(pairs, "input", "")));
    config.output = ExpandHome(Unquote(Get(pairs, "output", "")));

    if (config.input.empty() || config.output.empty())
        throw std::runtime_error("配置缺少 input 或 output");
    if (config.saturation < 0.0 || config.saturation > 100.0 ||
        config.value < 0.0 || config.value > 100.0)
        throw std::runtime_error("glow_color 的 s、v 应在 0 到 100 之间");
    return config;
}

int ConvertAll(const Config &config)
{
    if (!fs::is_directory(config.input))
        throw std::runtime_error("input 不是目录：" + config.input);

    fs::create_directories(config.output);

    int count = 0;
    for (const auto &entry : fs::directory_iterator(config.input)) {
        if (entry.path().extension() != ".png")
            continue;

        try {
            yuqing::GlowConverter converter;
            converter.SetInputFile(entry.path().string())
                     .SetOutputFile((fs::path(config.output) / entry.path().filename()).string())
                     .SetGlowColor(config.hue, config.saturation / 100.0, config.value / 100.0);
            if (config.simple)
                converter.SimpleGlow();
            converter.Convert();
            ++count;
        } catch (const std::exception &error) {
            std::cout << entry.path().string() << "：" << error.what() << "\n";
        }
    }
    return count;
}

} // namespace

int main()
{
    int count = 0;
    try {
        count = ConvertAll(LoadConfig());
    } catch (const std::exception &error) {
        std::cout << "错误：" << error.what() << "\n";
    }
    std::cout << "convert cnt: " << count << "\n";
    return 0;
}
