#include <stddef.h>
#include <stdint.h>
#include "kernel.h"
#include "dwc-vm.h"
#include "boot.h"

char tib[256];
void sys_load();

// ==================================================
void repl() {
    if (state != COMPILE) { state = INTERPRET; }
    zType((state == COMPILE) ? " ... "  : " ok\n");
    keyboard_readline(tib, sizeof(tib));
    emit(' ');
    outer(tib);
}

void dwcRun() {
    dwcInit();
    outer(DWC_SRC);
    outer(".\" Bare Metal Forth v\" .version cr");
    outer(".\" Hello\" cr");
    while (1) { repl(); }
}

// ==================================================
int strlen(const char *s) {
    int len = 0;
    while (s && s[len] != '\0') {
        len++;
    }
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *out = dest;
    while ((*dest++ = *src++) != '\0') {
    }
    return out;
}

int strEqI(const char *a, const char *b) {
    if (!a || !b) { return a == b; }
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') { ca = (unsigned char)(ca + 32); }
        if (cb >= 'A' && cb <= 'Z') { cb = (unsigned char)(cb + 32); }
        if (ca != cb) { return 0; }
        ++a; ++b;
    }
    return *a == *b;
}

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dest;
}

void *memset(void *s, int c, int n) {
    uint8_t *p = (uint8_t *)s;
    for (int i = 0; i < n; ++i) {
        p[i] = (uint8_t)c;
    }
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || n == 0) {
        return dest;
    }
    if (d < s) {
        for (size_t i = 0; i < n; ++i) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

int key(void) {
    int c = -1;
    while (c < 0) {
        c = keyboard_getchar();
    }
    return c;
}

int qKey(void) {
    return keyboard_has_input() ? 1 : 0;
}

void zType(const char *str) {
    if (str) {
        vga_puts(str);
        // serial_puts(str);
    }
}

void emit(const char ch) {
    vga_putchar(ch);
    // serial_putchar(ch);
}

int timer(void) {
    return (int)get_ticks();
}

void ms(int sleepForMS) {
    uint32_t start = get_ticks();
    uint32_t delay = (sleepForMS > 0) ? (uint32_t)sleepForMS : 0u;
    while ((get_ticks() - start) < delay) {
        /* busy wait until elapsed */
    }
}
