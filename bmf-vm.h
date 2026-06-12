// A Tachyon inspired system, MIT license, (c) 2025 Chris Curl

#ifndef __BMF_H__

#define VERSION         20260609

// Bare metal 32-bit build for QEMU
#define strEqI(s, d)  (strcasecmp(s, d) == 0)
#define BIN_DIR ""
#define MEM_SZ         0x01000000

#include <stdint.h>
#include <string.h>

#define LIT_MASK      0x40000000
#define LIT_BITS      0x3FFFFFFF
#define CELL_SZ                4
#define NAME_SZ               26
#define cell             int32_t
#define ucell           uint32_t

#define byte             uint8_t
#define STK_SZ                63
#define IMMED               0x80
#define INLINE              0x40
#define btwi(n,l,h)   ((l<=n) && (n<=h))
#define TOS           dstk[dsp]
#define NOS           dstk[dsp-1]
#define L0            lstk[lsp]
#define L1            lstk[lsp-1]
#define L2            lstk[lsp-2]

enum { INTERPRET=0, COMPILE=1, BYE=999 };
typedef struct { ucell xt; byte fl; byte ln; char nm[NAME_SZ]; } DE_T;
typedef struct { char *name; ucell value; } NVP_T;

// These are defined by bmf-vm.c
extern void inner(ucell start);
extern void outer(const char *src);
extern void addLit(const char *name, cell val);
extern void bmfInit();
extern int nextWord();
extern DE_T *addToDict(char *w);
extern cell state;
extern char mem[];

// bmf-vm.c needs these to be defined
extern void outer(const char *src);
extern void zType(const char *str);
extern void emit(const char ch);
extern int  key();
extern int  qKey();
extern cell timer();
extern void bmfBoot();
extern void ms(cell sleepForMS);
extern void readBlock(ucell lba, unsigned char *buf);
extern void writeBlock(ucell lba, unsigned char *buf);

#endif //  __BMF_H__
