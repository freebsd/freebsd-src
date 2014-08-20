#if defined(__aof__)
		AREA	|ARM$$code|,CODE,READONLY

		IMPORT	messages_get
		IMPORT	realloc
		IMPORT	strcpy
		IMPORT	|__rt_stkovf_split_big|

		EXPORT	awrender_init
		EXPORT	awrender_render


aw_rsz_block	*	0
aw_rsz_size	*	4
aw_fixed_block	*	8
aw_fixed_size	*	12
aw_sl		*	16
aw_fp		*	20
sizeof_aw	*	24


; os_error *awrender_init(byte **doc, size_t *doc_size, void *init_routine, void *init_workspace);

awrender_init	MOV	ip,sp
		STMFD	sp!,{a1,a2,v1,v2,fp,ip,lr,pc}
		SUB	fp,ip,#4
		SUB	ip,sp,#512
		CMP	ip,sl
		BLMI	|__rt_stkovf_split_big|

		LDR	v2,=aw_temp
		LDR	a1,[a1]
		MOV	v1,a3
		LDR	a3,[a2]
		MOV	ip,a4
		STR	a1,[v2,#aw_rsz_block]
		STR	a3,[v2,#aw_rsz_size]
		MOV	a2,#-1
		STR	a2,[v2,#aw_fixed_block]
		STR	a3,[v2,#aw_fixed_size]
		STR	sl,[v2,#aw_sl]
		STR	fp,[v2,#aw_fp]
		ADR	a2,aw_callback
		MOV	lr,pc
		MOV	pc,v1
		MOVVC	a1,#0

		;return updated block ptr & size to caller

		LDR	a2,[fp,#-28]
		LDR	a3,[fp,#-24]
		LDR	ip,[v2,#aw_rsz_block]
		LDR	lr,[v2,#aw_rsz_size]
		STR	ip,[a2]
		STR	lr,[a3]

		LDMEA	fp,{v1,v2,fp,sp,pc}


; os_error *awrender_render(const char *doc,
;		const struct awinfo_block *info,
;		const os_trfm *trans,
;		const int *vdu_vars,
;		char **rsz_block,
;		size_t *rsz_size,
;		int wysiwyg_setting,
;		int output_dest,
;		size_t doc_size,
;		void *routine,
;		void *workspace);

awrender_render	MOV	ip,sp
		STMFD	sp!,{v1-v4,fp,ip,lr,pc}
		SUB	fp,ip,#4
		SUB	ip,sp,#512
		CMP	ip,sl
		BLMI	|__rt_stkovf_split_big|

		LDR	R12,[fp,#20]
		LDR	R14,=aw_temp
		LDR	R5,[fp,#4]
		LDR	R6,[fp,#12]
		LDR	R4,[R5]				;resizable block
		LDR	R7,[fp,#16]
		STR	R4,[R14,#aw_rsz_block]
		STR	R0,[R14,#aw_fixed_block]	;document ptr
		STR	R12,[R14,#aw_fixed_size]	;document size
		LDR	R12,[fp,#8]

		STR	R5,[sp,#-4]!			;ptr to receive block
		STR	R12,[sp,#-4]!			;ptr to receive size

		LDR	R12,[R12]
		ADR	R5,aw_callback
		STR	R12,[R14,#aw_rsz_size]

		STR	sl,[R14,#aw_sl]
		STR	fp,[R14,#aw_fp]

		LDR	R12,[fp,#28]
		MOV	lr,pc
		LDR	pc,[fp,#24]
		MOVVC	a1,#0

		;return updated block ptr & size to caller

		LDR	R7,=aw_temp
		LDR	R12,[sp],#4
		LDR	R4,[sp],#4
		LDR	R5,[R7,#aw_rsz_size]
		LDR	R6,[R7,#aw_rsz_block]
		STR	R5,[R12]
		STR	R6,[R4]

		LDMEA	fp,{v1-v4,fp,sp,pc}


; Callback routine for block resizing
; (passed to AWRender init and render routines)
;
; entry	R11 = reason code
;		0 = CallBackReason_Memory
;		3 = CallBackReason_Interface
;			(0 => return capabilities)
; exit	R0 => base of resizable block
;	R1 =  size of resizable block
;	R2 => base of fixed block (or -1 if no fixed block)
;	R3 =  size of fixed block (or document in resizable block)
;	VC if resize successful, VS and R0 => error otherwise

aw_callback	TEQ	R11,#3
		TEQEQ	R0,#0
		MOVEQ	R0,#1<<10		;background colour supplied
		TEQ	R11,#0
		LDREQ	R11,=aw_temp
		MOVNE	PC,R14

		CMP	R0,#-1			;read block size?
		LDRNE	R2,[R11,#aw_rsz_size]
		MOVNE	R1,R0			;new block size
		LDR	R0,[R11,#aw_rsz_block]
		BEQ	aw_read

		; Note: because ArtworksRenderer seems to call
		;	this routine for every scanline rendered
		;	we never call realloc unless we have to in
		;	order to expand the block. Also it calls
		;	us with a size request of 0 which we must
		;	safely ignore otherwise rendering will stop.

		CMP	R1,R2
		BLS	aw_read

		STMFD	R13!,{R1,R10-R12,R14}
		LDR	sl,[R11,#aw_sl]
		LDR	fp,[R11,#aw_fp]
		BL	realloc
		LDMFD	R13!,{R1,R10-R12,R14}

		CMP	R0,#0			;did it work?
		BEQ	aw_nomem

		STR	R0,[R11]
		STR	R1,[R11,#aw_rsz_size]

aw_read		; return details of fixed block

		LDR	R2,[R11,#aw_fixed_block]
		LDR	R3,[R11,#aw_fixed_size]
		SUBS	R11,R11,R11		;clear V
		MOV	PC,R14

aw_nomem	STMFD	R13!,{R10,R12,R14}
		LDR	sl,[R11,#aw_sl]
		LDR	fp,[R11,#aw_fp]
		ADR	R0,tok_nomem
		BL	messages_get
		MOV	a2,a1
		LDR	a1,=errblk + 4
		BL	strcpy
		SUB	R0,R0,#4		;error number already 0
		MOV	R11,#0			;restore reason code
		CMP	PC,#1<<31		;set V
		LDMFD	R13!,{R10,R12,PC}

tok_nomem	=	"NoMemory",0
		ALIGN


		AREA	|ARM$$zidata|,DATA,NOINIT

aw_temp		%	sizeof_aw
errblk		%	256

		END

#elif defined(__ELF__)

		.text

.set aw_rsz_block, 0
.set aw_rsz_size, 4
.set aw_fixed_block, 8
.set aw_fixed_size, 12
.set aw_sl, 16
.set aw_fp, 20
.set sizeof_aw, 24

@ os_error *awrender_init(byte **doc, size_t *doc_size, void *init_routine, void *init_workspace);

		.global awrender_init
awrender_init:	MOV	ip,sp
		STMFD	sp!,{a1,a2,v1,v2,fp,ip,lr,pc}
		SUB	fp,ip,#4
		SUB	ip,sp,#512
		CMP	ip,sl
		BLMI	__rt_stkovf_split_big

		LDR	v2,=aw_temp
		LDR	a1,[a1]
		MOV	v1,a3
		LDR	a3,[a2]
		MOV	ip,a4
		STR	a1,[v2,#aw_rsz_block]
		STR	a3,[v2,#aw_rsz_size]
		MOV	a2,#-1
		STR	a2,[v2,#aw_fixed_block]
		STR	a3,[v2,#aw_fixed_size]
		STR	sl,[v2,#aw_sl]
		STR	fp,[v2,#aw_fp]
		ADR	a2,aw_callback
		MOV	lr,pc
		MOV	pc,v1
		MOVVC	a1,#0

		@ return updated block ptr & size to caller

		LDR	a2,[fp,#-28]
		LDR	a3,[fp,#-24]
		LDR	ip,[v2,#aw_rsz_block]
		LDR	lr,[v2,#aw_rsz_size]
		STR	ip,[a2]
		STR	lr,[a3]

		LDMEA	fp,{v1,v2,fp,sp,pc}


@ os_error *awrender_render(const char *doc,
@		const struct awinfo_block *info,
@		const os_trfm *trans,
@		const int *vdu_vars,
@		char **rsz_block,
@		size_t *rsz_size,
@		int wysiwyg_setting,
@		int output_dest,
@		size_t doc_size,
@		void *routine,
@		void *workspace);

		.global	awrender_render
awrender_render:	MOV	ip,sp
		STMFD	sp!,{v1-v4,fp,ip,lr,pc}
		SUB	fp,ip,#4
		SUB	ip,sp,#512
		CMP	ip,sl
		BLMI	__rt_stkovf_split_big

		LDR	R12,[fp,#20]
		LDR	R14,=aw_temp
		LDR	R5,[fp,#4]
		LDR	R6,[fp,#12]
		LDR	R4,[R5]				@ resizable block
		LDR	R7,[fp,#16]
		STR	R4,[R14,#aw_rsz_block]
		STR	R0,[R14,#aw_fixed_block]	@ document ptr
		STR	R12,[R14,#aw_fixed_size]	@ document size
		LDR	R12,[fp,#8]

		STR	R5,[sp,#-4]!			@ ptr to receive block
		STR	R12,[sp,#-4]!			@ ptr to receive size

		LDR	R12,[R12]
		ADR	R5,aw_callback
		STR	R12,[R14,#aw_rsz_size]

		STR	sl,[R14,#aw_sl]
		STR	fp,[R14,#aw_fp]

		LDR	R12,[fp,#28]
		MOV	lr,pc
		LDR	pc,[fp,#24]
		MOVVC	a1,#0

		@ return updated block ptr & size to caller

		LDR	R7,=aw_temp
		LDR	R12,[sp],#4
		LDR	R4,[sp],#4
		LDR	R5,[R7,#aw_rsz_size]
		LDR	R6,[R7,#aw_rsz_block]
		STR	R5,[R12]
		STR	R6,[R4]

		LDMEA	fp,{v1-v4,fp,sp,pc}


@ Callback routine for block resizing
@ (passed to AWRender init and render routines)
@
@ entry	R11 = reason code
@		0 = CallBackReason_Memory
@		3 = CallBackReason_Interface
@			(0 => return capabilities)
@ exit	R0 => base of resizable block
@	R1 =  size of resizable block
@	R2 => base of fixed block (or -1 if no fixed block)
@	R3 =  size of fixed block (or document in resizable block)
@	VC if resize successful, VS and R0 => error otherwise

aw_callback:	TEQ	R11,#3
		TEQEQ	R0,#0
		MOVEQ	R0,#1<<10		@ background colour supplied
		TEQ	R11,#0
		LDREQ	R11,=aw_temp
		MOVNE	PC,R14

		CMP	R0,#-1			@ read block size?
		LDRNE	R2,[R11,#aw_rsz_size]
		MOVNE	R1,R0			@ new block size
		LDR	R0,[R11,#aw_rsz_block]
		BEQ	aw_read

		@ Note: because ArtworksRenderer seems to call
		@	this routine for every scanline rendered
		@	we never call realloc unless we have to in
		@	order to expand the block. Also it calls
		@	us with a size request of 0 which we must
		@	safely ignore otherwise rendering will stop.

		CMP	R1,R2
		BLS	aw_read

		STMFD	R13!,{R1,R10-R12,R14}
		LDR	sl,[R11,#aw_sl]
		LDR	fp,[R11,#aw_fp]
		BL	realloc
		LDMFD	R13!,{R1,R10-R12,R14}

		CMP	R0,#0			@ did it work?
		BEQ	aw_nomem

		STR	R0,[R11]
		STR	R1,[R11,#aw_rsz_size]

aw_read:	@ return details of fixed block

		LDR	R2,[R11,#aw_fixed_block]
		LDR	R3,[R11,#aw_fixed_size]
		SUBS	R11,R11,R11		@ clear V
		MOV	PC,R14

aw_nomem:	STMFD	R13!,{R10,R12,R14}
		LDR	sl,[R11,#aw_sl]
		LDR	fp,[R11,#aw_fp]
		ADR	R0,tok_nomem
		BL	messages_get
		MOV	a2,a1
		LDR	a1,=errblk + 4
		BL	strcpy
		SUB	R0,R0,#4		@ error number already 0
		MOV	R11,#0			@ restore reason code
		CMP	PC,#1<<31		@ set V
		LDMFD	R13!,{R10,R12,PC}

tok_nomem:	.asciz	"NoMemory"
		.align

		.bss

aw_temp:	.space	sizeof_aw
		.type	aw_temp, %object
		.size	aw_temp, . - aw_temp

errblk:		.space	256
		.type	errblk, %object
		.size	errblk, . - errblk

		.end
#endif

