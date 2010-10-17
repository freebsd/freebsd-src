.psize 0
.text
.extern xxx

.code16gcc
1:	jmp	1b
	jmp	xxx
	jmp	*xxx
	jmp	*%edi
	jmp	*(%edi)
	ljmp	*xxx(%edi)
	ljmp	*xxx
	ljmp	$0x1234,$xxx

	call	1b
	call	xxx
	call	*xxx
	call	*%edi
	call	*(%edi)
	lcall	*xxx(%edi)
	lcall	*xxx
	lcall	$0x1234,$xxx

.code16
	jmp	1b
	jmp	*xxx
	jmp	*%di
	jmp	*(%di)
	ljmp	*xxx(%di)
	ljmp	*xxx
	ljmp	$0x1234,$xxx

	call	1b
	call	xxx
	call	*xxx
	call	*%di
	call	*(%di)
	lcall	*xxx(%di)
	lcall	*xxx
	lcall	$0x1234,$xxx

	# Force a good alignment.
	.p2align	4,0
