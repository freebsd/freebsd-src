.extCondCode   isbusy, 0x12
.extCoreRegister rwscreg,43,r|w,can_shortcut
.extCoreRegister roscreg,44,r,can_shortcut
.extCoreRegister woscreg,45,w,can_shortcut
	.section .text
condcodeTest:
	add.isbusy r0,r0,r1
	add 	rwscreg,r0,r1
	add 	r0,r1,roscreg
	add	woscreg,r0,r1
