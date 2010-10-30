@ Test -gc-sections and unwinding tables.  .data.eh should be pulled in
@ via the EH tables, .data.foo should not.
.text
.global _start
.fnstart
_start:
bx lr
.personality my_pr
.handlerdata
.word 0
.fnend

.section .data.foo
my_foo:
.word 0x11111111

.section .text.foo
.fnstart
foo:
bx lr
.personality my_pr
.handlerdata
.word my_foo
.fnend

.section .data.eh
my_eh:
.word 0x22222222

.section .text.eh
.fnstart
my_pr:
bx lr
.personality my_pr
.handlerdata
.word my_eh
.fnend

