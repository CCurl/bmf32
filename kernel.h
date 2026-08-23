#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

/* Keyboard state shared across kernel and OS support code. */
extern int shift_pressed;
extern int ctrl_pressed;

/* Keyboard API */
int keyboard_has_input(void);
int keyboard_getchar(void);
int keyboard_read_char(void);
void keyboard_readline(char *buffer, int max_length);

/* Timer API */
void pit_init(uint32_t hz);
uint32_t get_ticks(void);

/* VGA API */
void vga_putchar(char c);
void vga_puts(const char *str);
void vga_clear(void);
void vga_set_cursor(int x, int y);

/* Serial API */
void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char *str);

/* Interrupt registration */
void register_interrupt_handler(uint8_t vector, void (*handler)(void));

#endif
