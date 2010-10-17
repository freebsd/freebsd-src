	.text
	ld8.mov r2 = [r3], foo#
	ld8.mov r4 = [r5], bar#
	ld8.mov r6 = [r7], foo# + 100
	ld8.mov r8 = [r9], bar# + 100
	
	.data
bar:
