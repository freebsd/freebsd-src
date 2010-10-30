.text
.arm
.global _start
.type _start, %function
_start:
b foo

.thumb
.global foo
.type foo, %function
foo:
nop
bx lr

