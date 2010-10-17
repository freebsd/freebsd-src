# When GC, all sections in each file must be referenced from within a kept
# section (which .init is, which .text isn't).  Here, we don't refer to
# anything so whatever is linked will be discarded.
 .section .init,"ax",@progbits
_start:
 SETL $119,1190
