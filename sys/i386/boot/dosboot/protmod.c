/*
 *	protmod.c		Protected Mode Utilities
 *
 *	(C) 1994 by Christian Gusenbauer (cg@fimp01.fim.uni-linz.ac.at)
 *	All Rights Reserved.
 * 
 *	Permission to use, copy, modify and distribute this software and its
 *	documentation is hereby granted, provided that both the copyright
 *	notice and this permission notice appear in all copies of the
 *	software, derivative works or modified versions, and any portions
 *	thereof, and that both notices appear in supporting documentation.
 * 
 *	I ALLOW YOU USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION. I DISCLAIM
 *	ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 *	USE OF THIS SOFTWARE.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <memory.h>
#include <process.h>
#include "boot.h"
#include "bootinfo.h"
#include "protmod.h"

#define data32	_emit 0x66
#define addr32	_emit 0x67

#define SEG(a)		((unsigned int)(((long)(a))>>16l))
#define OFF(a)		((unsigned int)((long)(a)))
#define ptr2lin(a)	((unsigned long)(SEG(a)*0x10l+(long)OFF(a)))

typedef struct {
	unsigned short limit;		/* Segment limit */
	unsigned long addr:24;		/* address */
	unsigned long rights:8;		/* access rights */
	unsigned short reserved;	/* reserved on 80286 */
} DTENTRY;

struct dtr {
	unsigned short limit;
	unsigned long base;
};

struct {
	unsigned long cr3;
	unsigned long GdtrAddress;
	unsigned long IdtrAddress;
	unsigned short LDTR;
	unsigned short TR;
	unsigned long EIP;
	unsigned short CS;
} VCPI;

static DTENTRY gdt[] =
{
	{ 0, 0, 0, 0 },			/* Dummy */
	{ 0, 0, 0, 0 },			/* GDT itself */
	{ 0, 0, 0, 0 },			/* FROM */
	{ 0, 0, 0, 0 },			/* TO */
	{ 0, 0, 0, 0 },			/* BIOS CS */
	{ 0, 0, 0, 0 }			/* SS */
};

static DTENTRY gdt2[] =
{
	{ 0, 0, 0, 0 },			/* Dummy */
	{ 0, 0, 0, 0 },			/* GDT itself */
	{ 0, 0, 0, 0 },			/* IDT */
	{ 0, 0, 0, 0 },			/* DS */
	{ 0, 0, 0, 0 },			/* ES */
	{ 0, 0, 0, 0 },			/* SS */
	{ 0, 0, 0, 0 },			/* CS */
	{ 0, 0, 0, 0 },			/* BIOS CS, uninitialized */
	{ 0, 0, 0, 0 }			/* VCPI: TSS */
};

static DTENTRY FreeBSDGdt[] = {
	{ 0x0000, 0, 0x00, 0x0000 },	/*  0: empty */
	{ 0xffff, 0, 0x9f, 0x00cf },	/*  1: kernel code */
	{ 0xffff, 0, 0x93, 0x00cf },	/*  2: kernel data */
	{ 0xffff, 0, 0x9e, 0x0040 },	/*  3: boot code */
	{ 0xffff, 0, 0x92, 0x0040 },	/*  4: boot data */
	{ 0xffff, 0, 0x9e, 0x0000 },	/*  5: 16bit boot code */
};

static DTENTRY Ldt[] = {
	{ 0x0000, 0, 0x00, 0x0000 },	/* 0: empty */
};

static DTENTRY idt2[256] = { 0 };
static unsigned char Tss[256];

static struct dtr FreeBSDGdtr = { sizeof FreeBSDGdt - 1, 0 };
static struct dtr Gdtr = { sizeof gdt2 - 1, 0 };
static struct dtr Idtr = { sizeof idt2 - 1, 0 };

struct bootinfo bootinfo;
int VCPIboot;

int pm_copy(char far *from, unsigned long to, unsigned long count)
{
	unsigned char status;
	unsigned short cnt = (unsigned short) count;

	if (count == 0l) return -1;	/* count has to be > 0!! */
	gdt[2].limit = cnt-1;		/* so much bytes to receive */
	gdt[2].addr = _FP_SEG(from)*0x10l+_FP_OFF(from);
	gdt[2].rights = 0x92;		/* Data Segment: r/w */

	gdt[3].limit = cnt-1;		/* so much bytes to read */
	gdt[3].addr = to;		/* from HiMem */
	gdt[3].rights = 0x92;		/* Data Segment: r/w */

	cnt >>= 1;

	_asm {
		pusha
		mov ah,87h		; move words
		mov cx,cnt		; that many
		mov bx,seg gdt		; es:si points to the GDT
		mov es,bx
		mov si,offset gdt
		int 15h			; now move the memory block
		mov status,ah		; status is the return value:
					;	0 .. no error,
					;	1 .. parity error,
					;	2 .. exception interrupt
					;	3 .. gate A20 failed
		popa
	}

    return (int) status;
}

static int pm_enter(void)
{
	unsigned char status;
	unsigned int segment;

	/* setup GDT entry 1: GDT */
	gdt2[1].limit = sizeof(gdt2)-1;
	gdt2[1].addr = ptr2lin(gdt2);
	gdt2[1].rights = 0x92;		/* Data Segment: r/w */

	/* setup GDT entry 2: IDT */
	gdt2[2].limit = sizeof(idt2)-1;
	gdt2[2].addr = ptr2lin(idt2);
	gdt2[2].rights = 0x92;		/* Data Segment: r/w */

	/* setup GDT entry 3: DS */
	_asm mov segment,ds
	gdt2[3].limit = 0xffff;		/* max. offset */
	gdt2[3].addr = segment*0x10l;	/* segment starts at */
	gdt2[3].rights = 0x92;		/* Data Segment: r/w */

	/* setup GDT entry 4: ES */
	_asm mov segment,es
	gdt2[4].limit = 0xffff;		/* max. offset */
	gdt2[4].addr = segment*0x10l;	/* segment starts at */
	gdt2[4].rights = 0x92;		/* Data Segment: r/w */

	/* setup GDT entry 5: SS */
	_asm mov segment,ss
	gdt2[5].limit = 0;		/* max. offset = 64 K!! */
	gdt2[5].addr = segment*0x10l;	/* segment starts at */
	gdt2[5].rights = 0x96;		/* Stack Segment: r/w, expansion direction=down */

	/* setup GDT entry 7: uninitialized! */

	/* setup GDT entry 6: CS */
	_asm mov segment,cs
	gdt2[6].limit = 0xffff;		/* max. offset */
	gdt2[6].addr = segment*0x10l;	/* segment starts at */
	gdt2[6].rights = 0x9a;		/* Code Segment: execute only */

	_asm {
		pusha
		mov ah,89h		; enter protected mode
		mov bx,seg gdt2		; es:si points to the GDT
		mov es,bx
		mov si,offset gdt2
		mov bx,2820h		; setup Interrupt Levels
		int 15h			; now move the memory block
		mov status,ah		; status is the return value and 0 if no error occurred
		popa
	}

	if (status) return (int) status;/* no protected mode; return status */

	_asm {
		mov ax,30h
		mov word ptr ss:[bp+4],ax	; patch code selector
	}
	return 0;
}

static void setupVCPI(void)
{
	unsigned int segment;

	/* setup GDT entry 1: VCPI 1 (code) */
	gdt2[1].limit = 0;		/* max. offset */
	gdt2[1].addr = 0;		/* segment starts at */
	gdt2[1].rights = 0;		/* Data Segment: r/w */

	/* setup GDT entry 2: VCPI 2 */
	gdt2[2].limit = 0;		/* max. offset */
	gdt2[2].addr = 0;		/* segment starts at */
	gdt2[2].rights = 0;		/* Data Segment: r/w */

	/* setup GDT entry 3: VCPI 3 */
	gdt2[3].limit = 0;		/* max. offset */
	gdt2[3].addr = 0;		/* segment starts at */
	gdt2[3].rights = 0;		/* Data Segment: r/w */

	/* setup GDT entry 4: code segment (use16) */
	_asm mov segment,cs
	gdt2[4].limit = 0xffff;		/* max. offset */
	gdt2[4].addr = segment*0x10l;	/* segment starts at */
	gdt2[4].rights = 0x9a;		/* Code Segment */

	/* setup GDT entry 5: data segment (use16) */
	_asm mov segment,ds
	gdt2[5].limit = 0xffff;		/* max. offset */
	gdt2[5].addr = segment*0x10l;	/* segment starts at */
	gdt2[5].rights = 0x92;		/* Data Segment: r/w */

	/* setup GDT entry 6: stack segment */
	_asm mov segment,ss
	gdt2[6].limit = 0;		/* max. offset */
	gdt2[6].addr = segment*0x10l;	/* segment starts at */
	gdt2[6].rights = 0x96;		/* Stack Segment: r/w */

	/* setup GDT entry 7: LDT selector */
	gdt2[7].limit = 7;		/* max. offset */
	gdt2[7].addr = ptr2lin(Ldt);	/* segment starts at */
	gdt2[7].rights = 0x82;		/* Data Segment: r/w */

	/* setup GDT entry 8: 286-TSS */
	gdt2[8].limit = 43;		/* max. offset */
	gdt2[8].addr = ptr2lin(Tss);	/* segment starts at */
	gdt2[8].rights = 0x81;		/* TSS */
}

long get_high_memory(long size)
{
	int kb = ((int) (size/1024l)+3)&0xfffc;	/* we need this much KB */
	int lo, hi, vcpiVer, vcpiStatus;
	int (far *xms_entry)();
	FILE *fp;

	/*
	 * Let's check for VCPI services.
	 */

	fp = fopen("EMMXXXX0", "rb");
	if (fp) {
		fclose(fp);
		_asm {
			pusha
			mov ax,0de00h
			int 67h
			mov vcpiVer,bx
			mov vcpiStatus,ax
			popa
		}
		if (!(vcpiStatus&0xff00)) {
			VCPIboot = 1;
			printf("VCPI services Version %d.%d detected!\n", vcpiVer>>8, vcpiVer&0xff);
		}
	}

	/*
	 * I don't know why, but 386max seems to use the first 64 KB of that
	 * XMS area?! So I allocate more ram than I need!
	 */
	kb += 128;

	_asm {
		pusha
		mov ax,4300h
		int 2fh			; let's look if we have XMS
		cmp al,80h
		je wehaveit		; ok, we have it
		popa
	}
	return 0x110000l;		/* default load address */

no:	_asm popa
	return 0l;

	_asm {
wehaveit:	mov ax,4310h
		int 2fh			; get xms entry point
		mov word ptr [xms_entry],bx
		mov word ptr [xms_entry+2],es

		mov ah,8h
		call [xms_entry]

		cmp ax,kb
		jb no

		mov dx,kb
		mov ah,9h
		call [xms_entry]	; get memory
		cmp ax,0
		je no			; sorry, no memory

		mov ah,0ch
		call [xms_entry]	; lock memory block (dx = handle)
		cmp ax,0
		je no
		mov lo,bx
		mov hi,dx
		popa
	}
	return (long)hi*0x10000l+(long)lo + 128l*1024l;
}

void startprog(long hmaddress, long hmsize, long startaddr, long loadflags,
			   long bootdev)
{
	long GDTaddr=ptr2lin(FreeBSDGdt);
	long *stack=_MK_FP(0x9f00, 0);	/* prepare stack for starting the kernel */
	unsigned int pmseg, pmoff;
	unsigned int segment, pcxoff, psioff, pdioff;
	long h, BOOTaddr, ourret;
	unsigned char *page;
	int status;

	/*
	 * The MSVC 1.5 inline assembler is not able to work with
	 * 386 opcodes (ie. extended registers like eax). So we have
	 * to use a workaround (god save Micro$oft and their customers ;)
	 */

	_asm {
		mov segment,cs
		mov ax, offset our_return
		mov pmoff,ax
	}
	BOOTaddr = segment*0x10l;
	ourret = BOOTaddr + (long) pmoff;

	_asm {
		push ds

		mov ax,cs
		mov ds,ax
		mov bx,offset lab		; patch the far jump after
		mov byte ptr ds:[patch],bl	; switching gdt for FreeBSD
		mov byte ptr ds:[patch+1],bh

		mov bx,offset pcx
		mov pcxoff,bx
		mov bx,offset psi
		mov psioff,bx
		mov bx,offset pdi
		mov pdioff,bx
		mov segment,ds

		pop ds
	}

	*((long *)_MK_FP(segment, pcxoff+1)) = hmsize;
	*((long *)_MK_FP(segment, psioff+1)) = hmaddress;
	*((long *)_MK_FP(segment, pdioff+1)) = startaddr;

	h = ptr2lin(&VCPI);

	_asm {
		push ds
		mov ax,cs
		mov ds,ax

		mov bx,word ptr ss:[h]
		mov cx,word ptr ss:[h+2]

		mov byte ptr ds:[patch2+1],bl
		mov byte ptr ds:[patch2+2],bh
		mov byte ptr ds:[patch2+3],cl
		mov byte ptr ds:[patch2+4],ch

		pop ds
	}

	/*
	 * Setup the stack for executing the kernel. These parameters are
	 * put on the stack in reversed order (addresses are INCREMENTED)!
	 */

	*stack++ = startaddr;		/* that's the startaddress */
	*stack++ = 8l;			/* new CS */
	*stack++ = ourret;		/* ourreturn */
	*stack++ = loadflags;		/* howto */
	*stack++ = bootdev;		/* bootdev */
	*stack++ = 0l;			/* Parameter 4 */
	*stack++ = 0l;			/* Parameter 5 */
	*stack++ = 0l;			/* Parameter 6 */
	*stack++ = ptr2lin(&bootinfo);	/* bootinfo */

	/*
	 * Initialize FreeBSD GDT and GDTR
	 */

	FreeBSDGdtr.base = GDTaddr;

	FreeBSDGdt[3].addr = BOOTaddr;

	/*
	 * Now, we have to start the kernel at the given startaddress. To do this, we must
	 * switch to protected mode using INT15 with AH=0x89. This call uses its own layout
	 * of the GDT, so we switch to our own GDT after we return from the INT15 call. But
	 * before we do this, we must copy the 64 K which overwrites the HIMEM at 0x100000.
	 */

	if (!VCPIboot) {
		if (!(status=pm_enter())) {
			_asm {
				cli
				mov ax,18h
				mov ds,ax
			}
			goto nowgo;
		}
		fprintf(stderr, "Can't switch to protected mode!\n");
		fprintf(stderr, "Giving up :-(!\n");
		exit(0);
	}

	/*
	 * OK. Let's use VCPI services.
	 */

	Gdtr.base = ptr2lin(gdt2);
	Idtr.base = ptr2lin(idt2);
	setupVCPI();

	page = malloc(8192);		/* allocate 8 KB */
	if (!page) {
		fprintf(stderr, "not enough memory!\n");
		exit(0);
	}
	memset(page, 0, 8192);

	h = (ptr2lin(page)+4095l) & 0xfffff000l;
	pmseg = (unsigned short) (h>>4l);

	/*
	 * We *do* have VCPI services, so let's get the protected mode
	 * interface and page table 0 from the server.
	 */

	_asm {
		push ds
		push si
		push di
		mov ax,seg gdt2
		mov ds,ax
		mov ax,offset gdt2
		add ax,8
		mov si,ax
		mov ax,pmseg
		mov es,ax
		xor di,di
		mov ax,0xde01
		int 0x67
		pop di
		pop si
		pop ds
	}

	/*
	 * setup values for the mode change call
	 */

	*((unsigned long *) MK_FP(pmseg,0x1000)) = h+3l;

	VCPI.cr3 = h+0x1000l;		/* page dir is the next page */
	VCPI.GdtrAddress = ptr2lin(&Gdtr);
	VCPI.IdtrAddress = ptr2lin(&Idtr);
	VCPI.LDTR = 7*8;
	VCPI.TR = 8*8;

	_asm {
		mov ax,offset nowgoVCPI
		mov pmoff,ax
	}

	VCPI.EIP = (long) pmoff;
	VCPI.CS = 4*8;

	_asm {
		cli
		data32
patch2:		mov si,0
		_emit 0
		_emit 0
		mov ax,0de0ch
		int 67h

nowgoVCPI:	; we are now executing in protected mode
		; first, we turn paging off!

		data32
		_emit 0fh	; this is "mov eax,CR0"
		_emit 20h	;
		_emit 0c0h	;

		data32
		and ax,0ffffh
		_emit 0ffh
		_emit 7fh

		data32
		_emit 0fh	; this is "mov CR0,eax"
		_emit 22h	; and turns paging off
		_emit 0c0h	;

		data32
		xor ax,ax

		data32
		_emit 0fh	; this is "mov CR3,eax"
		_emit 22h	; and clears the page cache
		_emit 0d8h	;

		mov ax,28h
		mov ds,ax	; load new DS
		mov es,ax
		mov ax,6*8
		mov ss,ax
	}

/*******************************************************************************
 * now this is all executed in protected mode!!!
 */

	/* setup new gdt for the FreeBSD kernel */
	_asm {
nowgo:		cli
		lgdt FreeBSDGdtr

		data32
		_emit 0eah	; far jump to "lab" (switch cs)
patch:		_emit 0		; these two bytes are patched with the
		_emit 0		; correct offset of "lab"
		_emit 0
		_emit 0
		_emit 18h
		_emit 0

	; Setup SS, DS and ES registers with correct values, initialize the
	; stackpointer to the correct value and execute kernel

lab:		mov bx,10h
		_emit 0
		_emit 0
		mov ds,bx
		mov es,bx
		mov ss,bx

	; move kernel to its correct address

pcx:		_emit 0b9h	; Micro$oft knows, why "mov cx,0" does not
		_emit 0		; work here
		_emit 0
		_emit 0
		_emit 0
psi:		_emit 0beh	; mov si,0
		_emit 0
		_emit 0
		_emit 0
		_emit 0
pdi:		_emit 0bfh	; mov di,0
		_emit 0
		_emit 0
		_emit 0x10
		_emit 0

		rep movsb

	; MSVC is unable to assemble this instruction: mov esp,09f000h

		mov sp,0f000h
		_emit 9h
		_emit 0
		retf						; execute kernel
our_return:	jmp our_return
	}
	/* not reached */
}
