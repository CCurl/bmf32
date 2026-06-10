// Bare metal system layer for FWC - replaces POSIX system.c
// Provides minimal OS functionality for 32-bit x86 bare metal QEMU

#include "fwc-vm.h"
#include "drivers.h"

// ===== OUTPUT LAYER =====
// Serial-based output replaces stdout

void zType(const char *str) {
    serial_write_string(str);
}

void emit(const char ch) {
    serial_write_char(ch);
}

// ===== INPUT LAYER =====
// Serial and PS/2 keyboard for input

int qKey(void) {
    // Check keyboard first (PS/2 takes priority)
    if (ps2_has_key()) {
        return 1;
    }
    // Check serial
    return serial_has_data();
}

static int ps2_init_done = 0;

int key(void) {
    // Flush stray PS/2 scan code from initialization
    if (!ps2_init_done) {
        ps2_init_done = 1;
        if (ps2_has_key()) {
            ps2_getc();  // Discard stray code
        }
    }
    
    // Read from keyboard (PS/2) or serial
    if (ps2_has_key()) {
        char ch = ps2_getc();
        if (ch != 0) {
            return (int)ch;
        }
    }
    
    // Fall back to serial
    return serial_getc();
}

// ===== TIMER =====

cell timer(void) {
    return (cell)timer_get_ticks();
}

void ms(cell sleepForMS) {
    timer_sleep_ms((unsigned long)sleepForMS);
}

// ===== TYPE AND EMIT (already defined above for serial) =====

// ===== BLOCKS =====
void readBlock(ucell blockNum, unsigned char *buffer) {
    uint32_t blk = blockNum*2;  // Assuming 512-byte sectors and 1024-byte blocks
    ata_read_sector(blk, buffer);
    ata_read_sector(blk+1, buffer + 512);
}

void writeBlock(ucell blockNum, unsigned char *buffer) {
    uint32_t blk = blockNum*2;  // Assuming 512-byte sectors and 1024-byte blocks
    ata_write_sector(blk, buffer);
    ata_write_sector(blk+1, buffer + 512);
}

char tib[128], fn[32];

// ===== REPL (Read-Eval-Print Loop) =====

void repl(void) {
    if (state != COMPILE) {
        state = INTERPRET;
    }
    zType((state == COMPILE) ? " ... " : " ok\n");
    
    // Read from serial (no full line buffering in MVP)
    // Simple line-at-a-time reader
    int pos = 0;
    while (pos < 127) {
        int ch = key();
        
        if (ch == '\r' || ch == '\n') {
            tib[pos] = 0;
            emit('\n');
            break;
        } else if (ch == '\b' || ch == 127) {  // Backspace or DEL
            if (pos > 0) {
                pos--;
                emit('\b');
                emit(' ');
                emit('\b');
            }
        } else if (ch >= 32 && ch < 127) {  // Printable character
            tib[pos++] = (char)ch;
            emit((char)ch);
        }
    }
    
    if (pos > 0 || (pos == 0 && tib[0])) {
        outer(tib);
    }
}

// ===== MAIN KERNEL ENTRY POINT =====
// Called from boot.asm

extern void bmfInit(void);

void kmain(unsigned long magic, unsigned long addr) {
    // Initialize hardware
    serial_init();
    serial_write_string("FWC Bare Metal Kernel starting...\n");
    
    timer_init();
    serial_write_string("Timer initialized\n");
    
    pic_init();
    serial_write_string("PIC initialized\n");
    
    ps2_init();
    serial_write_string("PS/2 Keyboard initialized\n");
    
    idt_init();
    serial_write_string("IDT initialized\n");
    
    // Enable interrupts
    asm volatile("sti");
    serial_write_string("Interrupts enabled\n");
    
    // Initialize ATA after interrupts are enabled (it may sleep)
    ata_init();
    serial_write_string("ATA/IDE initialized\n");
    
    // Initialize FWC VM
    bmfInit();
    serial_write_string("FWC VM initialized\n");
    
    // Boot Forth system
    bmfBoot();
    
    // Main REPL loop
    zType("BMF - Bare Metal Forth - v0.1\n");
    zType(" ok\n");
    
    while (state != BYE) {
        repl();
    }
    
    // Shutdown
    zType("Shutting down...\n");
    asm volatile("cli");  // Disable interrupts
    asm volatile("hlt");  // Halt CPU
    
    // Infinite loop (should never reach here)
    while (1) {
        asm volatile("hlt");
    }
}
