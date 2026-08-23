#include <stddef.h>
#include <stdint.h>
#include "kernel.h"
#include "fwc-vm.h"

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

void fwcRun() {
    fwcInit();
    sys_load();
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

void sys_load() {
    outer(" \
: last (l) @ ; \
: here (h) @ ; \
: inline    ( -- ) $40 last cell + c! ; \
: immediate ( -- ) $80 last cell + c! ; \
: cells  ( n--n' ) cell * ; inline \
: ->code ( off--addr ) cells mem + ; \
: code@  ( off--dw )  ->code @ ; \
: code!  ( dw off-- ) ->code ! ; \
: , ( dw-- ) here dup 1 + (h) ! code! ; \
 \
: bye      ( -- ) 999 state ! ; \
: (exit)   ( --n )  0 ; inline \
: (lit)    ( --n )  1 ; inline \
: (jmp)    ( --n )  2 ; inline \
: (jmpz)   ( --n )  3 ; inline \
: (jmpnz)  ( --n )  4 ; inline \
: (njmpz)  ( --n )  5 ; inline \
: (njmpnz) ( --n )  6 ; inline \
: (ztype)  ( --n ) 48 ; inline \
 \
: if   (jmpz)   , here 0 , ; immediate \
: -if  (njmpz)  , here 0 , ; immediate \
: if0  (jmpnz)  , here 0 , ; immediate \
: -if0 (njmpnz) , here 0 , ; immediate \
: then here swap code!     ; immediate \
 \
: begin here ; immediate \
: again (jmp)     , , ; immediate \
: while (jmpnz)   , , ; immediate \
: -while (njmpnz) , , ; immediate \
: until (jmpz)    , , ; immediate \
 \
( val and (val) define a very efficient variable mechanism ) \
( Usage:  val a@   (val) (a)   : a! (xx) ! ; ) \
: const ( n-- ) add-word (lit) , , (exit) , ; \
:  val  ( -- ) 0 const ; \
: (val) ( -- ) here 2 - ->code const ; \
: kb ( n--m ) 1024 * ; \
: mb ( n--m ) kb kb ; \
\
mem mem-sz + const dict-end \
32 ->code const (vh) \
64 kb ->code const vars \
vars (vh) ! \
: vhere ( --a ) (vh) @ ; \
: allot ( n-- ) (vh) +! ; \
: var   ( n-- ) vhere const allot ; \
 \
( 3 variables that can be used as locals - x,y,z ) \
: +L1 ( x -- )    +L x! ; \
: +L2 ( x y-- )   +L y! x! ; \
: +L3 ( x y z-- ) +L z! y! x! ; \
 \
: x++ ( -- )  x@+ drop ;  : x--  ( -- )  x@ 1- x! ;  : x@-  ( --n ) x@ x-- ; \
: c@x ( --b ) x@ c@ ;     : c@x+ ( --b ) x@+ c@ ;    : c@x- ( --b ) x@- c@ ; \
: c!x ( b-- ) x@ c! ;     : c!x+ ( b-- ) x@+ c! ;    : c!x- ( b-- ) x@- c! ; \
 \
: y++ ( -- )  y@+ drop ;  : y--  ( -- )  y@ 1- y! ;  : y@-  ( --n ) y@ y-- ; \
: c@y ( --b ) y@ c@ ;     : c@y+ ( --b ) y@+ c@ ;    : c@y- ( --b ) y@- c@ ; \
: c!y ( b-- ) y@ c! ;     : c!y+ ( b-- ) y@+ c! ;    : c!y- ( b-- ) y@- c! ; \
 \
: z++ ( -- )  z@+ drop ;  : z--  ( -- )  z@ 1- z! ;  : z@-  ( --n ) z@ z-- ; \
: c@z ( --b ) z@ c@ ;     : c@z+ ( --b ) z@+ c@ ;    : c@z- ( --b ) z@- c@ ; \
: c!z ( b-- ) z@ c! ;     : c!z+ ( b-- ) z@+ c! ;    : c!z- ( b-- ) z@- c! ; \
 \
( Strings ) \
: compiling? ( --n ) state @ 1 = ; \
: (\") ( --a ) +L vhere dup z! x! 1 >in +! \
    begin \
        >in @ c@ y! 1 >in +! \
        y@ 0 = y@ '\"' = or \
        if  0 c!x+  z@ \
            compiling? if (lit) , , x@ (vh) ! then \
            -L exit \
        then \
        y@ c!x+ \
    again ; \
 \
: z\" ( str--addr ) (\") ; immediate \
: .\" ( str-- ) (\") compiling? if (ztype) , exit then ztype ; immediate \
 \
( More core words ) \
: [ ( -- ) 0 state ! ; immediate  ( 0 = INTERPRET ) \
: ] ( -- ) 1 state ! ;            ( 1 = COMPILE ) \
: rdrop ( -- ) r> drop ; inline \
: tuck  ( a b--b a b )   swap over ; inline \
: nip   ( a b--b )       swap drop ; inline \
: ?dup ( n--n n|0 )  -if dup then ; \
: 2dup  ( a b--a b a b ) over over ; inline \
: 2drop ( a b-- )        drop drop ; inline \
: -rot ( a b c--c a b )  swap >r swap r> ; \
: cell+ ( a--a1 ) cell + ; inline \
: 0< ( n--f ) 0 <    ; inline \
: <= ( a b--f ) > 0= ; \
: >= ( a b--f ) < 0= ; \
: type ( a n-- ) for dup c@ emit 1+ next drop ; \
: btwi ( n l h--f ) >r over <= swap r> <= and ; \
: negate ( n--n' ) 0 swap - ; \
: abs ( n--n1 ) dup 0< if negate then ; \
: cr  ( -- )     13 emit 10 emit ; \
: tab ( -- )      9 emit ; \
: space  ( -- )  32 emit ; \
: spaces ( n-- ) for space next ; \
: /   ( a b--q ) /mod nip  ; \
: mod ( a b--r ) /mod drop ; \
: */  ( n m q--n' ) >r * r> / ; \
: unloop  ( -- ) (lsp) @ 3 - 0 max (lsp) ! ; \
: execute ( xt-- ) ?dup if >r then ; \
: decimal  ( -- )  #10 base ! ; \
: hex      ( -- )  $10 base ! ; \
: binary   ( -- )  %10 base ! ; \
 \
   1 var (neg) \
  65 var buf \
cell var (buf) \
: ?neg ( n--n' ) dup 0< dup (neg) c! if negate then ; \
: hold ( c-- )   -1 (buf) +! (buf) @ c! ; \
: #.   ( -- )    '.' hold ; \
: #n   ( n-- )   '0' + dup '9' > if 7 + then hold ; \
: #    ( n--m )  base @ /mod swap #n ; \
: #s   ( n--0 )  # -if #s exit then ; \
: <#   ( n--n' ) ?neg buf 65 + (buf) ! 0 hold ; \
: #>   ( n--a )  drop (neg) @ if '-' hold then (buf) @ ; \
: (.)  ( n-- )   <# #s #> ztype ; \
: .    ( n-- )   (.) space ; \
 \
: 0sp 0 (sp) ! ; \
: depth ( --n ) (sp) @ 1- ; \
: .s '(' emit space depth ?dup if \
        stk swap for cell+ dup @ . next drop \
    then ')' emit ; \
 \
: .word ( de-- ) cell+ 2 + ztype ; \
: words ( -- ) +L last x! 0 y! 0 z! begin \
        x@ dict-end < if0 '(' emit z@ . .\" words)\" -L exit then \
        x@ .word tab z++ \
        x@ cell+ 1+ c@ 7 > if y++ then \
        y@+ 7 > if cr 0 y! then \
        x@ de-sz + x! \
    again ; \
 \
: words-n ( n-- ) +L last x! 0 y! for \
        x@ .word tab \
        y@+ 7 > if cr 0 y! then \
		x@ de-sz + x! \
    next -L ; \
 \
cell var t4   cell var t5 \
: [[ here t4 !  vhere t5 !  1 state ! ; \
: ]] (exit) , 0 state ! t4 @ dup >r (h) ! t5 @ (vh) ! ; immediate \
 \
cell var t4   cell var t5   cell var t6 \
: marker ( -- ) here t4 !   vhere t5 !   last t6 ! ; \
: forget ( -- ) t4 @ (h) !  t5 @ (vh) !  t6 @ (l) ! ; \
 \
( Strings / Memory ) \
: pad    ( --a ) vhere $100 + ; \
: fill   ( a num ch-- ) -rot for 2dup c! 1+ next 2drop ; \
: s-end  ( str--end ) dup s-len + ;   ( end: address of the null ) \
: s-cpy  ( dst src--dst ) 2dup s-len 1+ cmove ; \
: s-cat  ( dst src--dst ) over s-end  over s-len 1+  cmove ; \
: s-catc ( dst ch--dst )  over s-end  +L1  c!x+  0 c!x+  -L ; \
: s-catn ( dst num--dst ) <# #s #> s-cat ; \
: s-scat ( src dst--dst ) swap s-cat ; \
: s-eqn  ( s1 s2 n--f ) +L3 z@ for c@x+ c@y+ = if0 -L 0 unloop exit then next -L 1 ; \
: s-eq   ( s1 s2--f ) dup s-len 1+ s-eqn ; \
\
: .version ( -- ) version <# # # #. # # #. # # #s #> ztype ; \
: bm ( mb -- ) mb timer swap for next timer swap - . ; \
 \
marker \
");
}
