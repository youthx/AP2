#include <stdbool.h>
#include <stdint.h>

int main(void) {
    bool x = true;
    uint32_t y = 42;
    return x ? (int)y : 0;
}
