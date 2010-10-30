.text
.arch armv4t
.syntax unified
.thumb
foo:
movs r0, #12
adds r1, r2, #3
subs r1, r2, #3
adds r3, r3, #0x64
subs r4, r4, #0x83
cmp r5, #0x27

adr r1, bar
ldr r2, bar
ldr r3, [r4, #4]
ldr r5, [sp, #4]
add sp, sp, #4
sub sp, sp, #4
add r7, sp, #4

rsbs r1, r2, #0

.align 2
bar:

