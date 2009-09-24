/*-
 * Written by paradox <ddkprog@yahoo.com>
 * Public domain.
 *
 * x86 registers were borrowed from x86emu.h x86emu_regs.h
 * for compatability.
 *
 * $FreeBSD$
 */

#ifndef _X86BIOS_H_
#define _X86BIOS_H_

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/systm.h>

#ifdef	__BIG_ENDIAN__

struct x86_register32 {
	uint32_t e_reg;
};

struct x86_register16 {
	uint16_t filler0;
	uint16_t x_reg;
};

struct x86_register8 {
	uint8_t filler0, filler1;
	uint8_t h_reg, l_reg;
};

#else /* !__BIG_ENDIAN__ */

struct x86_register32 {
	uint32_t e_reg;
};

struct x86_register16 {
	uint16_t x_reg;
};

struct x86_register8 {
	uint8_t l_reg, h_reg;
};

#endif /* __BIG_ENDIAN__ */

union x86_register {
	struct x86_register32	I32_reg;
	struct x86_register16	I16_reg;
	struct x86_register8	I8_reg;
};

struct x86regs {
	uint16_t		register_cs;
	uint16_t		register_ds;
	uint16_t		register_es;
	uint16_t		register_fs;
	uint16_t		register_gs;
	uint16_t		register_ss;
	uint32_t		register_flags;
	union x86_register	register_a;
	union x86_register	register_b;
	union x86_register	register_c;
	union x86_register	register_d;

	union x86_register	register_sp;
	union x86_register	register_bp;
	union x86_register	register_si;
	union x86_register	register_di;
	union x86_register	register_ip;
};

typedef struct x86regs	x86regs_t;

/* 8 bit registers */
#define R_AH	register_a.I8_reg.h_reg
#define R_AL	register_a.I8_reg.l_reg
#define R_BH	register_b.I8_reg.h_reg
#define R_BL	register_b.I8_reg.l_reg
#define R_CH	register_c.I8_reg.h_reg
#define R_CL	register_c.I8_reg.l_reg
#define R_DH	register_d.I8_reg.h_reg
#define R_DL	register_d.I8_reg.l_reg

/* 16 bit registers */
#define R_AX	register_a.I16_reg.x_reg
#define R_BX	register_b.I16_reg.x_reg
#define R_CX	register_c.I16_reg.x_reg
#define R_DX	register_d.I16_reg.x_reg

/* 32 bit extended registers */
#define R_EAX	register_a.I32_reg.e_reg
#define R_EBX	register_b.I32_reg.e_reg
#define R_ECX	register_c.I32_reg.e_reg
#define R_EDX	register_d.I32_reg.e_reg

/* special registers */
#define R_SP	register_sp.I16_reg.x_reg
#define R_BP	register_bp.I16_reg.x_reg
#define R_SI	register_si.I16_reg.x_reg
#define R_DI	register_di.I16_reg.x_reg
#define R_IP	register_ip.I16_reg.x_reg
#define R_FLG	register_flags

/* special registers */
#define R_ESP	register_sp.I32_reg.e_reg
#define R_EBP	register_bp.I32_reg.e_reg
#define R_ESI	register_si.I32_reg.e_reg
#define R_EDI	register_di.I32_reg.e_reg
#define R_EIP	register_ip.I32_reg.e_reg
#define R_EFLG	register_flags

/* segment registers */
#define R_CS	register_cs
#define R_DS	register_ds
#define R_SS	register_ss
#define R_ES	register_es
#define R_FS	register_fs
#define R_GS	register_gs

#define SEG_ADDR(x)	(((x) >> 4) & 0x00F000)
#define SEG_OFF(x)	((x) & 0x0FFFF)
#define FARP(x)		((le32toh(x) & 0xffff) + ((le32toh(x) >> 12) & 0xffff00))

#define MAPPED_MEMORY_SIZE	0xc00000
#define PAGE_RESERV		(4096*5)

__BEGIN_DECLS
void *x86bios_alloc(int count, int *segs);
void  x86bios_free(void *pbuf, int count);
void  x86bios_intr(struct x86regs *regs, int intno);
void *x86bios_offset(uint32_t offs);
__END_DECLS

#endif /* !_X86BIOS_H_ */
