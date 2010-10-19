.psize 0
.text
#test jumps and calls
1:	jmp	1b
	jmp	xxx
	jmp	*xxx
	jmp	xxx(,1)
	jmp	*%edi
	jmp	%edi
	jmp	*(%edi)
	jmp	(%edi)
	ljmp	*xxx(,%edi,4)
	ljmp	xxx(,%edi,4)
	ljmp	*xxx
	ljmp	xxx(,1)
	ljmp	$0x1234,$xxx

	call	1b
	call	xxx
	call	*xxx
	call	xxx(,1)
	call	*%edi
	call	%edi
	call	*(%edi)
	call	(%edi)
	lcall	*xxx(,%edi,4)
	lcall	xxx(,%edi,4)
	lcall	*xxx
	lcall	xxx(,1)
	lcall	$0x1234,$xxx

# test various segment reg insns
	push	%ds
	pushl	%ds
	pop	%ds
	popl	%ds
	mov	%ds,%eax
	movl	%ds,%eax
	movl	%ds,%ax
	mov	%eax,%ds
	movl	%ax,%ds
	movl	%eax,%ds

	pushw	%ds
	popw	%ds
	mov	%ds,%ax
	movw	%ds,%ax
	movw	%ds,%eax
	mov	%ax,%ds
	movw	%ax,%ds
	movw	%eax,%ds

# test various pushes
	pushl	$10
	pushw	$10
	push	$10
	pushl	$1000
	pushw	$1000
	push	$1000
	pushl	1f
	pushw	1f
	push	1f
	push	(1f-.)(%ebx)
	push	1f-.
# these, and others like them should have no operand size prefix
1:	lldt	%cx
	lmsw	%ax

# Just to make sure these don't become illegal due to over-enthusiastic
# register checking
	movsbw	%al,%di
	movsbl	%al,%ecx
	movswl	%ax,%ecx
	movzbw	%al,%di
	movzbl	%al,%ecx
	movzwl	%ax,%ecx

	in	%dx,%al
	in	%dx,%ax
	in	%dx,%eax
	in	(%dx),%al
	in	(%dx),%ax
	in	(%dx),%eax
	inb	%dx,%al
	inw	%dx,%ax
	inl	%dx,%eax
	inb	%dx
	inw	%dx
	inl	%dx
	inb	$255
	inw	$2
	inl	$4
	in	$13, %ax
	out	%al,%dx
	out	%ax,%dx
	out	%eax,%dx
	out	%al,(%dx)
	out	%ax,(%dx)
	out	%eax,(%dx)
	outb	%al,%dx
	outw	%ax,%dx
	outl	%eax,%dx
	outb	%dx
	outw	%dx
	outl	%dx
	outb	$255
	outw	$2
	outl	$4
	out	%ax, $13
# These are used in AIX.
	inw	(%dx)
	outw	(%dx)

	movsb
	cmpsw
	scasl
	xlatb
	movsl	%cs:(%esi),%es:(%edi)
	setae	(%ebx)
	setaeb	(%ebx)
	setae	%al

#these should give warnings
	orb	$1,%ax
	orb	$1,%eax
	orb	$1,%bx
	orb	$1,%ebx
	fldl	%st(1)
	fstl	%st(2)
	fstpl	%st(3)
	fcoml	%st(4)
	fcompl	%st(5)
	faddp	%st(1),%st
	fmulp	%st(2),%st
	fsubp	%st(3),%st
	fsubrp	%st(4),%st
	fdivp	%st(5),%st
	fdivrp	%st(6),%st
	fadd
	fsub
	fmul
	fdiv
	fsubr
	fdivr
#these should all be legal
	btl	%edx, 0x123456
	btl	%edx, %eax
	orb	$1,%al
	orb	$1,%bl
	movl	17,%eax
	mov	17,%eax
	inw	%dx,%ax
	inl	%dx,%eax
	inw	(%dx),%ax
	inl	(%dx),%eax
	in	(%dx),%al
	in	(%dx),%ax
	in	(%dx),%eax
	movzbl	(%edi,%esi),%edx
	movzbl	28(%ebp),%eax
	movzbl	%al,%eax
	movzbl	%cl,%esi
	xlat	%es:(%ebx)
	xlat
	xlatb
1:	fstp	%st(0)
	loop	1b
	divb    %cl 
	divw    %cx 
	divl    %ecx
	div	%cl
	div	%cx
	div	%ecx
	div	%cl,%al
	div	%cx,%ax
	div	%ecx,%eax
	mov	%si,%ds
	movl	%si,%ds		# warning here
	pushl	%ds
	push	%ds
	mov	0,%al
	mov	0x10000,%ax
	mov	%eax,%ebx
	pushf
	pushfl
	pushfw
	popf
	popfl
	popfw
	mov	%esi,(,%ebx,1)
	andb	$~0x80,foo

	and	$0xfffe,%ax
	and	$0xff00,%ax
	and	$0xfffe,%eax
	and	$0xff00,%eax
	and	$0xfffffffe,%eax

.code16
	and	$0xfffe,%ax
	and	$0xff00,%ax
	and	$0xfffe,%eax
	and	$0xff00,%eax
	and	$0xfffffffe,%eax

#check 16-bit code auto address prefix
.code16gcc
	leal	-256(%ebp),%edx
	mov	%al,-129(%ebp)
	mov	%ah,-128(%ebp)
	leal	-1760(%ebp),%ebx
	movl	%eax,140(%esp)

.code32
# Make sure that we won't remove movzb by accident.
	movzb	%al,%di
	movzb	%al,%ecx

	# Force a good alignment.
	.p2align	4,0
