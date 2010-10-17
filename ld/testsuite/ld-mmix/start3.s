# When GC, all sections in each file must be referenced from within a kept
# section.
 .section .init,"ax",@progbits
_start:
 .quad x+41
 .quad x2+42

