( run this using fwc )
cell var in cell var out
64 kb var src

: open-in  z" boot.f" fopen-r dup in  ! ;
: open-out z" boot.h" fopen-w dup out ! ;
: close-in  in  @ fclose ;
: close-out out @ fclose ;
: read-in src 64 kb in @ fread drop ;
: write-char  ( -- )
    x@ '"' = if '\' emit '"' emit exit then
    x@ '\' = if '\' emit '\' emit exit then
    x@ 10  = if 32  emit '\' emit 10 emit exit then
    x@ emit ;
: write-out out @ ->file src y!
    ." #define DWC_SRC " '"' emit
    begin
        c@y+ x! x@
        if0 '"' emit cr exit then write-char
    again ;
: failed ." Forth source translation failed" cr ;
: success ." Forth source translation success" cr ;
: go read-in write-out close-in close-out ->stdout success ;
: main open-in open-out and if0 failed exit then go ;
main bye
