/*
 * Copyright (c) 2001 The FreeBSD Project, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY The FreeBSD Project, Inc. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL The FreeBSD Project, Inc. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "doscmd.h"
#include "video.h"

static u_int32_t	decode_modrm(u_int8_t *, u_int16_t,
				     regcontext_t *, int *);
static u_int8_t		*reg8(u_int8_t c, regcontext_t *);
static u_int16_t	*reg16(u_int8_t c, regcontext_t *);
#if 0
static u_int32_t	*reg32(u_int8_t c, regcontext_t *);
#endif
static u_int8_t		read_byte(u_int32_t);
static void		write_byte(u_int32_t, u_int8_t);
static void		write_word(u_int32_t, u_int16_t);

/*
** Hardware /0 interrupt
*/
void
int00(regcontext_t *REGS __unused)
{
    debug(D_ALWAYS, "Divide by 0 in DOS program!\n");
    exit(1);
}

void
int01(regcontext_t *REGS __unused)
{
    debug(D_ALWAYS, "INT 1 with no handler! (single-step/debug)\n");
}

void
int03(regcontext_t *REGS __unused)
{
    debug(D_ALWAYS, "INT 3 with no handler! (breakpoint)\n");
}

void
int0d(regcontext_t *REGS __unused)
{
    debug(D_ALWAYS, "IRQ5 with no handler!\n");
}

void
cpu_init(void)
{
    u_long vec;

    vec = insert_hardint_trampoline();
    ivec[0x00] = vec;
    register_callback(vec, int00, "int 00");

    vec = insert_softint_trampoline();
    ivec[0x01] = vec;
    register_callback(vec, int01, "int 01");

    vec = insert_softint_trampoline();
    ivec[0x03] = vec;
    register_callback(vec, int03, "int 03");

    vec = insert_hardint_trampoline();
    ivec[0x0d] = vec;
    register_callback(vec, int0d, "int 0d");

    vec = insert_null_trampoline();
    ivec[0x34] = vec;	/* floating point emulator */
    ivec[0x35] = vec;	/* floating point emulator */
    ivec[0x36] = vec;	/* floating point emulator */
    ivec[0x37] = vec;	/* floating point emulator */
    ivec[0x38] = vec;	/* floating point emulator */
    ivec[0x39] = vec;	/* floating point emulator */
    ivec[0x3a] = vec;	/* floating point emulator */
    ivec[0x3b] = vec;	/* floating point emulator */
    ivec[0x3c] = vec;	/* floating point emulator */
    ivec[0x3d] = vec;	/* floating point emulator */
    ivec[0x3e] = vec;	/* floating point emulator */
    ivec[0x3f] = vec;	/* floating point emulator */
}

/*
 * Emulate CPU instructions. We need this for VGA graphics, at least in the 16
 * color modes.
 *
 * The emulator is far from complete. We are adding the instructions as we
 * encounter them, so this function is likely to change over time. There are
 * no optimizations and we only emulate a single instruction at a time.
 *
 * As long as there is no support for DPMI or the Operand Size Override prefix
 * we won't need the 32-bit registers. This also means that the default
 * operand size is 16 bit.
 */
int
emu_instr(regcontext_t *REGS)
{
    int prefix = 1;
    u_int8_t *cs = (u_int8_t *)(R_CS << 4);
    int ip = R_IP;
    int dir, i, instrlen;
    u_int8_t *r8;
    u_int8_t val8;
    u_int16_t val16;
    u_int16_t *seg = &R_DS;
    u_int32_t addr, toaddr;
    
    while (prefix) {
	prefix = 0;
	switch (cs[ip]) {
	case 0x08:		/* or r/m8, r8 */
	    addr = decode_modrm(cs + ip, *seg, REGS, &instrlen);
	    r8 = reg8(cs[ip + 1], REGS);
	    val8 = read_byte(addr) | *r8;
	    write_byte(addr, val8);
	    /* clear carry and overflow; check zero, sign, parity */
	    R_EFLAGS &= ~PSL_C | ~PSL_V;
	    if (val8 == 0)
		R_EFLAGS |= PSL_Z;
	    if (val8 % 2 != 0)
		R_EFLAGS |= PSL_PF;
	    if (val8 & 0x80)
		R_EFLAGS |= PSL_N;
	    ip += 2 + instrlen;
	    break;
	case 0x22:		/* and r8, r/m8 */
	    addr = decode_modrm(cs + ip, *seg, REGS, &instrlen);
	    r8 = reg8(cs[ip + 1], REGS);
	    *r8 &= read_byte(addr);
	    /* clear carry and overflow; check zero, sign, parity */
	    R_EFLAGS &= ~PSL_C | ~PSL_V;
	    if (*r8 == 0)
		R_EFLAGS |= PSL_Z;
	    if (*r8 % 2 != 0)
		R_EFLAGS |= PSL_PF;
	    if (*r8 & 0x80)
		R_EFLAGS |= PSL_N;
	    ip += 2 + instrlen;
	    break;
	case 0x26:		/* Segment Override ES */
	    seg = &R_ES;
	    prefix = 1;
	    ip++;
	    break;
	case 0x2e:		/* Segment Override CS */
	    seg = &R_CS;
	    prefix = 1;
	    ip++;
	    break;
	case 0x36:		/* Segment Override SS */
	    seg = &R_SS;
	    prefix = 1;
	    ip++;
	    break;
	case 0x3e:		/* Segment Override DS */
	    seg = &R_DS;
	    prefix = 1;
	    ip++;
	    break;
	case 0x64:		/* Segment Override FS */
	    seg = &R_FS;
	    prefix = 1;
	    ip++;
	    break;
	case 0x65:		/* Segment Override GS */
	    seg = &R_GS;
	    prefix = 1;
	    ip++;
	    break;
	case 0x88:		/* mov r/m8, r8 */
	    addr = decode_modrm(cs + ip, *seg, REGS, &instrlen);
	    write_byte(addr, *reg8(cs[ip + 1], REGS));
	    ip += 2 + instrlen;
	    break;
	case 0x8a:		/* mov r8, r/m8 */
	    addr = decode_modrm(cs + ip, *seg, REGS, &instrlen);
	    r8 = reg8(cs[ip + 1], REGS);
	    *r8 = read_byte(addr);
	    ip += 2 + instrlen;
	    break;
	case 0xc6:		/* mov r/m8, imm8 */
	    addr = decode_modrm(cs + ip, *seg, REGS, &instrlen);
	    write_byte(addr, cs[ip + 2 + instrlen]);
	    ip += 2 + instrlen + 1;
	    break;
	case 0xc7:		/* mov r/m32/16, imm32/16 */
	    addr = decode_modrm(cs + ip, *seg, REGS, &instrlen);
	    val16 = *(u_int16_t *)&cs[ip + 2 + instrlen];
	    write_word(addr, val16);
	    ip += 2 + instrlen + 2;
	    break;
	case 0xa4:		/* movs m8, m8 */
	    write_byte(MAKEPTR(R_ES, R_DI), read_byte(MAKEPTR(*seg, R_SI)));
	    dir = (R_EFLAGS & PSL_D) ? -1 : 1;
	    R_DI += dir;
	    R_SI += dir;
	    ip++;
	    break;
	case 0xaa:		/* stos m8 */
	    addr = MAKEPTR(R_ES, R_DI);
	    write_byte(addr, R_AL);
	    R_DI += (R_EFLAGS & PSL_D) ? -1 : 1;
	    ip++;
	    break;
	case 0xab:		/* stos m32/16*/
	    addr = MAKEPTR(R_ES, R_DI);
	    write_word(addr, R_AX);
	    R_DI += (R_EFLAGS & PSL_D) ? -2 : 2;
	    ip++;
	    break;
	case 0xf3:		/* rep */
	    switch (cs[++ip]) {
	    case 0xa4:		/* movs m8, m8 */
		/* XXX Possible optimization: if both source and target
		   addresses lie within the video memory and write mode 1 is
		   selected, we can use memcpy(). */
		dir = (R_EFLAGS & PSL_D) ? -1 : 1;
		addr = MAKEPTR(R_ES, R_DI);
		toaddr = MAKEPTR(*seg, R_SI);
		for (i = R_CX; i > 0; i--) {
		    write_byte(addr, read_byte(toaddr));
		    addr += dir;
		    toaddr += dir;
		}
		PUTPTR(R_ES, R_DI, addr);
		PUTPTR(*seg, R_SI, toaddr);
		ip++;
		break;
	    case 0xaa:		/* stos m8 */
		/* direction */
		dir = (R_EFLAGS & PSL_D) ? -1 : 1;
		addr = MAKEPTR(R_ES, R_DI);
		for (i = R_CX; i > 0; i--) {
		    write_byte(addr, R_AL);
		    addr += dir;
		}
		PUTPTR(R_ES, R_DI, addr);
		ip++;
		break;
	    case 0xab:		/* stos m32/16 */
		/* direction */
		dir = (R_EFLAGS & PSL_D) ? -2 : 2;
		addr = MAKEPTR(R_ES, R_DI);
		for (i = R_CX; i > 0; i--) {
		    write_word(addr, R_AX);
		    addr += dir;
		}
		PUTPTR(R_ES, R_DI, addr);
		ip++;
		break;
	    default:
		R_IP = --ip;	/* Move IP back to the 'rep' instruction. */
		return -1;
	    }
	    R_CX = 0;
	    break;
	default:
	    /* Unknown instruction, get out of here and let trap.c:sigbus()
	       catch it. */
	    return -1;
	}
	R_IP = ip;
    }

    return 0;
}

/* Decode the ModR/M byte. Returns the memory address of the operand. 'c'
   points to the current instruction, 'seg' contains the value for the current
   base segment; this is usually 'DS', but may have been changed by a segment
   override prefix. We return the length of the current instruction in
   'instrlen' so we can adjust 'IP' on return.

   XXX We will probably need a second function for 32-bit instructions.

   XXX We do not check for undefined combinations, like Mod=01, R/M=001. */
static u_int32_t
decode_modrm(u_int8_t *c, u_int16_t seg, regcontext_t *REGS, int *instrlen)
{
    u_int32_t	addr = 0;		/* absolute address */
    int16_t	dspl = 0;		/* displacement, signed */
    *instrlen = 0;
    
    switch (c[1] & 0xc0) {	/* decode Mod */
    case 0x00:			/* DS:[reg] */
	/* 'reg' is selected in the R/M bits */
	break;
    case 0x40:			/* 8 bit displacement */
	dspl = (int16_t)(int8_t)c[2];
	*instrlen = 1;
	break;
    case 0x80:			/* 16 bit displacement */
	dspl = *(int16_t *)&c[2];
	*instrlen = 2;
	break;
    case 0xc0:			/* reg in R/M */
	if (c[0] & 1)		/* 16-bit reg */
	    return *reg16(c[1], REGS);
	else			/* 8-bit reg */
	    return *reg8(c[1], REGS);
	break;
    }
    
    switch (c[1] & 0x07) {	/* decode R/M */
    case 0x00:
	addr = MAKEPTR(seg, R_BX + R_SI);
	break;
    case 0x01:
	addr = MAKEPTR(seg, R_BX + R_DI);
	break;
    case 0x02:
	addr = MAKEPTR(seg, R_BP + R_SI);
	break;
    case 0x03:
	addr = MAKEPTR(seg, R_BP + R_DI);
	break;
    case 0x04:
	addr = MAKEPTR(seg, R_SI);
	break;
    case 0x05:
	addr = MAKEPTR(seg, R_DI);
	break;
    case 0x06:
	if ((c[1] & 0xc0) >= 0x40)
	    addr += R_BP;
	else {
	    addr = MAKEPTR(seg, *(int16_t *)&c[2]);
	    *instrlen = 2;
	}
	break;
    case 0x07:
	addr = MAKEPTR(seg, R_BX + dspl);
	break;
    }
    
    return addr;
}

static u_int8_t *
reg8(u_int8_t c, regcontext_t *REGS)
{
    u_int8_t *r8[] = {&R_AL, &R_CL, &R_DL, &R_BL,
		      &R_AH, &R_CH, &R_DH, &R_BH};

    /* select 'rrr' bits in ModR/M */
    return r8[(c & 0x34) >> 3];
}

static u_int16_t *
reg16(u_int8_t c, regcontext_t *REGS)
{
    u_int16_t *r16[] = {&R_AX, &R_CX, &R_DX, &R_BX,
			&R_SP, &R_BP, &R_SI, &R_DI};

    return r16[(c & 0x34) >> 3];
}

#if 0
/* not yet */
static u_int32_t *
reg32(u_int8_t c, regcontext_t *REGS)
{
    u_int32_t *r32[] = {&R_EAX, &R_ECX, &R_EDX, &R_EBX,
			&R_ESP, &R_EBP, &R_ESI, &R_EDI};

    return r32[(c & 0x34) >> 3];
}
#endif

/* Read an 8-bit value from the location specified by 'addr'. If 'addr' lies
   within the video memory, we call 'video.c:vga_read()'. */
static u_int8_t
read_byte(u_int32_t addr)
{
    if (addr >= 0xa0000 && addr < 0xb0000)
	return vga_read(addr);
    else
	return *(u_int8_t *)addr;
}

/* Write an 8-bit value to the location specified by 'addr'. If 'addr' lies
   within the video memory region, we call 'video.c:vga_write()'. */
static void
write_byte(u_int32_t addr, u_int8_t val)
{
    if (addr >= 0xa0000 && addr < 0xb0000)
	vga_write(addr, val);
    else
	*(u_int8_t *)addr = val;

    return;
}

/* Write a 16-bit value to the location specified by 'addr'. If 'addr' lies
   within the video memory region, we call 'video.c:vga_write()'. */
static void
write_word(u_int32_t addr, u_int16_t val)
{
    if (addr >= 0xa0000 && addr < 0xb0000) {
	vga_write(addr, (u_int8_t)(val & 0xff));
	vga_write(addr + 1, (u_int8_t)((val & 0xff00) >> 8));
    } else
	*(u_int16_t *)addr = val;

    return;
}
