// drivers.c - Consolidated bare metal driver implementation for BMF
// Combines: serial.c, timer.c, pic.c, ps2.c, string.c, idt.c

#include "drivers.h"
#include <stddef.h>

// ============================================================================
// SERIAL DRIVER (COM1)
// ============================================================================

#define COM1_PORT       0x3F8
#define COM1_DATA       (COM1_PORT + 0)
#define COM1_IER        (COM1_PORT + 1)   // Interrupt Enable Register
#define COM1_FCR        (COM1_PORT + 2)   // FIFO Control Register
#define COM1_LCR        (COM1_PORT + 3)   // Line Control Register
#define COM1_MCR        (COM1_PORT + 4)   // Modem Control Register
#define COM1_LSR        (COM1_PORT + 5)   // Line Status Register
#define COM1_MSR        (COM1_PORT + 6)   // Modem Status Register
#define COM1_DLL        (COM1_PORT + 0)   // Divisor Latch Low (when LCR bit 7 = 1)
#define COM1_DLH        (COM1_PORT + 1)   // Divisor Latch High (when LCR bit 7 = 1)

#define UART_BAUD_RATE  115200

static int serial_initialized = 0;

// Port I/O inline functions
static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void serial_init(void) {
    if (serial_initialized) return;
    
    // Set baud rate to 115200
    // Formula: divisor = 115200 / desired_baud
    // For 115200: divisor = 1
    unsigned short divisor = 1;
    
    // Set DLAB (Divisor Latch Access Bit) to access divisor registers
    outb(COM1_LCR, 0x80);
    
    // Set baud rate (divisor: 1 for 115200 baud at 1.8432 MHz clock)
    outb(COM1_DLL, divisor & 0xFF);
    outb(COM1_DLH, (divisor >> 8) & 0xFF);
    
    // Clear DLAB and set word length to 8 bits, 1 stop bit, no parity
    // LCR: 0x03 = 8N1
    outb(COM1_LCR, 0x03);
    
    // Enable FIFO, clear Tx/Rx buffers, set interrupt trigger level
    outb(COM1_FCR, 0xC7);
    
    // Set MCR: DTR, RTS, and OUT2 (needed for interrupts)
    outb(COM1_MCR, 0x0B);
    
    // Disable serial interrupts - we use polling for MVP
    // (we only enable data available interrupt here, but don't buffer the data)
    outb(COM1_IER, 0x00);
    
    serial_initialized = 1;
}

void serial_putc(char ch) {
    // Wait for transmit buffer to be empty
    while ((inb(COM1_LSR) & 0x20) == 0) {
        // Busy wait
    }
    outb(COM1_DATA, (unsigned char)ch);
}

void serial_write_string(const char *str) {
    while (*str) {
        serial_putc(*str++);
    }
}

int serial_has_data(void) {
    // Check if data is available in the receiver buffer
    return (inb(COM1_LSR) & 0x01) != 0;
}

int serial_getc(void) {
    // Wait for data to be available
    while (!serial_has_data()) {
        // Busy wait
    }
    return (int)inb(COM1_DATA);
}

// For Forth I/O redirection
void serial_write_char(char ch) {
    serial_putc(ch);
}

// ============================================================================
// TIMER DRIVER (PIT - Programmable Interval Timer)
// ============================================================================

#define PIT_CHANNEL_0   0x40
#define PIT_CHANNEL_1   0x41
#define PIT_CHANNEL_2   0x42
#define PIT_CTRL        0x43

#define PIT_FREQUENCY   1193182     // Base PIT frequency in Hz
#define TIMER_FREQ      1000        // Desired timer interrupt frequency (1000 Hz = 1ms ticks)
#define PIT_DIVISOR     (PIT_FREQUENCY / TIMER_FREQ)

volatile static unsigned long timer_ticks = 0;

void timer_init(void) {
    // Program PIT channel 0 for periodic interrupts
    // Control word: 0x34 = channel 0, access both bytes, mode 2 (rate generator)
    outb(PIT_CTRL, 0x34);
    
    // Send divisor (low byte then high byte)
    outb(PIT_CHANNEL_0, (unsigned char)(PIT_DIVISOR & 0xFF));
    outb(PIT_CHANNEL_0, (unsigned char)((PIT_DIVISOR >> 8) & 0xFF));
}

unsigned long timer_get_ticks(void) {
    return timer_ticks;
}

void timer_increment_ticks(void) {
    timer_ticks++;
}

void timer_sleep_ms(unsigned long ms) {
    unsigned long start = timer_ticks;
    unsigned long end = start + ms;
    
    while (timer_ticks < end) {
        // Busy wait - can be improved with halt instruction in real implementation
        asm volatile("hlt");
    }
}

// For Forth timer word
unsigned long timer_read(void) {
    return timer_ticks;
}

// ============================================================================
// PIC DRIVER (8259 Programmable Interrupt Controller)
// ============================================================================

#define PIC_MASTER_CMD      0x20
#define PIC_MASTER_DATA     0x21
#define PIC_SLAVE_CMD       0xA0
#define PIC_SLAVE_DATA      0xA1

#define ICW1_INIT           0x10    // Initialization command word
#define ICW1_ICW4           0x01    // ICW4 needed
#define ICW2_MASTER         0x20    // Master PIC base interrupt vector (IRQ0-7 map to INT 32-39)
#define ICW2_SLAVE          0x28    // Slave PIC base interrupt vector (IRQ8-15 map to INT 40-47)
#define ICW3_MASTER         0x04    // Master: IRQ2 connected to slave
#define ICW3_SLAVE          0x02    // Slave: Connected to IRQ2 of master
#define ICW4_8086           0x01    // 8086 mode

#define OCW1_IRQ0           0xFE    // Enable IRQ0 (timer), disable all others
#define OCW1_IRQ0_IRQ4      0xEE    // Enable IRQ0 and IRQ4 (serial), disable all others

void pic_init(void) {
    // Initialize master PIC
    outb(PIC_MASTER_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC_MASTER_DATA, ICW2_MASTER);    // Remap to INT 32-39
    outb(PIC_MASTER_DATA, ICW3_MASTER);    // IRQ2 = slave
    outb(PIC_MASTER_DATA, ICW4_8086);
    
    // Initialize slave PIC
    outb(PIC_SLAVE_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC_SLAVE_DATA, ICW2_SLAVE);      // Remap to INT 40-47
    outb(PIC_SLAVE_DATA, ICW3_SLAVE);      // Connected to master IRQ2
    outb(PIC_SLAVE_DATA, ICW4_8086);
    
    // Mask all interrupts except timer (IRQ0) and serial (IRQ4)
    // Interrupt mask: bits set to 1 = masked (disabled)
    outb(PIC_MASTER_DATA, OCW1_IRQ0_IRQ4);
    outb(PIC_SLAVE_DATA, 0xFF);             // Mask all slave interrupts for now
}

void pic_send_eoi(unsigned char irq) {
    // Send End-of-Interrupt (EOI) command to the appropriate PIC
    if (irq >= 8) {
        // IRQ from slave
        outb(PIC_SLAVE_CMD, 0x20);          // EOI to slave
    }
    // Always send to master (for IRQ 2-7 or as cascade)
    outb(PIC_MASTER_CMD, 0x20);             // EOI to master
}

void pic_enable_irq(unsigned char irq) {
    unsigned short port = (irq < 8) ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
    unsigned char mask = inb(port);
    mask &= ~(1 << (irq % 8));              // Clear bit to enable
    outb(port, mask);
}

void pic_disable_irq(unsigned char irq) {
    unsigned short port = (irq < 8) ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
    unsigned char mask = inb(port);
    mask |= (1 << (irq % 8));               // Set bit to disable
    outb(port, mask);
}

// ============================================================================
// PS/2 KEYBOARD DRIVER
// ============================================================================

#define PS2_DATA_PORT   0x60
#define PS2_CTRL_PORT   0x64

#define PS2_ACK         0xFA
#define PS2_RESEND      0xFE

// Simple unshifted ASCII lookup table for PS/2 scan codes
static const char scancode_to_ascii[] = {
    0,      // 0x00 - invalid
    27,     // 0x01 - ESC
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',  // 0x02-0x0B
    '-', '=', '\b',    // 0x0C-0x0E - minus, equals, backspace
    '\t',               // 0x0F - tab
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',  // 0x10-0x19
    '[', ']', '\n',     // 0x1A-0x1C - brackets, enter
    0,                  // 0x1D - left control (not printable)
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',        // 0x1E-0x26
    ';', '\'', '`',     // 0x27-0x29
    0,                  // 0x2A - left shift
    '\\',               // 0x2B - backslash
    'z', 'x', 'c', 'v', 'b', 'n', 'm',                  // 0x2C-0x32
    ',', '.', '/',      // 0x33-0x35
    0,                  // 0x36 - right shift
    '*',                // 0x37 - keypad multiply
    0,                  // 0x38 - left alt
    ' ',                // 0x39 - space
    0,                  // 0x3A - caps lock
    0,                  // 0x3B-0x44 - F1-F10 (not printable)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,                  // 0x45 - num lock
    0,                  // 0x46 - scroll lock
    // Keypad keys - simplified
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.',
};

#define SCANCODE_TABLE_SIZE (sizeof(scancode_to_ascii) / sizeof(scancode_to_ascii[0]))

static volatile int keyboard_ready = 0;
static volatile unsigned char last_scancode = 0;

void ps2_init(void) {
    // Initialize PS/2 controller (simplified for QEMU)
    // Disable PS/2 devices
    outb(PS2_CTRL_PORT, 0xAD);  // Disable keyboard
    outb(PS2_CTRL_PORT, 0xA7);  // Disable mouse (if present)
    
    // Flush output buffer
    inb(PS2_DATA_PORT);
    
    // Re-enable keyboard
    outb(PS2_CTRL_PORT, 0xAE);
    
    keyboard_ready = 1;
}

void ps2_interrupt_handler(void) {
    unsigned char status = inb(PS2_CTRL_PORT);
    
    // Check if data is from keyboard (bit 0 = output buffer full)
    if (status & 0x01) {
        unsigned char scancode = inb(PS2_DATA_PORT);
        
        // Filter out key releases (0xF0 prefix) for now
        if (scancode != 0xF0) {
            last_scancode = scancode;
            keyboard_ready = 1;
        }
    }
}

int ps2_has_key(void) {
    return keyboard_ready;
}

unsigned char ps2_read_raw_scancode(void) {
    while (!keyboard_ready) {
        asm volatile("hlt");
    }
    unsigned char code = last_scancode;
    keyboard_ready = 0;
    return code;
}

char ps2_read_char(void) {
    unsigned char scancode = ps2_read_raw_scancode();
    
    if (scancode < SCANCODE_TABLE_SIZE) {
        return scancode_to_ascii[scancode];
    }
    
    // Unknown scancode, return null
    return 0;
}

// Wait for keyboard and return ASCII character
// Returns 0 if no valid character can be produced from scan code
char ps2_getc(void) {
    while (1) {
        char ch = ps2_read_char();
        if (ch != 0) {
            return ch;
        }
    }
}

// ============================================================================
// STRING FUNCTIONS (libc replacements for bare metal)
// ============================================================================

// Compare strings case-insensitively (up to length n)
int strncasecmp(const char *s1, const char *s2, size_t n) {
    unsigned char c1, c2;
    while (n--) {
        c1 = *s1++;
        c2 = *s2++;
        if (c1 == 0 && c2 == 0) return 0;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
    }
    return 0;
}

// Compare strings case-insensitively
int strcasecmp(const char *s1, const char *s2) {
    unsigned char c1, c2;
    while (1) {
        c1 = *s1++;
        c2 = *s2++;
        if (c1 == 0 && c2 == 0) return 0;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        if (c1 != c2) return c1 - c2;
    }
}

// Get string length
size_t strlen(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

// Copy string
char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while (*src) *d++ = *src++;
    *d = 0;
    return dest;
}

// Copy n bytes
void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

// Move memory (handles overlapping regions)
void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

// Fill memory with byte
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

// System call - stub for bare metal
int system(const char *command) {
    // Not implemented for bare metal
    (void)command;
    return -1;
}

// ============================================================================
// IDT (Interrupt Descriptor Table)
// ============================================================================

#define IDT_SIZE 256

typedef struct {
    uint16_t offset_low;
    uint16_t segment;
    uint8_t  reserved;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed)) IDT_Entry;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) IDT_Ptr;

static IDT_Entry idt[IDT_SIZE];
static IDT_Ptr idt_ptr = {
    .limit = sizeof(idt) - 1,
    .base = (uint32_t)&idt
};

static void (*interrupt_handlers[256])(void) = {0};

// Forward declarations of ISR handlers (defined in idt.asm)
extern void isr_0(void);
extern void isr_1(void);
extern void isr_2(void);
extern void isr_3(void);
extern void isr_4(void);
extern void isr_5(void);
extern void isr_6(void);
extern void isr_7(void);
extern void isr_8(void);
extern void isr_10(void);
extern void isr_11(void);
extern void isr_12(void);
extern void isr_13(void);
extern void isr_14(void);
extern void isr_32(void);
extern void isr_33(void);
extern void isr_36(void);

// Implemented in idt.asm
extern void load_idt(IDT_Ptr *ptr);

// Generic interrupt handler (called from assembly)
void handle_interrupt(uint32_t irq_num) {
    if (interrupt_handlers[irq_num]) {
        interrupt_handlers[irq_num]();
    }
    
    // Send EOI for hardware IRQs (32+)
    if (irq_num >= 32 && irq_num < 48) {
        pic_send_eoi(irq_num - 32);
    }
}

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].segment = sel;
    idt[num].reserved = 0;
    idt[num].type_attr = flags;
}

void idt_init(void) {
    idt_ptr.base = (uint32_t)&idt;
    idt_ptr.limit = sizeof(idt) - 1;
    
    // Clear IDT
    memset(idt, 0, sizeof(idt));
    
    // Set up exception handlers (gates 0-15)
    idt_set_gate(0, (uint32_t)isr_0, 0x08, 0x8E);      // Division by zero
    idt_set_gate(1, (uint32_t)isr_1, 0x08, 0x8E);      // Debug
    idt_set_gate(2, (uint32_t)isr_2, 0x08, 0x8E);      // NMI
    idt_set_gate(3, (uint32_t)isr_3, 0x08, 0x8E);      // Breakpoint
    idt_set_gate(4, (uint32_t)isr_4, 0x08, 0x8E);      // Overflow
    idt_set_gate(5, (uint32_t)isr_5, 0x08, 0x8E);      // Bound exceeded
    idt_set_gate(6, (uint32_t)isr_6, 0x08, 0x8E);      // Invalid opcode
    idt_set_gate(7, (uint32_t)isr_7, 0x08, 0x8E);      // Device not available
    idt_set_gate(8, (uint32_t)isr_8, 0x08, 0x8E);      // Double fault
    idt_set_gate(10, (uint32_t)isr_10, 0x08, 0x8E);    // Invalid TSS
    idt_set_gate(11, (uint32_t)isr_11, 0x08, 0x8E);    // Segment not present
    idt_set_gate(12, (uint32_t)isr_12, 0x08, 0x8E);    // Stack-segment fault
    idt_set_gate(13, (uint32_t)isr_13, 0x08, 0x8E);    // General protection
    idt_set_gate(14, (uint32_t)isr_14, 0x08, 0x8E);    // Page fault
    
    // Set up hardware interrupt handlers (gates 32-47)
    idt_set_gate(32, (uint32_t)isr_32, 0x08, 0x8E);    // Timer (IRQ0)
    idt_set_gate(33, (uint32_t)isr_33, 0x08, 0x8E);    // Keyboard (IRQ1)
    idt_set_gate(36, (uint32_t)isr_36, 0x08, 0x8E);    // Serial (IRQ4)
    
    // Load IDT
    load_idt(&idt_ptr);
}

void register_interrupt_handler(uint8_t irq, void (*handler)(void)) {
    interrupt_handlers[irq] = handler;
}

// ============================================================================
// ATA/IDE DRIVER (PIO Mode)
// ============================================================================

// IDE Controller Base Addresses
#define IDE_PRIMARY_BASE     0x1F0    // Primary channel I/O base
#define IDE_PRIMARY_CTRL     0x3F6    // Primary channel control/status base
#define IDE_SECONDARY_BASE   0x170    // Secondary channel (not used)
#define IDE_SECONDARY_CTRL   0x376    // Secondary control (not used)

// IDE Register Offsets (relative to channel base)
#define IDE_REG_DATA         0        // Data (16-bit read/write)
#define IDE_REG_ERROR        1        // Error (read only)
#define IDE_REG_FEATURES     1        // Features (write only)
#define IDE_REG_SECTOR_COUNT 2        // Sector count
#define IDE_REG_LBA_LOW      3        // LBA 0-7
#define IDE_REG_LBA_MID      4        // LBA 8-15
#define IDE_REG_LBA_HIGH     5        // LBA 16-23
#define IDE_REG_DEVICE       6        // Device/head (bit 6 = LBA mode)
#define IDE_REG_STATUS       7        // Status (read only)
#define IDE_REG_COMMAND      7        // Command (write only)

// ATA Commands
#define ATA_CMD_READ_PIO     0x20     // Read sectors (PIO)
#define ATA_CMD_WRITE_PIO    0x30     // Write sectors (PIO)
#define ATA_CMD_IDENTIFY     0xEC     // Identify device

// Status Register Bits
#define IDE_STATUS_BSY       0x80     // Busy
#define IDE_STATUS_DRDY      0x40     // Drive ready
#define IDE_STATUS_DRQ       0x08     // Data request ready
#define IDE_STATUS_ERR       0x01     // Error

// Sector size (standard)
#define SECTOR_SIZE          512

static inline uint16_t ide_inw(uint16_t port) {
    uint16_t val;
    asm volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void ide_outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t ide_inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

// Poll IDE status register until condition met
// Returns 0 on success, non-zero on timeout/error
static int ide_poll_status(uint8_t mask, uint8_t expected) {
    unsigned long timeout = 1000000;  // ~1 million iterations
    
    while (timeout--) {
        uint8_t status = ide_inb(IDE_PRIMARY_BASE + IDE_REG_STATUS);
        
        // Check for error
        if (status & IDE_STATUS_ERR) {
            return -1;  // Error occurred
        }
        
        // Check if condition met
        if ((status & mask) == expected) {
            return 0;  // Success
        }
        
        // Small delay
        asm volatile("nop");
    }
    
    return -1;  // Timeout
}

// Initialize ATA/IDE controller
void ata_init(void) {
    // Soft reset: set SRST bit in control register
    // Note: This is optional for QEMU, but good practice
    uint8_t ctrl = ide_inb(IDE_PRIMARY_CTRL);
    ctrl |= 0x04;  // Set SRST bit
    ide_outb(IDE_PRIMARY_CTRL, ctrl);
    
    // Clear SRST
    ctrl &= ~0x04;
    ide_outb(IDE_PRIMARY_CTRL, ctrl);
    
    // QEMU responds instantly to soft reset, no delay needed
}

// Read a single 512-byte sector from disk
// lba = Logical Block Address (sector number)
// buf = buffer to read into (must be at least 512 bytes)
// Returns 0 on success, non-zero on error
int ata_read_sector(uint32_t lba, unsigned char *buf) {
    if (!buf) return -1;
    
    // Set up LBA28 mode (supports up to 128 GB)
    // Device register: bit 6 = 1 (LBA mode), bits 3-0 = head number (0-15)
    uint8_t device = 0x40 | ((lba >> 24) & 0x0F);  // LBA bits 24-27
    
    // Set sector count to 1
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_SECTOR_COUNT, 1);
    
    // Set LBA address (3 bytes: low, mid, high)
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_LBA_LOW, lba & 0xFF);           // Bits 0-7
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_LBA_MID, (lba >> 8) & 0xFF);    // Bits 8-15
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_LBA_HIGH, (lba >> 16) & 0xFF);  // Bits 16-23
    
    // Set device (with LBA mode bit)
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_DEVICE, device);
    
    // Send READ command
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_COMMAND, ATA_CMD_READ_PIO);
    
    // Poll for DRQ (Data Request) bit to be set (drive is ready to send data)
    if (ide_poll_status(IDE_STATUS_DRQ, IDE_STATUS_DRQ) != 0) {
        return -1;  // Read failed
    }
    
    // Read 512 bytes (256 words of 16 bits) from data port
    for (int i = 0; i < SECTOR_SIZE / 2; i++) {
        uint16_t word = ide_inw(IDE_PRIMARY_BASE + IDE_REG_DATA);
        buf[i * 2] = word & 0xFF;
        buf[i * 2 + 1] = (word >> 8) & 0xFF;
    }
    
    // Poll for BSY bit to clear (operation complete)
    if (ide_poll_status(IDE_STATUS_BSY, 0) != 0) {
        return -1;
    }
    
    return 0;  // Success
}

// Write a single 512-byte sector to disk
// lba = Logical Block Address (sector number)
// buf = buffer to write from (must contain 512 bytes)
// Returns 0 on success, non-zero on error
int ata_write_sector(uint32_t lba, unsigned char *buf) {
    if (!buf) return -1;
    
    // Set up LBA28 mode
    uint8_t device = 0x40 | ((lba >> 24) & 0x0F);
    
    // Set sector count to 1
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_SECTOR_COUNT, 1);
    
    // Set LBA address
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_LBA_LOW, lba & 0xFF);
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_LBA_MID, (lba >> 8) & 0xFF);
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // Set device
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_DEVICE, device);
    
    // Send WRITE command
    ide_outb(IDE_PRIMARY_BASE + IDE_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    // Poll for DRQ (drive ready to accept data)
    if (ide_poll_status(IDE_STATUS_DRQ, IDE_STATUS_DRQ) != 0) {
        return -1;
    }
    
    // Write 512 bytes (256 words) to data port
    for (int i = 0; i < SECTOR_SIZE / 2; i++) {
        uint16_t word = buf[i * 2] | (buf[i * 2 + 1] << 8);
        asm volatile("outw %0, %1" : : "a"(word), "Nd"(IDE_PRIMARY_BASE + IDE_REG_DATA));
    }
    
    // Poll for BSY to clear (operation complete)
    if (ide_poll_status(IDE_STATUS_BSY, 0) != 0) {
        return -1;
    }
    
    return 0;  // Success
}
