#define ALIGN_DATA	.align	2	/* 4 byte alignment, zero filled */
#define ALIGN_TEXT	.align	2,0x90	/* 4-byte alignment, nop filled */
#define SUPERALIGN_TEXT	.align	4,0x90	/* 16-byte alignment (better for 486), nop filled */

#define GEN_ENTRY(name)		ALIGN_TEXT;	.globl name; name:
#define NON_GPROF_ENTRY(name)	GEN_ENTRY(_/**/name)

#ifdef GPROF
/*
 * ALTENTRY() must be before a corresponding ENTRY() so that it can jump
 * over the mcounting.
 */
#define ALTENTRY(name)	GEN_ENTRY(_/**/name); MCOUNT; jmp 2f
#define ENTRY(name)	GEN_ENTRY(_/**/name); MCOUNT; 2:
/*
 * The call to mcount supports the usual (bad) conventions.  We allocate
 * some data and pass a pointer to it although the FreeBSD doesn't use
 * the data.  We set up a frame before calling mcount because that is
 * the standard convention although it makes work for both mcount and
 * callers.
 */
#define MCOUNT		.data; ALIGN_DATA; 1:; .long 0; .text; \
			pushl %ebp; movl %esp,%ebp; \
			movl $1b,%eax; call mcount; popl %ebp
#else
/*
 * ALTENTRY() has to align because it is before a corresponding ENTRY().
 * ENTRY() has to align to because there may be no ALTENTRY() before it.
 * If there is a previous ALTENTRY() then the alignment code is empty.
 */
#define ALTENTRY(name)	GEN_ENTRY(_/**/name)
#define ENTRY(name)	GEN_ENTRY(_/**/name)

#endif

#ifdef DUMMY_NOPS			/* this will break some older machines */
#define FASTER_NOP
#define NOP
#else
#define FASTER_NOP	pushl %eax ; inb $0x84,%al ; popl %eax
#define NOP		pushl %eax ; inb $0x84,%al ; inb $0x84,%al ; popl %eax
#endif

