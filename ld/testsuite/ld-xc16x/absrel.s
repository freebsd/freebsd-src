	.global _start
_start:
	mov r5,#0xf
	mov r6,#0xf

.12:
	mov r5,.end
	mov r6,#0xd
	mov r7,.end
	mov r8,#0xd
.13:
	mov r5,.end
	mov r6,#0xf
	mov r7,.end
	mov r8,#0xf
.end:
	;calla cc_UC,.13
	;calla cc_usr1,.12

	;calla+ cc_UGT,.12
	calla- cc_nusr0,.12
	calla- cc_nusr1,.12
	calla- cc_usr0,.12

	;jmpa cc_UGT,.end
	;jmpa cc_nusr0,.end

	;jmpa+ cc_UGT,.12
	jmpa- cc_nusr0,.12
	jmpa- cc_nusr1,.12
	jmpa- cc_usr0,.12
