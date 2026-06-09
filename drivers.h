// drivers.h - Bare metal driver interface for FWC

#ifndef DRIVERS_H
#define DRIVERS_H

#include <string.h>
#include <stdint.h>

// ===== Serial Driver (COM1) =====
void serial_init(void);
void serial_putc(char ch);
void serial_write_string(const char *str);
void serial_write_char(char ch);
int serial_has_data(void);
int serial_getc(void);

// ===== Timer Driver (PIT) =====
void timer_init(void);
unsigned long timer_get_ticks(void);
void timer_increment_ticks(void);
void timer_sleep_ms(unsigned long ms);
unsigned long timer_read(void);

// ===== PIC Driver (8259) =====
void pic_init(void);
void pic_send_eoi(unsigned char irq);
void pic_enable_irq(unsigned char irq);
void pic_disable_irq(unsigned char irq);

// ===== PS/2 Keyboard Driver =====
void ps2_init(void);
void ps2_interrupt_handler(void);
int ps2_has_key(void);
unsigned char ps2_read_raw_scancode(void);
char ps2_read_char(void);
char ps2_getc(void);

// ===== IDT (Interrupt Descriptor Table) =====
void idt_init(void);
void register_interrupt_handler(uint8_t irq, void (*handler)(void));

// ===== ATA/IDE Driver =====
void ata_init(void);
int ata_read_sector(uint32_t lba, unsigned char *buf);
int ata_write_sector(uint32_t lba, unsigned char *buf);

#endif // DRIVERS_H
