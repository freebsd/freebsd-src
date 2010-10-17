@ test for OFFSET_IMM reloc against global symbols

.globl foo
foo: .word 0
ldr r0, foo
