
#define STACKADDR	0xe000	/* Needs to be end of bss + stacksize */
#define KERN_CODE_SEG	0x08
#define KERN_DATA_SEG	0x10
#define REAL_MODE_CSEG	0x18
#define REAL_MODE_DSEG  0x20
#define CR0_PE		1

#define opsize		.byte 0x66
#define addrsize	.byte 0x67

/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 */
#ifdef BOOTROM
#ifndef PC98
	.word	0xaa55			/* bios extension signature */
	.byte	(ROMSIZE>>9)		/* no. of 512B blocks */
	jmp	1f			/* enter from bios here */
	.byte	0			/* checksum */
#ifdef	PCI
	.ascii   "FreeBSD boot ROM.."		/* 18 bytes total */
	.word	0x1a
/* PCI rom data structure format */
	.ascii	"PCIR"			/* signature */
	.word	PCI_VENDOR		/* vendor ID */
	.word	PCI_DEVICE		/* device ID */
	.word	0			/* vital product data */
	.word	0x0018			/* PCI data structure */
	.byte	0			/* PCI data struct. rev -- 0 */
	.byte	PCI_CLASS		/* Class code */
	.word	(ROMSIZE>>9)            /* no. of 512B blocks */
	.byte	0,0			/* rev. level */
	.byte	0			/* code type - 0 =x86 */
	.byte	0x80			/* indicator of last block */
	.word	0			/* reserved */
#endif
1:	push	%eax
	push	%ds
	xor	%eax,%eax
	mov	%ax,%ds
	.byte	0xa1			/* MOV 0x304,%ax */
	.word	0x304
	.byte	0x3d			/* CMP $0x4d52, %ax  == 'MR' */
	.word	0x4d52
	jz	2f
	.byte	0xa1			/* MOV 0x64, %ax */
	.word	0x64
	.byte	0xa3			/* MOV %ax, 0x300 */
	.word	0x300
	.byte	0xa1			/* MOV 0x66, %ax */
	.word	0x66
	.byte	0xa3			/* MOV %ax, 0x302 */
	.word	0x302
	.byte	0xb8			/* MOV $_start-RELOCADDR, %ax */
	.word	(_start-RELOC)
	.byte	0xa3			/* MOV %ax, 0x64 */
	.word	0x64
	mov	%cs,%ax
	.byte	0xa3			/* MOV %ax, 0x66 */
	.word	0x66
	.byte	0xb8			/* MOV 'MR',%ax */
	.word	0x4d52
	.byte	0xa3			/* MOV %ax, 0x304 */
	.word	0x304
2:	pop	%ds
	pop	%eax
	lret
#else	/*  defined PC98  */
	.byte	0xcb			/*  00 retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  03 retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  06 retf  */
	.byte	0x00
	.byte	0x00
	.byte	0x55			/*  09 MAGIC  */
	.byte	0xaa
	.byte	0x00
	.byte	0xeb			/*  0c jmp 1f */
	.byte	0x22
	.byte	0x00
	.byte	0xcb			/*  0f retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  12 retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  15 retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  18 retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  1b retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  1e retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  21 retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  24 retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  27 retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  2a retf  */
	.byte	0x00
	.byte	0x00
	.byte	0xcb			/*  2d retf  */
	.byte	0x00
	.byte	0x00
1:
	cli
	pusha
	opsize
	mov	$0x1fc0, %eax
	mov	%ax, %es
	mov	%esp, %eax
	addrsize
	.byte	0x26			/*  es:  */
	mov	%eax, (0x0000)
	mov	%ss, %ax
	addrsize
	.byte	0x26			/*  es:  */
	mov	%eax, (0x0004)
#endif
#endif

/**************************************************************************
START - Where all the fun begins....
**************************************************************************/
	.globl	_start
_start:
	cli
	cld
#ifdef BOOTROM				/* relocate ourselves */
        xor     %esi, %esi		/* zero for ROMs */
#else
	.byte	0xbe			/* MOV $0x100,%si -- 100h for .COM */
	.word	0x100
#endif
	xor	%edi,%edi
	.byte	0xb8			/* MOV $RELOCADDR>>4, %ax */
	.word	(RELOC>>4)
        mov     %ax, %es
	.byte	0xb9			/* MOV $ROMSIZE, %cx */
	.word   ROMSIZE
        cs
        rep
        movsb
	opsize
	ljmp	$(RELOC>>4),$1f-RELOC		/* Jmp to RELOC:1f */
1:
	nop
#ifdef PC98
	opsize
	mov	$0x0a00, %eax		/* 80 x 25 mode  */
	int	$0x18
	movb	$0x0c, %ah		/* text on */
	int	$0x18
	movb	$0x16, %ah		/* t-vram clear */
	opsize
	mov	$0xe100, %edx
	int	$0x18

        /* cursor home and on */
	xor	%edx, %edx
	movb	$0x13, %ah
	int	$0x18
	movb	$0x11, %ah
	int     $0x18

	/* set up %ds */
	xor	%ax, %ax
	mov	%ax, %ds

	opsize
	mov	$0xa000, %eax
	mov	%ax, %es

	/* transfer PC-9801 system common area to 0xa1000 */
	opsize
	mov	$0x0000, %esi
	opsize
	mov	$0x1000, %edi
	opsize
	mov	$0x0630, %ecx
	cld
	rep
	movsb

	/* transfer EPSON machine type to 0xa1200 */
	push	%ds
	opsize
	mov	$0xfd00, %eax
	mov	%ax, %ds
	addrsize
	opsize
	mov	0x804, %eax
	opsize
	and	$0x00ffffff, %eax
	addrsize
	opsize
	.byte	0x26
	mov	%eax, %es: (0x1624)
	pop	%ds
#endif
	mov	%cs,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%ss
	.byte	0xb8			/* MOV $STACKADDR, %ax */
	.word	STACKADDR
	mov	%eax,%esp
	opsize
	call	_real_to_prot
	call	_main
	.globl	_exit
_exit:
	call	_prot_to_real
#ifndef PC98
#ifdef BOOTROM
	xor	%eax,%eax
	mov	%ax,%ds
	.byte	0xa1			/* MOV 0x302, %ax */
	.word	0x302
	push	%eax
	.byte	0xa1			/* MOV 0x300, %ax */
	.word	0x300
	push	%eax
	lret
#else
	int	$0x19
#endif
#else	/*  defined PC98  */
#ifdef BOOTROM
	cli
	movb	$0x0e, %al		/*  CPU RESET  */
	outb	%al, $0x37
	xor	%eax, %eax
	mov	%ax, %es
	push	%cs
	push	$2f
	addrsize
	.byte	0x26			/*  es:  */
	mov	%esp, (0x0404)
	addrsize
	.byte	0x26			/*  es:  */
	movw	%ss, (0x0406)
	movb	$0x00, %al
	outb	%al, $0xf0
1:
	jmp	1b
2:
	movb	$0x0f, %al
	outb	%al, $0x37

	opsize				/*  restore ss:sp  */
	mov	$0x1fc0, %eax
	mov	%ax, %es
	addrsize
	.byte	0x26			/*  es:  */
	mov	(0x0000), %ebx
	addrsize
	.byte	0x26			/*  es:  */
	mov	(0x0004), %eax
	mov	%ax, %ss
	mov	%ebx, %esp
	sti
	popa
	lret
#else
	mov	$0x8000, %ecx
	movb	$0x00, %al
1:
	jmp	1b
	outb	%al, $0x5f
	loop	1b
	outb	%al, $0xf0
1:
	jmp	1b
#endif
#endif

#ifndef PC98
/**************************************************************************
CURRTICKS - Get Time
**************************************************************************/
	.globl	_currticks
_currticks:
	push	%ebp
	mov	%esp,%ebp
	push    %ebx
	push	%esi
	push	%edi
	xor	%edx,%edx
	call	_prot_to_real
	xor	%eax,%eax
	int	$0x1a
	opsize
	call	_real_to_prot
	xor	%eax,%eax
	shl	$16,%ecx
	mov	%edx,%eax
	or	%ecx,%eax
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret

/**************************************************************************
PUTCHAR - Print a character
**************************************************************************/
	.globl	_putchar
_putchar:
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%esi
	push	%edi
	movb	8(%ebp),%cl
	call	_prot_to_real
	opsize
	mov	$1,%ebx
	movb	$0x0e,%ah
	movb	%cl,%al
	int	$0x10
	opsize
	call	_real_to_prot
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret

/**************************************************************************
GETCHAR - Get a character
**************************************************************************/
	.globl	_getchar
_getchar:
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%esi
	push	%edi
	call	_prot_to_real
	movb	$0x0,%ah
	int	$0x16
	movb	%al,%bl
	opsize
	call	_real_to_prot
	xor	%eax,%eax
	movb	%bl,%al
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret

/**************************************************************************
ISKEY - Check for keyboard interrupt
**************************************************************************/
	.globl	_iskey
_iskey:
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%esi
	push	%edi
	call	_prot_to_real
	xor	%ebx,%ebx
	movb	$0x1,%ah
	int	$0x16
	opsize
	jz	1f
	movb	%al,%bl
1:
	opsize
	call	_real_to_prot
	xor	%eax,%eax
	movb	%bl,%al
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
#else	/*  defined PC98  */
/**************************************************************************
Get Date & Time
**************************************************************************/
	.globl	_bios98getdate
_bios98getdate:
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%es

	call	_prot_to_real

	opsize
	mov	$0x1ff0, %eax
	mov	%ax, %es
	movb	$0x00, %ah
	opsize
	mov	$0x80, %ebx	/*  bios uses 1ff0:0080  */
	int	$0x1c

	opsize
	mov	$0x82, %ebx
	opsize
	.byte	0x26		/*  es:  */
	.byte	0x8b		/*  mov	(%ebx), %ebx  */
	.byte	0x1f

	opsize
	call	_real_to_prot

	mov	%ebx, %eax
	pop	%es
	pop	%ebx
	pop	%ebp
	ret

/*
 * getc()
 * BIOS call "INT 18H Function 00H" to read character from keyboard
 *	Call with	%ah = 0x0
 *	Return:		%ah = keyboard scan code
 *			%al = ASCII character
 */

	.globl	_getchar
_getchar:
	push	%ebp
	mov	%esp, %ebp
	push	%ebx			/* save %ebx */

	call	_prot_to_real

	movb	$0x0, %ah
	int	$0x18

	movb	%al, %bl		/* real_to_prot uses %eax */

	opsize
	call	_real_to_prot

	xor	%eax, %eax
	movb	%bl, %al

	pop	%ebx
	pop	%ebp
	ret
/*
 * ischar()
 *	if there is a character pending, return it; otherwise return 0
 * BIOS call "INT 18H Function 01H" to check whether a character is pending
 *	Call with	%ah = 0x1
 *	Return:
 *		If key waiting to be input:
 *			%ah = keyboard scan code
 *			%al = ASCII character
 *			%bh = 1
 *		else
 *			%bh = 0
 */
	.globl	_iskey
_iskey:
	push	%ebp
	mov	%esp, %ebp
	push	%ebx

	call	_prot_to_real		/* enter real mode */

	xor	%ebx, %ebx
	movb	$0x1, %ah
	int	$0x18
	andb	%bh, %bh
	opsize
	jz	nochar
	movb	%al, %bl

nochar:
	opsize
	call	_real_to_prot

	xor	%eax, %eax
	movb	%bl, %al

	pop	%ebx
	pop	%ebp
	ret

#endif


/*
 * C library -- _setjmp, _longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from the last call to
 *	setjmp(a)
 * by restoring registers from the stack.
 * The previous signal state is restored.
 */

	.globl	_setjmp
_setjmp:
	movl	4(%esp),%ecx 
	movl	0(%esp),%edx
	movl	%edx, 0(%ecx)
	movl	%ebx, 4(%ecx)
	movl	%esp, 8(%ecx)
	movl	%ebp,12(%ecx)
	movl	%esi,16(%ecx)
	movl	%edi,20(%ecx)
	movl	%eax,24(%ecx)
	movl	$0,%eax
	ret

	.globl	_longjmp
_longjmp:
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	movl	0(%edx),%ecx
	movl	4(%edx),%ebx
	movl	8(%edx),%esp
	movl	12(%edx),%ebp
	movl	16(%edx),%esi
	movl	20(%edx),%edi
	cmpl	$0,%eax
	jne	1f
	movl	$1,%eax
1:	movl	%ecx,0(%esp)
	ret

/**************************************************************************
___MAIN - Dummy to keep GCC happy
**************************************************************************/
	.globl	___main
___main:
	ret

/**************************************************************************
REAL_TO_PROT - Go from REAL mode to Protected Mode
**************************************************************************/
	.globl	_real_to_prot
_real_to_prot:
	cli
	cs
	addrsize
	lgdt	gdtarg-RELOC
	mov	%cr0, %eax
	opsize
	or	$CR0_PE, %eax
	mov	%eax, %cr0		/* turn on protected mode */

	/* jump to relocation, flush prefetch queue, and reload %cs */
	opsize
	ljmp	$KERN_CODE_SEG, $1f
1:
	/* reload other segment registers */
	movl	$KERN_DATA_SEG, %eax
	movl	%ax, %ds
	movl	%ax, %es
	movl	%ax, %ss
	add	$RELOC,%esp		/* Fix up stack pointer */
	pop	%eax			/* Fix up return Address */
	add	$RELOC,%eax
	push	%eax
	ret


/**************************************************************************
PROT_TO_REAL - Go from Protected Mode to REAL Mode
**************************************************************************/
	.globl	_prot_to_real
_prot_to_real:
	pop	%eax
	sub	$RELOC,%eax		/* Adjust return address */
	push	%eax
	sub	$RELOC,%esp		/* Adjust stack pointer */
		
	/* Prepare %ax while we're still in a mode that gas understands. */
	movw	$REAL_MODE_DSEG, %ax
	
	ljmp	$REAL_MODE_CSEG, $1f-RELOC /* jump to a 16 bit segment */
1:
	mov	%ax, %ds
	mov	%ax, %ss 
	mov	%ax, %es
	mov	%ax, %fs
	
	/* clear the PE bit of CR0 */
	mov	%cr0, %eax
	opsize
	andl 	$0!CR0_PE, %eax
	mov	%eax, %cr0

	/* make intersegment jmp to flush the processor pipeline
	 * and reload CS register
	 */
	opsize
	ljmp	$(RELOC)>>4, $2f-RELOC
2:
	/* we are in real mode now
	 * set up the real mode segment registers : DS, SS, ES
	 */
	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss
	sti
	opsize
	ret

/**************************************************************************
GET DISK GEOMETRY INFO
**************************************************************************/
#ifdef	PC98
/*
 *
 * get_diskinfo():  return a word that represents the
 *	max number of sectors and heads and drives for this device
 *
 */
	.globl	_get_diskinfo
_get_diskinfo:
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push	%esi
	push    %edi

	movb	0x8(%ebp), %dl		/* diskinfo(drive #) */
	call	_prot_to_real		/* enter real mode */

	movb	%dl, %al		/* ask for disk info */
	andb	$0xf0, %al
	cmpb	$0x90, %al
	jz	fdd

	movb	%dl, %al
	movb	$0x84, %ah

	int	$0x1b

	jnc	ok
	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.
	 */
fdd:
	subb	%ah, %ah		/* %ax = 0 */
	movb	%al, %al
	movb	%ah, %bh		/* %bh = 0 */
	movb	$2, %bl			/* %bl	bits 0-3 = drive type,
						bit    2 = 1.2M */
	movb	$79, %ch		/* max track */
	movb	$1, %cl			/* # floppy drives installed */
	movb	$2, %dh			/* max head */
	movb	$15, %dl		/* max sector */
	/* es:di = parameter table */
	/* carry = 0 */
ok:

	opsize
	call	_real_to_prot		/* back to protected mode */

	/* 
	 * form a longword representing all this gunk:
	 *	16 bit cylinder
	 *	 8 bit head
	 *	 8 bit sector
	 */
	mov	%ecx, %eax
	sall	$16,%eax		/* << 16 */
	movb	%dh, %ah		/* max head */
	movb	%dl, %al		/* max sector (and # sectors) */

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
#endif
/**************************************************************************
GLOBAL DESCRIPTOR TABLE
**************************************************************************/
	.align	4
gdt:
	.word	0, 0
	.byte	0, 0x00, 0x00, 0

	/* code segment */
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0

	/* data segment */
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0

	/* 16 bit real mode code segment */
	.word	0xffff, RELOC & 0xffff
	.byte	(RELOC>>16), 0x9b, 0x00, (RELOC>>24)

	/* 16 bit real mode data segment */
	.word	0xffff, RELOC & 0xffff
	.byte	(RELOC>>16), 0x93, 0x00, (RELOC>>24)

	.align	4
gdtarg:
	.word	0x27			/* limit */
	.long	gdt			/* addr */
