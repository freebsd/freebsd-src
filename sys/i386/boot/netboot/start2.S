
#define STACKADDR	0xe000	/* Needs to be end of bss + stacksize */
#define KERN_CODE_SEG	0x08
#define KERN_DATA_SEG	0x10
#define REAL_MODE_SEG	0x18
#define CR0_PE		1

#define opsize		.byte 0x66
#define addrsize	.byte 0x67

/* At entry, the processor is in 16 bit real mode and the code is being
 * executed from an address it was not linked to. Code must be pic and
 * 32 bit sensitive until things are fixed up.
 */
#ifdef BOOTROM
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

/**************************************************************************
CURRTICKS - Get Time
**************************************************************************/
	.globl	_currticks
_currticks:
	push	%ebp
	mov	%esp,%ebp
	push	%ecx
	push	%edx
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
	pop	%edx
	pop	%ecx
	pop	%ebp
	ret

/**************************************************************************
PUTCHAR - Print a character
**************************************************************************/
	.globl	_putchar
_putchar:
	push	%ebp
	mov	%esp,%ebp
	push	%ecx
	push	%ebx
	movb	8(%ebp),%cl
	call	_prot_to_real
	opsize
	mov	$1,%ebx
	movb	$0x0e,%ah
	movb	%cl,%al
	int	$0x10
	opsize
	call	_real_to_prot
	pop	%ebx
	pop	%ecx
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
	call	_prot_to_real
	movb	$0x0,%ah
	int	$0x16
	movb	%al,%bl
	opsize
	call	_real_to_prot
	xor	%eax,%eax
	movb	%bl,%al
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
	pop	%ebx
	pop	%ebp
	ret


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
	ljmp	$REAL_MODE_SEG, $1f	/* jump to a 16 bit segment */
1:
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
	push	%es
	push	%ebx
	push	%esi
	push	%edi
	push	%ecx
	push	%edx

	movb	0x8(%ebp), %dl		/* diskinfo(drive #) */
	call	_prot_to_real		/* enter real mode */

	movb	$0x8, %ah		/* ask for disk info */

	sti
	int	$0x13
	cli

	jnc	ok
	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.
	 */
	subb	%ah, %ah		/* %ax = 0 */
	movb	%al, %al
	movb	%ah, %bh		/* %bh = 0 */
	movb	$2, %bl			/* %bl	bits 0-3 = drive type,
						bit    2 = 1.2M */
	movb	$79, %ch		/* max track */
	movb	$15, %cl		/* max sector */
	movb	$1, %dh			/* max head */
	movb	$1, %dl			/* # floppy drives installed */
	/* es:di = parameter table */
	/* carry = 0 */
ok:

	opsize
	call	_real_to_prot		/* back to protected mode */

	/* 
	 * form a longword representing all this gunk:
	 *       6 bit zero
	 *	10 bit max cylinder (0 based)
	 *	 8 bit max head (0 based)
	 *	 8 bit zero
	 *	 6 bit max sector (1 based) = # sectors
	 */
	movb	%cl, %al		/* Upper two bits of cylinder count */
	andl	$192,%eax	
	leal	0(,%eax,4),%eax		/* << 2 */
	movb	%ch, %al		/* Lower 8 bits */
	sall	$16,%eax		/* << 16 */
	movb	%dh, %ah		/* max head */
	andb	$0x3f, %cl		/* mask of cylinder gunk */
	movb	%cl, %al		/* max sector (and # sectors) */

	pop	%edx
	pop	%ecx
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%es
	pop	%ebp
	ret

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

	/* 16 bit real mode */
	.word	0xffff, 0
	.byte	0, 0x9b, 0x0f, 0

	.align	4
gdtarg:
	.word	0x1f			/* limit */
	.long	gdt			/* addr */
