	.code
	.align 4
	.export divu,entry
	.proc
	.callinfo
divu:	stws,ma		%r4,4(%r5)		; save registers on stack
	.procend

	.export divu2,entry
	.proc
	.callinfo
	.entry
divu2:	stws,ma		%r4,4(%r5)		; save registers on stack
	.exit
	.procend
