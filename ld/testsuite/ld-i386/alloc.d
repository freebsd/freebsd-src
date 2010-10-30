#name: Invalid allocated section
#as: --32
#ld: -melf_i386 -T alloc.t
#error: .*section `.foo' can't be allocated in segment 0.*
