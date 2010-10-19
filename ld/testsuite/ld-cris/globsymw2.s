	.text
	.stabn	162,0,0,0
;# A bit like globsymw1.s but containing a valid, working, stabs
;# symbol warning construct.
	.stabs "isatty is not implemented and will always fail",30,0,0,0
	.stabs "globsym1",1,0,0,0
	.global globsym1
	.type	globsym1, @function
globsym1:
	.stabd	46,0,0
	.stabn	68,0,16,LM0-globsym1
LM0:
	.long 0
	.size	globsym1, .-globsym1
	.stabs	"",100,0,0,Letext0
Letext0:
