
	.text
	.global misc
misc:
	di
	ei
	halt
	nop
	reti
	trap 0
	trap 31
	ldsr r7,psw
	stsr psw,r7
