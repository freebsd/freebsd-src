#
# Copyright (c) 1998 Robert Nordier
# All rights reserved.
#
# Redistribution and use in source and binary forms are freely
# permitted provided that the above copyright notice and this
# paragraph and the following disclaimer are duplicated in all
# such forms.
#
# This software is provided "AS IS" and without any express or
# implied warranties, including, without limitation, the implied
# warranties of merchantability and fitness for a particular
# purpose.
#

# $FreeBSD$

#
# Memory layout.
#
		.set MEM_BTX,0x1000		# Start of BTX memory
		.set MEM_ESP0,0x1800		# Supervisor stack
		.set MEM_BUF,0x1800		# Scratch buffer
		.set MEM_ESP1,0x1e00		# Link stack
		.set MEM_IDT,0x1e00		# IDT
		.set MEM_TSS,0x1f98		# TSS
		.set MEM_MAP,0x2000		# I/O bit map
		.set MEM_DIR,0x4000		# Page directory
		.set MEM_TBL,0x5000		# Page tables
		.set MEM_ORG,0x9000		# BTX code
		.set MEM_USR,0xa000		# Start of user memory
#
# Paging control.
#
		.set PAG_SIZ,0x1000		# Page size
		.set PAG_CNT,0x1000		# Pages to map
#
# Segment selectors.
#
		.set SEL_SCODE,0x8		# Supervisor code
		.set SEL_SDATA,0x10		# Supervisor data
		.set SEL_RCODE,0x18		# Real mode code
		.set SEL_RDATA,0x20		# Real mode data
		.set SEL_UCODE,0x28|3		# User code
		.set SEL_UDATA,0x30|3		# User data
		.set SEL_TSS,0x38		# TSS
#
# Task state segment fields.
#
		.set TSS_ESP0,0x4		# PL 0 ESP
		.set TSS_SS0,0x8		# PL 0 SS
		.set TSS_ESP1,0xc		# PL 1 ESP
		.set TSS_MAP,0x66		# I/O bit map base
#
# System calls.
#
		.set SYS_EXIT,0x0		# Exit
		.set SYS_EXEC,0x1		# Exec
#
# V86 constants.
#
		.set V86_FLG,0x208eff		# V86 flag mask
		.set V86_STK,0x400		# V86 stack allowance
#
# Dump format control bytes.
#
		.set DMP_X16,0x1		# Word
		.set DMP_X32,0x2		# Long
		.set DMP_MEM,0x4		# Memory
		.set DMP_EOL,0x8		# End of line
#
# Screen defaults and assumptions.
#
		.set SCR_MAT,0x7		# Mode/attribute
		.set SCR_COL,0x50		# Columns per row
		.set SCR_ROW,0x19		# Rows per screen
#
# BIOS Data Area locations.
#
		.set BDA_MEM,0x413		# Free memory
		.set BDA_KEYFLAGS,0x417		# Keyboard shift-state flags
		.set BDA_SCR,0x449		# Video mode
		.set BDA_POS,0x450		# Cursor position
		.set BDA_BOOT,0x472		# Boot howto flag
#
# Derivations, for brevity.
#
		.set _ESP0H,MEM_ESP0>>0x8	# Byte 1 of ESP0
		.set _ESP1H,MEM_ESP1>>0x8	# Byte 1 of ESP1
		.set _TSSIO,MEM_MAP-MEM_TSS	# TSS I/O base
		.set _TSSLM,MEM_DIR-MEM_TSS-1	# TSS limit
		.set _IDTLM,MEM_TSS-MEM_IDT-1	# IDT limit
#
# Code segment.
#
		.globl start
		.code16
start:						# Start of code
#
# BTX header.
#
btx_hdr:	.byte 0xeb			# Machine ID
		.byte 0xe			# Header size
		.ascii "BTX"			# Magic
		.byte 0x1			# Major version
		.byte 0x1			# Minor version
		.byte BTX_FLAGS			# Flags
		.word PAG_CNT-MEM_ORG>>0xc	# Paging control
		.word break-start		# Text size
		.long 0x0			# Entry address
#
# Initialization routine.
#
init:		cli				# Disable interrupts
		xor %ax,%ax			# Zero/segment
		mov %ax,%ss			# Set up
		mov $MEM_ESP0,%sp		#  stack
		mov %ax,%es			# Address
		mov %ax,%ds			#  data
		pushl $0x2			# Clear
		popfl				#  flags
#
# Initialize memory.
#
		mov $MEM_IDT,%di		# Memory to initialize
		mov $(MEM_ORG-MEM_IDT)/2,%cx	# Words to zero
		push %di			# Save
		rep				# Zero-fill
		stosw				#  memory
		pop %di				# Restore
#
# Create IDT.
#
		mov $idtctl,%si			# Control string
init.1: 	lodsb				# Get entry
		cbw				#  count
		xchg %ax,%cx			#  as word
		jcxz init.4			# If done
		lodsb				# Get segment
		xchg %ax,%dx	 		#  P:DPL:type
		lodsw				# Get control
		xchg %ax,%bx			#  set
		lodsw				# Get handler offset
		mov $SEL_SCODE,%dh		# Segment selector
init.2: 	shr %bx				# Handle this int?
		jnc init.3			# No
		mov %ax,(%di)			# Set handler offset
		mov %dh,0x2(%di)		#  and selector
		mov %dl,0x5(%di)		# Set P:DPL:type
		add $0x4,%ax			# Next handler
init.3: 	lea 0x8(%di),%di		# Next entry
		loop init.2			# Till set done
		jmp init.1			# Continue
#
# Initialize TSS.
#
init.4: 	movb $_ESP0H,TSS_ESP0+1(%di)	# Set ESP0
		movb $SEL_SDATA,TSS_SS0(%di)	# Set SS0
		movb $_ESP1H,TSS_ESP1+1(%di)	# Set ESP1
		movb $_TSSIO,TSS_MAP(%di)	# Set I/O bit map base
ifdef(`PAGING',`
#
# Create page directory.
#
		xor %edx,%edx			# Page
		mov $PAG_SIZ>>0x8,%dh		#  size
		xor %eax,%eax			# Zero
		mov $MEM_DIR,%di		# Page directory
		mov $PAG_CNT>>0xa,%cl		# Entries
		mov $MEM_TBL|0x7,%ax	 	# First entry
init.5: 	stosl				# Write entry
		add %dx,%ax			# To next
		loop init.5			# Till done
#
# Create page tables.
#
		mov $MEM_TBL,%di		# Page table
		mov $PAG_CNT>>0x8,%ch		# Entries
		xor %ax,%ax			# Start address
init.6: 	mov $0x7,%al			# Set U:W:P flags
		cmp btx_hdr+0x8,%cx	 	# Standard user page?
		jb init.7			# Yes
		cmp $PAG_CNT-MEM_BTX>>0xc,%cx	# BTX memory?
		jae init.7			# No or first page
		and $~0x2,%al			# Clear W flag
		cmp $PAG_CNT-MEM_USR>>0xc,%cx	# User page zero?
		jne init.7			# No
		testb $0x80,btx_hdr+0x7		# Unmap it?
		jz init.7			# No
		and $~0x1,%al			# Clear P flag
init.7: 	stosl				# Set entry
		add %edx,%eax			# Next address
		loop init.6			# Till done
')
#
# Bring up the system.
#
		mov $0x2820,%bx			# Set protected mode
		callw setpic			#  IRQ offsets
		lidt idtdesc	 		# Set IDT
ifdef(`PAGING',`
		xor %eax,%eax			# Set base
		mov $MEM_DIR>>0x8,%ah		#  of page
		mov %eax,%cr3			#  directory
')
		lgdt gdtdesc	 		# Set GDT
		mov %cr0,%eax			# Switch to protected
ifdef(`PAGING',`
		or $0x80000001,%eax             #  mode and enable paging
',`
		or $0x01,%eax			#  mode
')
		mov %eax,%cr0			#  
		ljmp $SEL_SCODE,$init.8		# To 32-bit code
		.code32
init.8: 	xorl %ecx,%ecx			# Zero
		movb $SEL_SDATA,%cl		# To 32-bit
		movw %cx,%ss			#  stack
#
# Launch user task.
#
		movb $SEL_TSS,%cl		# Set task
		ltr %cx				#  register
		movl $MEM_USR,%edx		# User base address
		movzwl %ss:BDA_MEM,%eax 	# Get free memory
		shll $0xa,%eax			# To bytes
		subl $0x1000,%eax		# Less arg space
		subl %edx,%eax			# Less base
		movb $SEL_UDATA,%cl		# User data selector
		pushl %ecx			# Set SS
		pushl %eax			# Set ESP
		push $0x202			# Set flags (IF set)
		push $SEL_UCODE			# Set CS
		pushl btx_hdr+0xc		# Set EIP
		pushl %ecx			# Set GS
		pushl %ecx			# Set FS
		pushl %ecx			# Set DS
		pushl %ecx			# Set ES
		pushl %edx			# Set EAX
		movb $0x7,%cl			# Set remaining
init.9:		push $0x0			#  general
		loop init.9			#  registers
		popa				#  and initialize
		popl %es			# Initialize
		popl %ds			#  user
		popl %fs			#  segment
		popl %gs			#  registers
		iret				# To user mode
#
# Exit routine.
#
exit:		cli				# Disable interrupts
		movl $MEM_ESP0,%esp		# Clear stack
#
# Turn off paging.
#
		movl %cr0,%eax			# Get CR0
		andl $~0x80000000,%eax		# Disable
		movl %eax,%cr0			#  paging
		xorl %ecx,%ecx			# Zero
		movl %ecx,%cr3			# Flush TLB
#
# Restore the GDT in case we caught a kernel trap.
#
		lgdt gdtdesc	 		# Set GDT
#
# To 16 bits.
#
		ljmpw $SEL_RCODE,$exit.1	# Reload CS
		.code16
exit.1: 	mov $SEL_RDATA,%cl		# 16-bit selector
		mov %cx,%ss			# Reload SS
		mov %cx,%ds			# Load
		mov %cx,%es			#  remaining
		mov %cx,%fs			#  segment
		mov %cx,%gs			#  registers
#
# To real-address mode.
#
		dec %ax				# Switch to
		mov %eax,%cr0			#  real mode
		ljmp $0x0,$exit.2		# Reload CS
exit.2: 	xor %ax,%ax			# Real mode segment
		mov %ax,%ss			# Reload SS
		mov %ax,%ds			# Address data
		mov $0x7008,%bx			# Set real mode
		callw setpic			#  IRQ offsets
		lidt ivtdesc	 		# Set IVT
#
# Reboot or await reset.
#
		sti				# Enable interrupts
		testb $0x1,btx_hdr+0x7		# Reboot?
exit.3:		jz exit.3			# No
		movw $0x1234, BDA_BOOT		# Do a warm boot
		ljmp $0xffff,$0x0		# reboot the machine
#
# Set IRQ offsets by reprogramming 8259A PICs.
#
setpic: 	in $0x21,%al			# Save master
		push %ax			#  IMR
		in $0xa1,%al			# Save slave
		push %ax			#  IMR
		movb $0x11,%al			# ICW1 to
		outb %al,$0x20			#  master,
		outb %al,$0xa0			#  slave
		movb %bl,%al			# ICW2 to
		outb %al,$0x21			#  master
		movb %bh,%al			# ICW2 to
		outb %al,$0xa1			#  slave
		movb $0x4,%al			# ICW3 to
		outb %al,$0x21			#  master
		movb $0x2,%al			# ICW3 to
		outb %al,$0xa1			#  slave
		movb $0x1,%al			# ICW4 to
		outb %al,$0x21			#  master,
		outb %al,$0xa1			#  slave
		pop %ax				# Restore slave
		outb %al,$0xa1			#  IMR
		pop %ax				# Restore master
		outb %al,$0x21			#  IMR
		retw				# To caller
		.code32
#
# Initiate return from V86 mode to user mode.
#
inthlt: 	hlt				# To supervisor mode
#
# Exception jump table.
#
intx00: 	push $0x0			# Int 0x0: #DE
		jmp ex_noc			# Divide error
		push $0x1			# Int 0x1: #DB
		jmp ex_noc			# Debug
		push $0x3			# Int 0x3: #BP
		jmp ex_noc			# Breakpoint
		push $0x4			# Int 0x4: #OF
		jmp ex_noc			# Overflow
		push $0x5			# Int 0x5: #BR
		jmp ex_noc			# BOUND range exceeded
		push $0x6			# Int 0x6: #UD
		jmp ex_noc			# Invalid opcode
		push $0x7			# Int 0x7: #NM
		jmp ex_noc			# Device not available
		push $0x8			# Int 0x8: #DF
		jmp except			# Double fault
		push $0xa			# Int 0xa: #TS
		jmp except			# Invalid TSS
		push $0xb			# Int 0xb: #NP
		jmp except			# Segment not present
		push $0xc			# Int 0xc: #SS
		jmp except			# Stack segment fault
		push $0xd			# Int 0xd: #GP
		jmp ex_v86			# General protection
		push $0xe			# Int 0xe: #PF
		jmp except			# Page fault
intx10: 	push $0x10			# Int 0x10: #MF
		jmp ex_noc			# Floating-point error
#
# Handle #GP exception.
#
ex_v86: 	testb $0x2,0x12(%esp,1) 	# V86 mode?
		jz except			# No
		jmp v86mon			# To monitor
#
# Save a zero error code.
#
ex_noc: 	pushl (%esp,1)			# Duplicate int no
		movb $0x0,0x4(%esp,1)		# Fake error code
#
# Handle exception.
#
except: 	cld				# String ops inc
		pushl %ds			# Save
		pushl %es			#  most
		pusha				#  registers
		movb $0x6,%al			# Push loop count
		testb $0x2,0x3a(%esp,1) 	# V86 mode?
		jnz except.1			# Yes
		pushl %gs			# Set GS
		pushl %fs			# Set FS
		pushl %ds			# Set DS
		pushl %es			# Set ES
		movb $0x2,%al			# Push loop count
		cmpw $SEL_SCODE,0x44(%esp,1)	# Supervisor mode?
		jne except.1			# No
		pushl %ss			# Set SS
		leal 0x50(%esp,1),%eax		# Set
		pushl %eax			#  ESP
		jmp except.2			# Join common code
except.1:	pushl 0x50(%esp,1)		# Set GS, FS, DS, ES
		decb %al			#  (if V86 mode), and
		jne except.1			#  SS, ESP
except.2:	push $SEL_SDATA			# Set up
		popl %ds			#  to
		pushl %ds			#  address
		popl %es			#  data
		movl %esp,%ebx			# Stack frame
		movl $dmpfmt,%esi		# Dump format string
		movl $MEM_BUF,%edi		# Buffer
		pushl %edi			# Dump to
		call dump			#  buffer
		popl %esi			#  and
		call putstr			#  display
		leal 0x18(%esp,1),%esp		# Discard frame
		popa				# Restore
		popl %es			#  registers
		popl %ds			#  saved
		cmpb $0x3,(%esp,1)		# Breakpoint?
		je except.3			# Yes
		cmpb $0x1,(%esp,1)		# Debug?
		jne except.2a			# No
		testl $0x100,0x10(%esp,1)	# Trap flag set?
		jnz except.3			# Yes
except.2a:	jmp exit			# Exit
except.3:	leal 0x8(%esp,1),%esp		# Discard err, int no
		iret				# From interrupt
#
# Return to user mode from V86 mode.
#
intrtn: 	cld				# String ops inc
		pushl %ds			# Address
		popl %es			#  data
		leal 0x3c(%ebp),%edx		# V86 Segment registers
		movl MEM_TSS+TSS_ESP1,%esi	# Link stack pointer
		lodsl				# INT_V86 args pointer
		movl %esi,%ebx			# Saved exception frame
		testl %eax,%eax 		# INT_V86 args?
		jz intrtn.2			# No
		movl $MEM_USR,%edi		# User base
		movl 0x1c(%esi),%ebx		# User ESP
		movl %eax,(%edi,%ebx,1) 	# Restore to user stack
		leal 0x8(%edi,%eax,1),%edi	# Arg segment registers
		testb $0x4,-0x6(%edi)		# Return flags?
		jz intrtn.1			# No
		movl 0x30(%ebp),%eax		# Get V86 flags
		movw %ax,0x18(%esi)		# Set user flags
intrtn.1:	leal 0x10(%esi),%ebx		# Saved exception frame
		xchgl %edx,%esi 		# Segment registers
		movb $0x4,%cl			# Update seg regs
		rep				#  in INT_V86
		movsl				#  args
intrtn.2:	movl %edx,%esi			# Segment registers
		leal 0x28(%ebp),%edi		# Set up seg
		movb $0x4,%cl			#  regs for
		rep				#  later
		movsl				#  pop
		movl %ebx,%esi			# Restore exception
		movb $0x5,%cl			#  frame to
		rep				#  supervisor
		movsl				#  stack
		movl %esi,MEM_TSS+TSS_ESP1	# Link stack pointer
		popa				# Restore
		leal 0x8(%esp,1),%esp		# Discard err, int no
		popl %es			# Restore
		popl %ds			#  user
		popl %fs			#  segment
		popl %gs			#  registers
		iret				# To user mode
#
# V86 monitor.
#
v86mon: 	cld				# String ops inc
		pushl $SEL_SDATA		# Set up for
		popl %ds			#  flat addressing
		pusha				# Save registers
		movl %esp,%ebp			# Address stack frame
		movzwl 0x2c(%ebp),%edi		# Load V86 CS
		shll $0x4,%edi			# To linear
		movl 0x28(%ebp),%esi		# Load V86 IP
		addl %edi,%esi			# Code pointer
		xorl %ecx,%ecx			# Zero
		movb $0x2,%cl			# 16-bit operands
		xorl %eax,%eax			# Zero
v86mon.1:	lodsb				# Get opcode
		cmpb $0x66,%al			# Operand size prefix?
		jne v86mon.2			# No
		movb $0x4,%cl			# 32-bit operands
		jmp v86mon.1			# Continue
v86mon.2:	cmpb $0xf4,%al			# HLT?
		jne v86mon.3			# No
		cmpl $inthlt+0x1,%esi		# Is inthlt?
		jne v86mon.7			# No (ignore)
		jmp intrtn			# Return to user mode
v86mon.3:	cmpb $0xf,%al			# Prefixed instruction?
		jne v86mon.4			# No
		cmpb $0x09,(%esi)		# Is it a WBINVD?
		je v86wbinvd			# Yes
		cmpb $0x30,(%esi)		# Is it a WRMSR?
		je v86wrmsr			# Yes
		cmpb $0x32,(%esi)		# Is it a RDMSR?
		je v86rdmsr			# Yes
		cmpb $0x20,(%esi)		# Is this a
		jne v86mon.4			#  MOV EAX,CR0
		cmpb $0xc0,0x1(%esi)		#  instruction?
		je v86mov			# Yes
v86mon.4:	cmpb $0xfa,%al			# CLI?
		je v86cli			# Yes
		cmpb $0xfb,%al			# STI?
		je v86sti			# Yes
		movzwl 0x38(%ebp),%ebx		# Load V86 SS
		shll $0x4,%ebx			# To offset
		pushl %ebx			# Save
		addl 0x34(%ebp),%ebx		# Add V86 SP
		movl 0x30(%ebp),%edx		# Load V86 flags
		cmpb $0x9c,%al			# PUSHF/PUSHFD?
		je v86pushf			# Yes
		cmpb $0x9d,%al			# POPF/POPFD?
		je v86popf			# Yes
		cmpb $0xcd,%al			# INT imm8?
		je v86intn			# Yes
		cmpb $0xcf,%al			# IRET/IRETD?
		je v86iret			# Yes
		popl %ebx			# Restore
		popa				# Restore
		jmp except			# Handle exception
v86mon.5:	movl %edx,0x30(%ebp)		# Save V86 flags
v86mon.6:	popl %edx			# V86 SS adjustment
		subl %edx,%ebx			# Save V86
		movl %ebx,0x34(%ebp)		#  SP
v86mon.7:	subl %edi,%esi			# From linear
		movl %esi,0x28(%ebp)		# Save V86 IP
		popa				# Restore
		leal 0x8(%esp,1),%esp		# Discard int no, error
		iret				# To V86 mode
#
# Emulate MOV EAX,CR0.
#
v86mov: 	movl %cr0,%eax			# CR0 to
		movl %eax,0x1c(%ebp)		#  saved EAX
		incl %esi			# Adjust IP
#
# Return from emulating a 0x0f prefixed instruction
#
v86preret:	incl %esi			# Adjust IP
		jmp v86mon.7			# Finish up
#
# Emulate WBINVD
#
v86wbinvd:	wbinvd				# Write back and invalidate
						#  cache
		jmp v86preret			# Finish up
#
# Emulate WRMSR
#
v86wrmsr:	movl 0x18(%ebp),%ecx		# Get user's %ecx (MSR to write)
		movl 0x14(%ebp),%edx		# Load the value
		movl 0x1c(%ebp),%eax		#  to write
		wrmsr				# Write MSR
		jmp v86preret			# Finish up
#
# Emulate RDMSR
#
v86rdmsr:	movl 0x18(%ebp),%ecx		# MSR to read
		rdmsr				# Read the MSR
		movl %eax,0x1c(%ebp)		# Return the value of
		movl %edx,0x14(%ebp)		#  the MSR to the user
		jmp v86preret			# Finish up
#
# Emulate CLI.
#
v86cli: 	andb $~0x2,0x31(%ebp)		# Clear IF
		jmp v86mon.7			# Finish up
#
# Emulate STI.
#
v86sti: 	orb $0x2,0x31(%ebp)		# Set IF
		jmp v86mon.7			# Finish up
#
# Emulate PUSHF/PUSHFD.
#
v86pushf:	subl %ecx,%ebx			# Adjust SP
		cmpb $0x4,%cl			# 32-bit
		je v86pushf.1			# Yes
		data16				# 16-bit
v86pushf.1:	movl %edx,(%ebx)		# Save flags
		jmp v86mon.6			# Finish up
#
# Emulate IRET/IRETD.
#
v86iret:	movzwl (%ebx),%esi		# Load V86 IP
		movzwl 0x2(%ebx),%edi		# Load V86 CS
		leal 0x4(%ebx),%ebx		# Adjust SP
		movl %edi,0x2c(%ebp)		# Save V86 CS
		xorl %edi,%edi			# No ESI adjustment
#
# Emulate POPF/POPFD (and remainder of IRET/IRETD).
#
v86popf:	cmpb $0x4,%cl			# 32-bit?
		je v86popf.1			# Yes
		movl %edx,%eax			# Initialize
		data16				# 16-bit
v86popf.1:	movl (%ebx),%eax		# Load flags
		addl %ecx,%ebx			# Adjust SP
		andl $V86_FLG,%eax		# Merge
		andl $~V86_FLG,%edx		#  the
		orl %eax,%edx			#  flags
		jmp v86mon.5			# Finish up
#
# trap int 15, function 87
# reads %es:%si from saved registers on stack to find a GDT containing
# source and destination locations
# reads count of words from saved %cx
# returns success by setting %ah to 0
#
int15_87:	pushl %eax			# Save 
		pushl %ebx			#  some information 
		pushl %esi			#  onto the stack.
		pushl %edi
		xorl %eax,%eax			# clean EAX 
		xorl %ebx,%ebx			# clean EBX 
		movl 0x4(%ebp),%esi		# Get user's ESI
		movl 0x3C(%ebp),%ebx		# store ES
		movw %si,%ax			# store SI
		shll $0x4,%ebx			# Make it a seg.
		addl %eax,%ebx			# ebx=(es<<4)+si
		movb 0x14(%ebx),%al		# Grab the
		movb 0x17(%ebx),%ah		#  necessary
		shll $0x10,%eax			#  information
		movw 0x12(%ebx),%ax		#  from
		movl %eax,%esi			#  the
		movb 0x1c(%ebx),%al		#  GDT in order to
		movb 0x1f(%ebx),%ah		#  have %esi offset
		shll $0x10,%eax			#  of source and %edi
		movw 0x1a(%ebx),%ax		#  of destination.
		movl %eax,%edi
		pushl %ds			# Make:
		popl %es			# es = ds
		pushl %ecx			# stash ECX
		xorl %ecx,%ecx			# highw of ECX is clear
		movw 0x18(%ebp),%cx		# Get user's ECX
		shll $0x1,%ecx			# Convert from num words to num
						#  bytes
		rep				# repeat...
		movsb				#  perform copy.
		popl %ecx			# Restore
		popl %edi
		popl %esi			#  previous
		popl %ebx			#  register
		popl %eax			#  values.
		movb $0x0,0x1d(%ebp)		# set ah = 0 to indicate
						#  success
		andb $0xfe,%dl			# clear CF
		jmp v86mon.5			# Finish up

#
# Reboot the machine by setting the reboot flag and exiting
#
reboot:		orb $0x1,btx_hdr+0x7		# Set the reboot flag
		jmp exit			# Terminate BTX and reboot

#
# Emulate INT imm8... also make sure to check if it's int 15/87
#
v86intn:	lodsb				# Get int no
		cmpb $0x19,%al			# is it int 19?
		je reboot			#  yes, reboot the machine
		cmpb $0x15,%al			# is it int 15?
		jne v86intn.3			#  no, skip parse
		pushl %eax                      # stash EAX
		movl 0x1c(%ebp),%eax		# user's saved EAX
		cmpb $0x87,%ah			# is it the memcpy subfunction?
		jne v86intn.1			#  no, keep checking
		popl %eax			# get the stack straight
		jmp int15_87			# it's our cue
v86intn.1:	cmpw $0x4f53,%ax		# is it the delete key callout?
		jne v86intn.2			#  no, handle the int normally
		movb BDA_KEYFLAGS,%al		# get the shift key state
		andb $0xc,%al			# mask off just Ctrl and Alt
		cmpb $0xc,%al			# are both Ctrl and Alt down?
		jne v86intn.2			#  no, handle the int normally
		popl %eax			# restore EAX
		jmp reboot			# reboot the machine
v86intn.2:	popl %eax			# restore EAX
v86intn.3:	subl %edi,%esi			# From
		shrl $0x4,%edi			#  linear
		movw %dx,-0x2(%ebx)		# Save flags
		movw %di,-0x4(%ebx)		# Save CS
		leal -0x6(%ebx),%ebx		# Adjust SP
		movw %si,(%ebx) 		# Save IP
		shll $0x2,%eax			# Scale
		movzwl (%eax),%esi		# Load IP
		movzwl 0x2(%eax),%edi		# Load CS
		movl %edi,0x2c(%ebp)		# Save CS
		xorl %edi,%edi			# No ESI adjustment
		andb $~0x1,%dh			# Clear TF
		jmp v86mon.5			# Finish up
#
# Hardware interrupt jump table.
#
intx20: 	push $0x8			# Int 0x20: IRQ0
		jmp int_hw			# V86 int 0x8
		push $0x9			# Int 0x21: IRQ1
		jmp int_hw			# V86 int 0x9
		push $0xa			# Int 0x22: IRQ2
		jmp int_hw			# V86 int 0xa
		push $0xb			# Int 0x23: IRQ3
		jmp int_hw			# V86 int 0xb
		push $0xc			# Int 0x24: IRQ4
		jmp int_hw			# V86 int 0xc
		push $0xd			# Int 0x25: IRQ5
		jmp int_hw			# V86 int 0xd
		push $0xe			# Int 0x26: IRQ6
		jmp int_hw			# V86 int 0xe
		push $0xf			# Int 0x27: IRQ7
		jmp int_hw			# V86 int 0xf
		push $0x70			# Int 0x28: IRQ8
		jmp int_hw			# V86 int 0x70
		push $0x71			# Int 0x29: IRQ9
		jmp int_hw			# V86 int 0x71
		push $0x72			# Int 0x2a: IRQ10
		jmp int_hw			# V86 int 0x72
		push $0x73			# Int 0x2b: IRQ11
		jmp int_hw			# V86 int 0x73
		push $0x74			# Int 0x2c: IRQ12
		jmp int_hw			# V86 int 0x74
		push $0x75			# Int 0x2d: IRQ13
		jmp int_hw			# V86 int 0x75
		push $0x76			# Int 0x2e: IRQ14
		jmp int_hw			# V86 int 0x76
		push $0x77			# Int 0x2f: IRQ15
		jmp int_hw			# V86 int 0x77
#
# Reflect hardware interrupts.
#
int_hw: 	testb $0x2,0xe(%esp,1)		# V86 mode?
		jz intusr			# No
		pushl $SEL_SDATA		# Address
		popl %ds			#  data
		xchgl %eax,(%esp,1)		# Swap EAX, int no
		pushl %ebp			# Address
		movl %esp,%ebp			#  stack frame
		pushl %ebx			# Save
		shll $0x2,%eax			# Get int
		movl (%eax),%eax		#  vector
		subl $0x6,0x14(%ebp)		# Adjust V86 ESP
		movzwl 0x18(%ebp),%ebx		# V86 SS
		shll $0x4,%ebx			#  * 0x10
		addl 0x14(%ebp),%ebx		#  + V86 ESP
		xchgw %ax,0x8(%ebp)		# Swap V86 IP
		rorl $0x10,%eax 		# Swap words
		xchgw %ax,0xc(%ebp)		# Swap V86 CS
		roll $0x10,%eax 		# Swap words
		movl %eax,(%ebx)		# CS:IP for IRET
		movl 0x10(%ebp),%eax		# V86 flags
		movw %ax,0x4(%ebx)		# Flags for IRET
		andb $~0x3,0x11(%ebp)		# Clear IF, TF
		popl %ebx			# Restore
		popl %ebp			#  saved
		popl %eax			#  registers
		iret				# To V86 mode
#
# Invoke V86 interrupt from user mode, with arguments.
#
intx31: 	stc				# Have btx_v86
		pushl %eax			# Missing int no
#
# Invoke V86 interrupt from user mode.
#
intusr: 	std				# String ops dec
		pushl %eax			# Expand
		pushl %eax			#  stack
		pushl %eax			#  frame
		pusha				# Save
		pushl %gs			# Save
		movl %esp,%eax			#  seg regs
		pushl %fs			#  and
		pushl %ds			#  point
		pushl %es			#  to them
		push $SEL_SDATA			# Set up
		popl %ds			#  to
		pushl %ds			#  address
		popl %es			#  data
		movl $MEM_USR,%ebx		# User base
		movl %ebx,%edx			#  address
		jc intusr.1			# If btx_v86
		xorl %edx,%edx			# Control flags
		xorl %ebp,%ebp			# btx_v86 pointer
intusr.1:	leal 0x50(%esp,1),%esi		# Base of frame
		pushl %esi			# Save
		addl -0x4(%esi),%ebx		# User ESP
		movl MEM_TSS+TSS_ESP1,%edi	# Link stack pointer
		leal -0x4(%edi),%edi		# Adjust for push
		xorl %ecx,%ecx			# Zero
		movb $0x5,%cl			# Push exception
		rep				#  frame on
		movsl				#  link stack
		xchgl %eax,%esi 		# Saved seg regs
		movl 0x40(%esp,1),%eax		# Get int no
		testl %edx,%edx 		# Have btx_v86?
		jz intusr.2			# No
		movl (%ebx),%ebp		# btx_v86 pointer
		movb $0x4,%cl			# Count
		addl %ecx,%ebx			# Adjust for pop
		rep				# Push saved seg regs
		movsl				#  on link stack
		addl %ebp,%edx			# Flatten btx_v86 ptr
		leal 0x14(%edx),%esi		# Seg regs pointer
		movl 0x4(%edx),%eax		# Get int no/address
		movzwl 0x2(%edx),%edx		# Get control flags
intusr.2:	movl %ebp,(%edi)		# Push btx_v86 and
		movl %edi,MEM_TSS+TSS_ESP1	#  save link stack ptr
		popl %edi			# Base of frame
		xchgl %eax,%ebp 		# Save intno/address
		movl 0x48(%esp,1),%eax		# Get flags
		testb $0x2,%dl			# Simulate CALLF?
		jnz intusr.3			# Yes
		decl %ebx			# Push flags
		decl %ebx			#  on V86
		movw %ax,(%ebx) 		#  stack
intusr.3:	movb $0x4,%cl			# Count
		subl %ecx,%ebx			# Push return address
		movl $inthlt,(%ebx)		#  on V86 stack
		rep				# Copy seg regs to
		movsl				#  exception frame
		xchgl %eax,%ecx 		# Save flags
		movl %ebx,%eax			# User ESP
		subl $V86_STK,%eax		# Less bytes
		ja intusr.4			#  to
		xorl %eax,%eax			#  keep
intusr.4:	shrl $0x4,%eax			# Gives segment
		stosl				# Set SS
		shll $0x4,%eax			# To bytes
		xchgl %eax,%ebx 		# Swap
		subl %ebx,%eax			# Gives offset
		stosl				# Set ESP
		xchgl %eax,%ecx 		# Get flags
		btsl $0x11,%eax 		# Set VM
		andb $~0x1,%ah			# Clear TF
		stosl				# Set EFL
		xchgl %eax,%ebp 		# Get int no/address
		testb $0x1,%dl			# Address?
		jnz intusr.5			# Yes
		shll $0x2,%eax			# Scale
		movl (%eax),%eax		# Load int vector
intusr.5:	movl %eax,%ecx			# Save
		shrl $0x10,%eax 		# Gives segment
		stosl				# Set CS
		movw %cx,%ax			# Restore
		stosl				# Set EIP
		leal 0x10(%esp,1),%esp		# Discard seg regs
		popa				# Restore
		iret				# To V86 mode
#
# System Call.
#
intx30: 	cmpl $SYS_EXEC,%eax		# Exec system call?
		jne intx30.1			# No
		pushl %ss			# Set up
		popl %es			#  all
		pushl %es			#  segment
		popl %ds			#  registers
		pushl %ds			#  for the
		popl %fs			#  program
		pushl %fs			#  we're
		popl %gs			#  invoking
		movl $MEM_USR,%eax		# User base address
		addl 0xc(%esp,1),%eax		# Change to user
		leal 0x4(%eax),%esp		#  stack
ifdef(`PAGING',`
		movl %cr0,%eax			# Turn
		andl $~0x80000000,%eax		#  off
		movl %eax,%cr0			#  paging
		xorl %eax,%eax			# Flush
		movl %eax,%cr3			#  TLB
')
		popl %eax			# Call
		call *%eax			#  program
intx30.1:	orb $0x1,%ss:btx_hdr+0x7	# Flag reboot
		jmp exit			# Exit
#
# Dump structure [EBX] to [EDI], using format string [ESI].
#
dump.0: 	stosb				# Save char
dump:		lodsb				# Load char
		testb %al,%al			# End of string?
		jz dump.10			# Yes
		testb $0x80,%al 		# Control?
		jz dump.0			# No
		movb %al,%ch			# Save control
		movb $'=',%al			# Append
		stosb				#  '='
		lodsb				# Get offset
		pushl %esi			# Save
		movsbl %al,%esi 		# To
		addl %ebx,%esi			#  pointer
		testb $DMP_X16,%ch		# Dump word?
		jz dump.1			# No
		lodsw				# Get and
		call hex16			#  dump it
dump.1: 	testb $DMP_X32,%ch		# Dump long?
		jz dump.2			# No
		lodsl				# Get and
		call hex32			#  dump it
dump.2: 	testb $DMP_MEM,%ch		# Dump memory?
		jz dump.8			# No
		pushl %ds			# Save
		testb $0x2,0x52(%ebx)		# V86 mode?
		jnz dump.3			# Yes
		verr 0x4(%esi)	 		# Readable selector?
		jnz dump.3			# No
		ldsl (%esi),%esi		# Load pointer
		jmp dump.4			# Join common code
dump.3: 	lodsl				# Set offset
		xchgl %eax,%edx 		# Save
		lodsl				# Get segment
		shll $0x4,%eax			#  * 0x10
		addl %edx,%eax			#  + offset
		xchgl %eax,%esi 		# Set pointer
dump.4: 	movb $2,%dl			# Num lines
dump.4a:	movb $0x10,%cl			# Bytes to dump
dump.5: 	lodsb				# Get byte and
		call hex8			#  dump it
		decb %cl			# Keep count
		jz dump.6a			# If done
		movb $'-',%al			# Separator
		cmpb $0x8,%cl			# Half way?
		je dump.6			# Yes
		movb $' ',%al			# Use space
dump.6: 	stosb				# Save separator
		jmp dump.5			# Continue
dump.6a:	decb %dl			# Keep count
		jz dump.7			# If done
		movb $0xa,%al			# Line feed
		stosb				# Save one
		movb $7,%cl			# Leading
		movb $' ',%al			#  spaces
dump.6b:	stosb				# Dump
		decb %cl			#  spaces
		jnz dump.6b
		jmp dump.4a			# Next line
dump.7: 	popl %ds			# Restore
dump.8: 	popl %esi			# Restore
		movb $0xa,%al			# Line feed
		testb $DMP_EOL,%ch		# End of line?
		jnz dump.9			# Yes
		movb $' ',%al			# Use spaces
		stosb				# Save one
dump.9: 	jmp dump.0			# Continue
dump.10:	stosb				# Terminate string
		ret				# To caller
#
# Convert EAX, AX, or AL to hex, saving the result to [EDI].
#
hex32:		pushl %eax			# Save
		shrl $0x10,%eax 		# Do upper
		call hex16			#  16
		popl %eax			# Restore
hex16:		call hex16.1			# Do upper 8
hex16.1:	xchgb %ah,%al			# Save/restore
hex8:		pushl %eax			# Save
		shrb $0x4,%al			# Do upper
		call hex8.1			#  4
		popl %eax			# Restore
hex8.1: 	andb $0xf,%al			# Get lower 4
		cmpb $0xa,%al			# Convert
		sbbb $0x69,%al			#  to hex
		das				#  digit
		orb $0x20,%al			# To lower case
		stosb				# Save char
		ret				# (Recursive)
#
# Output zero-terminated string [ESI] to the console.
#
putstr.0:	call putchr			# Output char
putstr: 	lodsb				# Load char
		testb %al,%al			# End of string?
		jnz putstr.0			# No
		ret				# To caller
#
# Output character AL to the console.
#
putchr: 	pusha				# Save
		xorl %ecx,%ecx			# Zero for loops
		movb $SCR_MAT,%ah		# Mode/attribute
		movl $BDA_POS,%ebx		# BDA pointer
		movw (%ebx),%dx 		# Cursor position
		movl $0xb8000,%edi		# Regen buffer (color)
		cmpb %ah,BDA_SCR-BDA_POS(%ebx)	# Mono mode?
		jne putchr.1			# No
		xorw %di,%di			# Regen buffer (mono)
putchr.1:	cmpb $0xa,%al			# New line?
		je putchr.2			# Yes
		xchgl %eax,%ecx 		# Save char
		movb $SCR_COL,%al		# Columns per row
		mulb %dh			#  * row position
		addb %dl,%al			#  + column
		adcb $0x0,%ah			#  position
		shll %eax			#  * 2
		xchgl %eax,%ecx 		# Swap char, offset
		movw %ax,(%edi,%ecx,1)		# Write attr:char
		incl %edx			# Bump cursor
		cmpb $SCR_COL,%dl		# Beyond row?
		jb putchr.3			# No
putchr.2:	xorb %dl,%dl			# Zero column
		incb %dh			# Bump row
putchr.3:	cmpb $SCR_ROW,%dh		# Beyond screen?
		jb putchr.4			# No
		leal 2*SCR_COL(%edi),%esi	# New top line
		movw $(SCR_ROW-1)*SCR_COL/2,%cx # Words to move
		rep				# Scroll
		movsl				#  screen
		movb $' ',%al			# Space
		movb $SCR_COL,%cl		# Columns to clear
		rep				# Clear
		stosw				#  line
		movb $SCR_ROW-1,%dh		# Bottom line
putchr.4:	movw %dx,(%ebx) 		# Update position
		popa				# Restore
		ret				# To caller

		.p2align 4
#
# Global descriptor table.
#
gdt:		.word 0x0,0x0,0x0,0x0		# Null entry
		.word 0xffff,0x0,0x9a00,0xcf	# SEL_SCODE
		.word 0xffff,0x0,0x9200,0xcf	# SEL_SDATA
		.word 0xffff,0x0,0x9a00,0x0	# SEL_RCODE
		.word 0xffff,0x0,0x9200,0x0	# SEL_RDATA
		.word 0xffff,MEM_USR,0xfa00,0xcf# SEL_UCODE
		.word 0xffff,MEM_USR,0xf200,0xcf# SEL_UDATA
		.word _TSSLM,MEM_TSS,0x8900,0x0 # SEL_TSS
gdt.1:
#
# Pseudo-descriptors.
#
gdtdesc:	.word gdt.1-gdt-1,gdt,0x0	# GDT
idtdesc:	.word _IDTLM,MEM_IDT,0x0	# IDT
ivtdesc:	.word 0x400-0x0-1,0x0,0x0	# IVT
#
# IDT construction control string.
#
idtctl: 	.byte 0x10,  0x8e		# Int 0x0-0xf
		.word 0x7dfb,intx00		#  (exceptions)
		.byte 0x10,  0x8e		# Int 0x10
		.word 0x1,   intx10		#  (exception)
		.byte 0x10,  0x8e		# Int 0x20-0x2f
		.word 0xffff,intx20		#  (hardware)
		.byte 0x1,   0xee		# int 0x30
		.word 0x1,   intx30		#  (system call)
		.byte 0x2,   0xee		# Int 0x31-0x32
		.word 0x1,   intx31		#  (V86, null)
		.byte 0x0			# End of string
#
# Dump format string.
#
dmpfmt: 	.byte '\n'			# "\n"
		.ascii "int"			# "int="
		.byte 0x80|DMP_X32,	   0x40 # "00000000  "
		.ascii "err"			# "err="
		.byte 0x80|DMP_X32,	   0x44 # "00000000  "
		.ascii "efl"			# "efl="
		.byte 0x80|DMP_X32,	   0x50 # "00000000  "
		.ascii "eip"			# "eip="
		.byte 0x80|DMP_X32|DMP_EOL,0x48 # "00000000\n"
		.ascii "eax"			# "eax="
		.byte 0x80|DMP_X32,	   0x34 # "00000000  "
		.ascii "ebx"			# "ebx="
		.byte 0x80|DMP_X32,	   0x28 # "00000000  "
		.ascii "ecx"			# "ecx="
		.byte 0x80|DMP_X32,	   0x30 # "00000000  "
		.ascii "edx"			# "edx="
		.byte 0x80|DMP_X32|DMP_EOL,0x2c # "00000000\n"
		.ascii "esi"			# "esi="
		.byte 0x80|DMP_X32,	   0x1c # "00000000  "
		.ascii "edi"			# "edi="
		.byte 0x80|DMP_X32,	   0x18 # "00000000  "
		.ascii "ebp"			# "ebp="
		.byte 0x80|DMP_X32,	   0x20 # "00000000  "
		.ascii "esp"			# "esp="
		.byte 0x80|DMP_X32|DMP_EOL,0x0	# "00000000\n"
		.ascii "cs"			# "cs="
		.byte 0x80|DMP_X16,	   0x4c # "0000  "
		.ascii "ds"			# "ds="
		.byte 0x80|DMP_X16,	   0xc	# "0000  "
		.ascii "es"			# "es="
		.byte 0x80|DMP_X16,	   0x8	# "0000  "
		.ascii "  "			# "  "
		.ascii "fs"			# "fs="
		.byte 0x80|DMP_X16,	   0x10 # "0000  "
		.ascii "gs"			# "gs="
		.byte 0x80|DMP_X16,	   0x14 # "0000  "
		.ascii "ss"			# "ss="
		.byte 0x80|DMP_X16|DMP_EOL,0x4	# "0000\n"
		.ascii "cs:eip" 		# "cs:eip="
		.byte 0x80|DMP_MEM|DMP_EOL,0x48 # "00 00 ... 00 00\n"
		.ascii "ss:esp" 		# "ss:esp="
		.byte 0x80|DMP_MEM|DMP_EOL,0x0	# "00 00 ... 00 00\n"
		.asciz "BTX halted\n"		# End
#
# End of BTX memory.
#
		.p2align 4
break:
