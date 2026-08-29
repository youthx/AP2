/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include <stdbool.h>
#include <stdint.h>

int main(void) {
    bool x = true;
    uint32_t y = 42;
    return x ? (int)y : 0;
}
