.psize 0
.text
.extern xxx

1:	jmp	1b
	jmp	xxx
	jmp	*xxx
	jmp	*%edi
	jmp	*(%edi)
	ljmp	*xxx(,%edi,4)
	ljmp	*xxx
	ljmp	$0x1234,$xxx

	call	1b
	call	xxx
	call	*xxx
	call	*%edi
	call	*(%edi)
	lcall	*xxx(,%edi,4)
	lcall	*xxx
	lcall	$0x1234,$xxx

	# Force a good alignment.
	.p2align	4,0
