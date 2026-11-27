#include <png.h>

#include <cstdio>

int main() {
    const unsigned version = png_access_version_number();
    std::printf("Hello, world\n");
    std::printf("libpng %u.%u.%u\n", version / 10000, version % 10000 / 100, version % 100);
    return 0;
}
