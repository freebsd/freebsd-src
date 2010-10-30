
.*jump19:     file format elf32-(big|little)arm

Disassembly of section .text:

00008000 <_start>:
    8000:	4280      	cmp	r0, r0
    8002:	f010 8000 	beq.w	18006 <bar>
	...

00018006 <bar>:
   18006:	4770      	bx	lr
