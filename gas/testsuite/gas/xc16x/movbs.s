        .section .text
        .global  _fun

xc16x_movbs:

	movbs  r0,rl1
	movbs  r0,0xff
	movbs  0xffcb,rl0
