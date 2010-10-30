.arch armv6
.text
arm:
mov r0, #0
$m:
bx lr
.thumb
.thumb_func
thumb:
nop
bx lr
bl thumb
data:
.word 0x12345678
