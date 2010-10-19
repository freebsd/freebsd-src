.section  .text
.global	 _fun

xc16x_cmpd:

	cmpd1	r0,#0x0f
	cmpd1 	r0,#0x0fccb
	cmpd1	r0,0xffcb
	cmpd2	r0,#0x0f
	cmpd2 	r0,#0x0fccb
	cmpd2	r0,0xffcb
	cmpi1	r0,#0x0f
	cmpi1 	r0,#0x0fccb
	cmpi1	r0,0xffcb
	cmpi2	r0,#0x0f
	cmpi2 	r0,#0x0fccb
	cmpi2	r0,0xffcb
	