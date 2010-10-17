# Assembler initialization code for actual execution test.
	
# This code becomes the start of the execution test program.  It is
# responsible for initializing the static data, invoking the C code,
# and returning the result.  It is called as though it were a C
# function with an argument of the address of the data segment.

# We need to know the value of _ftext and _fdata at link time, but we
# have no way to actually get that at runtime.  This is because when
# this code is compiled with -membedded-pic, the la instruction will
# be turned into an addiu $gp instruction.  We work around this by
# storing the information in words in the .data section.  We then load
# the values of these words *before* doing the runtime relocation.
	.sdata
text_start:
	.word	_ftext
data_start:
	.word	_fdata

	.globl	start
	.text
start:
	# Grab some space on the stack, just as though we were a real
	# function.
	addiu	$sp,$sp,-8
	sw	$31,0($sp)

	# Save the $gp register, and set it up for our data section.
	sw	$gp,4($sp)

	addu	$gp,$4,0x8000		# macro

	# The start of the data segment is in $4.

	# Get the address of start into $5 in a position independent
	# fashion.
	.set	noreorder
	$LF1 = . + 8
	bal	$LF1
	la	$5,start-$LF1		# macro
	.set	reorder
	addu	$5,$5,$31

	# Now get the address of _ftext into $6.
	la	$6,_ftext-start		# macro
	addu	$6,$6,$5

	# Get the value of _ftext used to link into $7.
	lw	$7,text_start		# macro

	# Get the value of _fdata used to link into $8.
	lw	$8,data_start		# macro

	# Get the address of __runtime_reloc_start into $9.
	la	$9,__runtime_reloc_start-start	# macro
	addu	$9,$9,$5

	# Get the address of __runtime_reloc_stop into $10.
	la	$10,__runtime_reloc_stop-start	# macro
	addu	$10,$10,$5

	# The words between $9 and $10 are the runtime initialization
	# instructions.  Step through and relocate them.  First set
	# $11 and $12 to the values to add to text and data sections,
	# respectively.
	subu	$11,$6,$7
	subu	$12,$4,$8

1:	
	bge	$9,$10,3f		# macro
	lw	$13,0($9)
	and	$14,$13,0xfffffffe	# macro
	move	$15,$11
	beq	$13,$14,2f
	move	$15,$12
2:	
	addu	$14,$14,$4
	lw	$24,0($14)
	addu	$24,$24,$15
	sw	$24,0($14)
	addiu	$9,$9,4
	b	1b
3:	

	# Now the statically initialized data has been relocated
	# correctly, and we can call the C code which does the actual
	# testing.
	bal	foo

	# We return the value returned by the C code.
	lw	$31,0($sp)
	lw	$gp,4($sp)
	addu	$sp,$sp,8
	j	$31
