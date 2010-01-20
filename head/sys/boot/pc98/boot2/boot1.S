/*-
 * Copyright (c) 2008-2009 TAKAHASHI Yoshihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* Memory Locations */
		.set STACK_OFF,0x6000		# Stack offset
		.set LOAD_SIZE,8192		# Load size
		.set DAUA,0x0584		# DA/UA
		.set MEM_REL,0x700		# Relocation address
		.set MEM_ARG,0x900		# Arguments
		.set MEM_BUF,0x8cec		# Load area
		.set MEM_BTX,0x9000		# BTX start
		.set MEM_JMP,0x9010		# BTX entry point
		.set MEM_USR,0xa000		# Client start

/* PC98 machine type from sys/pc98/pc98/pc98_machdep.h */
		.set MEM_SYS,		0xa100	# System common area segment
		.set PC98_MACHINE_TYPE,	0x0620	# PC98 machine type
		.set EPSON_ID,		0x0624	# EPSON machine id

		.set M_NEC_PC98,	0x0001
		.set M_EPSON_PC98,	0x0002
		.set M_NOT_H98,		0x0010
		.set M_H98,		0x0020
		.set M_NOTE,		0x0040
		.set M_NORMAL,		0x1000
		.set M_8M,		0x8000

/* Partition Constants */
		.set PRT_OFF,0x1be		# Partition offset

/* Misc. Constants */
		.set SIZ_PAG,0x1000		# Page size
		.set SIZ_SEC,0x200		# Sector size

		.set NSECT,0x10

		.globl start
		.globl read
		.globl putc
		.code16

start:		jmp main

boot_cyl:	.org 4
		.ascii "IPL1   "

main:		cld

		/* Setup the stack */
		xor %si,%si
		mov %si,%ss
		mov $STACK_OFF,%sp

		push %cx

		/* Relocate ourself to MEM_REL */
		push %cs
		pop %ds
		mov %si,%es
		mov $MEM_REL,%di
		mov $SIZ_SEC,%cx
		rep
		movsb

		/* Transfer PC-9801 system common area */
		xor %ax,%ax
		mov %ax,%si
		mov %ax,%ds
		mov %ax,%di
		mov $MEM_SYS,%ax
		mov %ax,%es
		mov $0x0600,%cx
		rep
		movsb

		/* Transfer EPSON machine type */
		mov $0xfd00,%ax
		mov %ax,%ds
		mov (0x804),%eax
		and $0x00ffffff,%eax
		mov %eax,%es:(EPSON_ID)

		/* Set machine type to PC98_SYSTEM_PARAMETER */
#ifdef SET_MACHINE_TYPE
		call set_machine_type
#else
		mov $M_NEC_PC98+M_NOT_H98,%eax
		mov %eax,%es:(PC98_MACHINE_TYPE)
#endif

		/* Setup graphic screen */
		mov $0x42,%ah		/* 640x400 */
		mov $0xc0,%ch
		int $0x18
		mov $0x40,%ah		/* graph on */
		int $0x18

		/* Setup text screen */
		mov $0x0a00,%ax		/* 80x25 */
		int $0x18
		mov $0x0c,%ah		/* text on */
		int $0x18
		mov $0x13,%ah		/* cursor home */
		xor %dx,%dx
		int $0x18
		mov $0x11,%ah		/* cursor on */
		int $0x18

		/* Setup keyboard */
		mov $0x03,%ah
		int $0x18

		pop %cx

		/* bootstrap passes */
		xor %edi,%edi
		mov %di,%ds
		mov %di,%es
		mov %cs,%bx
		cmp $0x1fe0,%bx
		jz boot_fd
		cmp $0x1fc0,%bx
		jnz boot_hd
		xor %cx,%cx
		mov (DAUA),%al
		and $0xf0,%al
		cmp $0x30,%al
		jz boot_fd
		cmp $0x90,%al
		jnz boot_hd
boot_fd:	xor %cx,%cx
		jmp boot_load
boot_hd:	test %cx,%cx
		jnz boot_load
		mov %cs:(boot_cyl),%cx
boot_load:	mov %cx,MEM_ARG		/* Save cylinder number */
		mov %cx,%di
		xor %dx,%dx
		mov $LOAD_SIZE,%bx
		mov $MEM_BUF,%bp
		push %cs
		callw read
		jc error

		/* Transfer boot2.bin */
		mov $MEM_BTX,%bx
		mov 0xa(%bx),%si	/* BTX size */
		add %bx,%si		/* start of boot2.bin */
		mov $MEM_USR+SIZ_PAG*2,%di
		mov $MEM_BTX+(NSECT-1)*SIZ_SEC,%cx
		sub %si,%cx
		rep
		movsb

		/* Enable A20 */
		xor %ax,%ax
		outb %al,$0xf2
		mov $0x02,%al
		outb %al,$0xf6

		/* Start BTX */
		ljmp $0x0000,$MEM_JMP

/*
 * Reads sectors from the disk.
 * Call with:
 *
 * %bx		- bytes to read
 * %cx		- cylinder
 * %dh		- head
 * %dl		- sector
 * %edi		- lba
 * %es:(%bp)	- buffer to read data into
 */
read:		xor %ax,%ax
		mov %ax,%ds
		mov $0x06,%ah
		mov (DAUA),%al
		mov %ax,%si
		and $0xf0,%al
		cmp $0x30,%al		/* 1.44MB FDD */
		jz read_fd
		cmp $0x90,%al		/* 1MB FDD */
		jz read_fd
		cmp $0xa0,%al		/* Is SCSI device? */
		jnz read_load
		push %cx
		mov %si,%cx
		and $0x0f,%cl
		inc %cl
		mov (0x482),%ah
		shr %cl,%ah		/* Is SCSI HDD? */
		pop %cx
		jc read_load
		and $0xff7f,%si		/* SCSI MO */
		mov %di,%cx
		shr $16,%di
		mov %di,%dx
		jmp read_load
read_fd:	or $0xd000,%si
		or $0x0200,%cx
		inc %dx
read_load:	mov %si,%ax
		int $0x1b
		lret

/*
 * Print out the error message, wait for a keypress, and then reboot
 * the machine.
 */
error:		push %cs
		pop %ds
		mov $msg_eread,%si
		call putstr
		xor %ax,%ax		/* Get keypress */
		int $0x18
		xor %ax,%ax		/* CPU reset */
		outb %al,$0xf0
halt:		hlt
		jmp halt		/* Spin */

/*
 * Display a null-terminated string.
 */
putstr.0:	push %cs
		callw putc
putstr:		lodsb
		test %al,%al
		jne putstr.0
		ret

/*
 * Display a single char.
 */
putc:		pusha
		xor %dx,%dx
		mov %dx,%ds
		mov MEM_REL+cursor-start,%di
		mov $0xa000,%bx
		mov %bx,%es
		mov $(80*2),%cx

		cmp $0x08,%al
		je putc.bs
		cmp $0x0d,%al
		je putc.cr
		cmp $0x0a,%al
		je putc.lf
		cmp $0x5c,%al			/* \ */
		jne 1f
		mov $0xfc,%al
1:		movb $0xe1,%es:0x2000(%di)
		stosw
		jmp putc.scr
putc.bs:	test %di,%di
		jz putc.move
		dec %di
		dec %di
		movb $0xe1,%es:0x2000(%di)
		movw $0x20,%es:(%di)
		jmp putc.move
putc.cr:	mov %di,%ax
		div %cx
		sub %dx,%di
		jmp putc.move
putc.lf:	add %cx,%di
putc.scr:	cmp $(80*2*25),%di		/* Scroll screen */
		jb putc.move
		push %ds
		mov %bx,%ds
		mov $(80*2),%si
		xor %di,%di
		mov $(80*24/2),%cx
		rep
		movsl
		xor %ax,%ax
		mov $0x20,%al
		mov $80,%cl
		rep
		stosw
		pop %ds
		mov $(80*24*2),%di
putc.move:	mov %di,MEM_REL+cursor-start	/* Move cursor */
		mov $0x13,%ah
		mov %di,%dx
		int $0x18
		popa
		lret

cursor:		.word 0

#ifdef SET_MACHINE_TYPE
/*
 * Set machine type to PC98_SYSTEM_PARAMETER.
 */
set_machine_type:
		xor %edx,%edx
		mov %dx,%ds
//		mov $MEM_SYS,%ax
//		mov %ax,%es

		/* Wait V-SYNC */
vsync.1:	inb $0x60,%al
		test $0x20,%al
		jnz vsync.1
vsync.2:	inb $0x60,%al
		test $0x20,%al
		jz vsync.2

		/* ANK 'A' font */
		xor %al,%al
		outb %al,$0xa1
		mov $0x41,%al
		outb %al,$0xa3

		/* Get 'A' font from CG window */
		push %ds
		mov $0xa400,%ax
		mov %ax,%ds
		xor %eax,%eax
		xor %bx,%bx
		mov $4,%cx
font.1:		add (%bx),%eax
		add $4,%bx
		loop font.1
		pop %ds
		cmp $0x6efc58fc,%eax
		jnz m_epson

m_pc98:		or $M_NEC_PC98,%edx
		mov $0x0458,%bx
		mov (%bx),%al
		test $0x80,%al
		jz m_not_h98
		or $M_H98,%edx
		jmp 1f
m_epson:	or $M_EPSON_PC98,%edx
m_not_h98:	or $M_NOT_H98,%edx

1:		inb $0x42,%al
		test $0x20,%al
		jz 1f
		or $M_8M,%edx

1:		mov $0x0400,%bx
		mov (%bx),%al
		test $0x80,%al
		jz 1f
		or $M_NOTE,%edx

1:		mov $PC98_MACHINE_TYPE,%bx
		mov %edx,%es:(%bx)
		ret
#endif

/* Messages */

msg_eread:	.asciz "Error\r\n"

		.org PRT_OFF,0x90

/* Partition table */

		.fill 0x30,0x1,0x0
		.byte 0x80, 0x00, 0x01, 0x00
		.byte 0xa5, 0xff, 0xff, 0xff
		.byte 0x00, 0x00, 0x00, 0x00
		.byte 0x50, 0xc3, 0x00, 0x00

		.word 0xaa55			# Magic number
