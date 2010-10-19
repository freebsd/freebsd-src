 .macro bad args:vararg
  .ifdef _bad_
	\args
  .endif
 .endm

 .macro ill args:vararg
  # This is used to mark entries that aren't handled consistently,
  # and thus shouldn't currently be checked for.
  #	\args
 .endm

 .text
_start:
	mov	$xtrn, %eax
	mov	$xtrn, %ax
	mov	$xtrn, %al
	mov	xtrn(%ebx), %eax
	mov	xtrn(%bx), %eax

	mov	$(xtrn - .), %eax
	mov	$(xtrn - .), %ax
	mov	$(xtrn - .), %al
	mov	xtrn - .(%ebx), %eax
	mov	xtrn - .(%bx), %eax
	call	xtrn
	jecxz	xtrn

	mov	$xtrn@got, %eax
bad	mov	$xtrn@got, %ax
bad	mov	$xtrn@got, %al
	mov	xtrn@got(%ebx), %eax
bad	mov	xtrn@got(%bx), %eax
bad	call	xtrn@got

	mov	$xtrn@gotoff, %eax
bad	mov	$xtrn@gotoff, %ax
bad	mov	$xtrn@gotoff, %al
	mov	xtrn@gotoff(%ebx), %eax
bad	mov	xtrn@gotoff(%bx), %eax
bad	call	xtrn@gotoff

	add	$_GLOBAL_OFFSET_TABLE_, %eax
ill	add	$_GLOBAL_OFFSET_TABLE_, %ax
ill	add	$_GLOBAL_OFFSET_TABLE_, %al
	add	$(_GLOBAL_OFFSET_TABLE_ - .), %eax
ill	add	$(_GLOBAL_OFFSET_TABLE_ - .), %ax
ill	add	$(_GLOBAL_OFFSET_TABLE_ - .), %al

	mov	$xtrn@plt, %eax
bad	mov	$xtrn@plt, %ax
bad	mov	$xtrn@plt, %al
	mov	xtrn@plt(%ebx), %eax
bad	mov	xtrn@plt(%bx), %eax
	call	xtrn@plt
bad	jecxz	xtrn@plt

	mov	$xtrn@tlsgd, %eax
bad	mov	$xtrn@tlsgd, %ax
bad	mov	$xtrn@tlsgd, %al
	mov	xtrn@tlsgd(%ebx), %eax
bad	mov	xtrn@tlsgd(%bx), %eax
bad	call	xtrn@tlsgd

	mov	$xtrn@gotntpoff, %eax
bad	mov	$xtrn@gotntpoff, %ax
bad	mov	$xtrn@gotntpoff, %al
	mov	xtrn@gotntpoff(%ebx), %eax
bad	mov	xtrn@gotntpoff(%bx), %eax
bad	call	xtrn@gotntpoff

	mov	$xtrn@indntpoff, %eax
bad	mov	$xtrn@indntpoff, %ax
bad	mov	$xtrn@indntpoff, %al
	mov	xtrn@indntpoff(%ebx), %eax
bad	mov	xtrn@indntpoff(%bx), %eax
bad	call	xtrn@indntpoff

	mov	$xtrn@gottpoff, %eax
bad	mov	$xtrn@gottpoff, %ax
bad	mov	$xtrn@gottpoff, %al
	mov	xtrn@gottpoff(%ebx), %eax
bad	mov	xtrn@gottpoff(%bx), %eax
bad	call	xtrn@gottpoff

	mov	$xtrn@tlsldm, %eax
bad	mov	$xtrn@tlsldm, %ax
bad	mov	$xtrn@tlsldm, %al
	mov	xtrn@tlsldm(%ebx), %eax
bad	mov	xtrn@tlsldm(%bx), %eax
bad	call	xtrn@tlsldm

	mov	$xtrn@dtpoff, %eax
bad	mov	$xtrn@dtpoff, %ax
bad	mov	$xtrn@dtpoff, %al
	mov	xtrn@dtpoff(%ebx), %eax
bad	mov	xtrn@dtpoff(%bx), %eax
bad	call	xtrn@dtpoff

	mov	$xtrn@ntpoff, %eax
bad	mov	$xtrn@ntpoff, %ax
bad	mov	$xtrn@ntpoff, %al
	mov	xtrn@ntpoff(%ebx), %eax
bad	mov	xtrn@ntpoff(%bx), %eax
bad	call	xtrn@ntpoff

	mov	$xtrn@tpoff, %eax
bad	mov	$xtrn@tpoff, %ax
bad	mov	$xtrn@tpoff, %al
	mov	xtrn@tpoff(%ebx), %eax
bad	mov	xtrn@tpoff(%bx), %eax
bad	call	xtrn@tpoff

 .data
	.long	xtrn
	.long	xtrn - .
	.long	xtrn@got
	.long	xtrn@gotoff
	.long	_GLOBAL_OFFSET_TABLE_
	.long	_GLOBAL_OFFSET_TABLE_ - .
	.long	xtrn@plt
	.long	xtrn@tlsgd
	.long	xtrn@gotntpoff
	.long	xtrn@indntpoff
	.long	xtrn@gottpoff
	.long	xtrn@tlsldm
	.long	xtrn@dtpoff
	.long	xtrn@ntpoff
	.long	xtrn@tpoff
	
	.word	xtrn
	.word	xtrn - .
bad	.word	xtrn@got
bad	.word	xtrn@gotoff
ill	.word	_GLOBAL_OFFSET_TABLE_
ill	.word	_GLOBAL_OFFSET_TABLE_ - .
bad	.word	xtrn@plt
bad	.word	xtrn@tlsgd
bad	.word	xtrn@gotntpoff
bad	.word	xtrn@indntpoff
bad	.word	xtrn@gottpoff
bad	.word	xtrn@tlsldm
bad	.word	xtrn@dtpoff
bad	.word	xtrn@ntpoff
bad	.word	xtrn@tpoff

	.byte	xtrn
	.byte	xtrn - .
bad	.byte	xtrn@got
bad	.byte	xtrn@gotoff
ill	.byte	_GLOBAL_OFFSET_TABLE_
ill	.byte	_GLOBAL_OFFSET_TABLE_ - .
bad	.byte	xtrn@plt
bad	.byte	xtrn@tlsgd
bad	.byte	xtrn@gotntpoff
bad	.byte	xtrn@indntpoff
bad	.byte	xtrn@gottpoff
bad	.byte	xtrn@tlsldm
bad	.byte	xtrn@dtpoff
bad	.byte	xtrn@ntpoff
bad	.byte	xtrn@tpoff
