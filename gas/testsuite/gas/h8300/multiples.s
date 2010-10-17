	.h8300s
	.text
h8300s_multiple:
	ldm.l @sp+,er0-er1
	ldm.l @sp+,er0-er2
	ldm.l @sp+,er0-er3
	stm.l er0-er1,@-sp
	stm.l er0-er2,@-sp
	stm.l er0-er3,@-sp
        ldm.l @sp+,er2-er3
        stm.l er2-er3,@-sp
        ldm.l @sp+,er4-er5
        ldm.l @sp+,er4-er6
        stm.l er4-er5,@-sp
        stm.l er4-er6,@-sp


