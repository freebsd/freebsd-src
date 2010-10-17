	.text
	.explicit

	mov r2 = r0
	ld8.mov r3 = [r2], foo#
	;;
	ld8.mov r2 = [r0], foo#
	mov r3 = r2
