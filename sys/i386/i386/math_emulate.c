/*
 * linux/kernel/math/math_emulate.c
 *
 * (C) 1991 Linus Torvalds
 *
 * [expediant "port" of linux 8087 emulator to 386BSD, with apologies -wfj]
 *
 *	from: 386BSD 0.1
 *	$Id: math_emulate.c,v 1.15 1995/12/07 12:45:33 davidg Exp $
 */

/*
 * Limited emulation 27.12.91 - mostly loads/stores, which gcc wants
 * even for soft-float, unless you use bruce evans' patches. The patches
 * are great, but they have to be re-applied for every version, and the
 * library is different for soft-float and 80387. So emulation is more
 * practical, even though it's slower.
 *
 * 28.12.91 - loads/stores work, even BCD. I'll have to start thinking
 * about add/sub/mul/div. Urgel. I should find some good source, but I'll
 * just fake up something.
 *
 * 30.12.91 - add/sub/mul/div/com seem to work mostly. I should really
 * test every possible combination.
 */

/*
 * This file is full of ugly macros etc: one problem was that gcc simply
 * didn't want to make the structures as they should be: it has to try to
 * align them. Sickening code, but at least I've hidden the ugly things
 * in this one file: the other files don't need to know about these things.
 *
 * The other files also don't care about ST(x) etc - they just get addresses
 * to 80-bit temporary reals, and do with them as they please. I wanted to
 * hide most of the 387-specific things here.
 */

#include <sys/param.h>
#include <sys/systm.h>

#ifdef LKM
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/lkm.h>
#endif

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>

#include <sys/proc.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/signal.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>

#define __ALIGNED_TEMP_REAL 1
#include "math_emu.h"

#define bswapw(x) __asm__("xchgb %%al,%%ah":"=a" (x):"0" ((short)x))
#define ST(x) (*__st((x)))
#define PST(x) ((const temp_real *) __st((x)))
#define	math_abort(tfp, signo) tfp->tf_eip = oldeip; return (signo);

/*
 * We don't want these inlined - it gets too messy in the machine-code.
 */
static void fpop(void);
static void fpush(void);
static void fxchg(temp_real_unaligned * a, temp_real_unaligned * b);
static temp_real_unaligned * __st(int i);

static unsigned char
get_fs_byte(char *adr) 
	{ return(fubyte(adr)); }

static unsigned short
get_fs_word(unsigned short *adr)
	{ return(fuword(adr)); }

static unsigned long 
get_fs_long(unsigned long *adr)
	{ return(fuword(adr)); }

static void 
put_fs_byte(unsigned char val, char *adr)
	{ (void)subyte(adr,val); }

static void 
put_fs_word(unsigned short val, short *adr)
	{ (void)susword(adr,val); }

static void 
put_fs_long(u_long val, unsigned long *adr)
	{ (void)suword(adr,val); }

static int
math_emulate(struct trapframe * info)
{
	unsigned short code;
	temp_real tmp;
	char * address;
	u_long oldeip;

	/* ever used fp? */
	if ((((struct pcb *)curproc->p_addr)->pcb_flags & FP_SOFTFP) == 0) {
		((struct pcb *)curproc->p_addr)->pcb_flags |= FP_SOFTFP;
		I387.cwd = 0x037f;
		I387.swd = 0x0000;
		I387.twd = 0x0000;
	}

	if (I387.cwd & I387.swd & 0x3f)
		I387.swd |= 0x8000;
	else
		I387.swd &= 0x7fff;
	oldeip = info->tf_eip;
/* 0x001f means user code space */
	if ((u_short)info->tf_cs != 0x001F) {
		printf("math_emulate: %04x:%08lx\n", (u_short)info->tf_cs,
			oldeip);
		panic("?Math emulation needed in kernel?");
	}
	code = get_fs_word((unsigned short *) oldeip);
	bswapw(code);
	code &= 0x7ff;
	I387.fip = oldeip;
	*(unsigned short *) &I387.fcs = (u_short) info->tf_cs;
	*(1+(unsigned short *) &I387.fcs) = code;
	info->tf_eip += 2;
	switch (code) {
		case 0x1d0: /* fnop */
			return(0);
		case 0x1d1: case 0x1d2: case 0x1d3:  /* fst to 32-bit mem */
		case 0x1d4: case 0x1d5: case 0x1d6: case 0x1d7:
			math_abort(info,SIGILL);
		case 0x1e0: /* fchs */
			ST(0).exponent ^= 0x8000;
			return(0);
		case 0x1e1: /* fabs */
			ST(0).exponent &= 0x7fff;
			return(0);
		case 0x1e2: case 0x1e3:
			math_abort(info,SIGILL);
		case 0x1e4: /* ftst */
			ftst(PST(0));
			return(0);
		case 0x1e5: /* fxam */
			printf("fxam not implemented\n");
			math_abort(info,SIGILL);
		case 0x1e6: case 0x1e7: /* fldenv */
			math_abort(info,SIGILL);
		case 0x1e8: /* fld1 */
			fpush();
			ST(0) = CONST1;
			return(0);
		case 0x1e9: /* fld2t */
			fpush();
			ST(0) = CONSTL2T;
			return(0);
		case 0x1ea: /* fld2e */
			fpush();
			ST(0) = CONSTL2E;
			return(0);
		case 0x1eb: /* fldpi */
			fpush();
			ST(0) = CONSTPI;
			return(0);
		case 0x1ec: /* fldlg2 */
			fpush();
			ST(0) = CONSTLG2;
			return(0);
		case 0x1ed: /* fldln2 */
			fpush();
			ST(0) = CONSTLN2;
			return(0);
		case 0x1ee: /* fldz */
			fpush();
			ST(0) = CONSTZ;
			return(0);
		case 0x1ef:
			math_abort(info,SIGILL);
		case 0x1f0: /* f2xm1 */
		case 0x1f1: /* fyl2x */
		case 0x1f2: /* fptan */
		case 0x1f3: /* fpatan */
		case 0x1f4: /* fxtract */
		case 0x1f5: /* fprem1 */
		case 0x1f6: /* fdecstp */
		case 0x1f7: /* fincstp */
		case 0x1f8: /* fprem */
		case 0x1f9: /* fyl2xp1 */
		case 0x1fa: /* fsqrt */
		case 0x1fb: /* fsincos */
		case 0x1fe: /* fsin */
		case 0x1ff: /* fcos */
			uprintf(
			 "math_emulate: instruction %04x not implemented\n",
			  code + 0xd800);
			math_abort(info,SIGILL);
		case 0x1fc: /* frndint */
			frndint(PST(0),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0x1fd: /* fscale */
			/* incomplete and totally inadequate -wfj */
			Fscale(PST(0), PST(1), &tmp);
			real_to_real(&tmp,&ST(0));
			return(0);			/* 19 Sep 92*/
		case 0x2e9: /* ????? */
/* if this should be a fucomp ST(0),ST(1) , it must be a 0x3e9  ATS */
			fucom(PST(1),PST(0));
			fpop(); fpop();
			return(0);
		case 0x3d0: case 0x3d1: /* fist ?? */
			return(0);
		case 0x3e2: /* fclex */
			I387.swd &= 0x7f00;
			return(0);
		case 0x3e3: /* fninit */
			I387.cwd = 0x037f;
			I387.swd = 0x0000;
			I387.twd = 0x0000;
			return(0);
		case 0x3e4:
			return(0);
		case 0x6d9: /* fcompp */
			fcom(PST(1),PST(0));
			fpop(); fpop();
			return(0);
		case 0x7e0: /* fstsw ax */
			*(short *) &info->tf_eax = I387.swd;
			return(0);
	}
	switch (code >> 3) {
		case 0x18: /* fadd */
			fadd(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0x19: /* fmul */
			fmul(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0x1a: /* fcom */
			fcom(PST(code & 7),PST(0));
			return(0);
		case 0x1b: /* fcomp */
			fcom(PST(code & 7),PST(0));
			fpop();
			return(0);
		case 0x1c: /* fsubr */
			real_to_real(&ST(code & 7),&tmp);
			tmp.exponent ^= 0x8000;
			fadd(PST(0),&tmp,&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0x1d: /* fsub */
			ST(0).exponent ^= 0x8000;
			fadd(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0x1e: /* fdivr */
			fdiv(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0x1f: /* fdiv */
			fdiv(PST(code & 7),PST(0),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0x38: /* fld */
			fpush();
			ST(0) = ST((code & 7)+1);  /* why plus 1 ????? ATS */
			return(0);
		case 0x39: /* fxch */
			fxchg(&ST(0),&ST(code & 7));
			return(0);
		case 0x3b: /*  ??? ??? wrong ???? ATS */
			ST(code & 7) = ST(0);
			fpop();
			return(0);
		case 0x98: /* fadd */
			fadd(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			return(0);
		case 0x99: /* fmul */
			fmul(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			return(0);
		case 0x9a: /* ???? , my manual don't list a direction bit
for fcom , ??? ATS */
			fcom(PST(code & 7),PST(0));
			return(0);
		case 0x9b: /* same as above , ATS */
			fcom(PST(code & 7),PST(0));
			fpop();
			return(0);
		case 0x9c: /* fsubr */
			ST(code & 7).exponent ^= 0x8000;
			fadd(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			return(0);
		case 0x9d: /* fsub */
			real_to_real(&ST(0),&tmp);
			tmp.exponent ^= 0x8000;
			fadd(PST(code & 7),&tmp,&tmp);
			real_to_real(&tmp,&ST(code & 7));
			return(0);
		case 0x9e: /* fdivr */
			fdiv(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			return(0);
		case 0x9f: /* fdiv */
			fdiv(PST(code & 7),PST(0),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			return(0);
		case 0xb8: /* ffree */
			printf("ffree not implemented\n");
			math_abort(info,SIGILL);
		case 0xb9: /* fstp ???? where is the pop ? ATS */
			fxchg(&ST(0),&ST(code & 7));
			return(0);
		case 0xba: /* fst */
			ST(code & 7) = ST(0);
			return(0);
		case 0xbb: /* ????? encoding of fstp to mem ? ATS */
			ST(code & 7) = ST(0);
			fpop();
			return(0);
		case 0xbc: /* fucom */
			fucom(PST(code & 7),PST(0));
			return(0);
		case 0xbd: /* fucomp */
			fucom(PST(code & 7),PST(0));
			fpop();
			return(0);
		case 0xd8: /* faddp */
			fadd(PST(code & 7),PST(0),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			fpop();
			return(0);
		case 0xd9: /* fmulp */
			fmul(PST(code & 7),PST(0),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			fpop();
			return(0);
		case 0xda: /* ??? encoding of ficom with 16 bit mem ? ATS */
			fcom(PST(code & 7),PST(0));
			fpop();
			return(0);
		case 0xdc: /* fsubrp */
			ST(code & 7).exponent ^= 0x8000;
			fadd(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			fpop();
			return(0);
		case 0xdd: /* fsubp */
			real_to_real(&ST(0),&tmp);
			tmp.exponent ^= 0x8000;
			fadd(PST(code & 7),&tmp,&tmp);
			real_to_real(&tmp,&ST(code & 7));
			fpop();
			return(0);
		case 0xde: /* fdivrp */
			fdiv(PST(0),PST(code & 7),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			fpop();
			return(0);
		case 0xdf: /* fdivp */
			fdiv(PST(code & 7),PST(0),&tmp);
			real_to_real(&tmp,&ST(code & 7));
			fpop();
			return(0);
		case 0xf8: /* fild 16-bit mem ???? ATS */
			printf("ffree not implemented\n");
			math_abort(info,SIGILL);
			fpop();
			return(0);
		case 0xf9: /*  ????? ATS */
			fxchg(&ST(0),&ST(code & 7));
			return(0);
		case 0xfa: /* fist 16-bit mem ? ATS */
		case 0xfb: /* fistp 16-bit mem ? ATS */
			ST(code & 7) = ST(0);
			fpop();
			return(0);
	}
	switch ((code>>3) & 0xe7) {
		case 0x22:
			put_short_real(PST(0),info,code);
			return(0);
		case 0x23:
			put_short_real(PST(0),info,code);
			fpop();
			return(0);
		case 0x24:
			address = ea(info,code);
			for (code = 0 ; code < 7 ; code++) {
				((long *) & I387)[code] =
				   get_fs_long((unsigned long *) address);
				address += 4;
			}
			return(0);
		case 0x25:
			address = ea(info,code);
			*(unsigned short *) &I387.cwd =
				get_fs_word((unsigned short *) address);
			return(0);
		case 0x26:
			address = ea(info,code);
			/*verify_area(address,28);*/
			for (code = 0 ; code < 7 ; code++) {
				put_fs_long( ((long *) & I387)[code],
					(unsigned long *) address);
				address += 4;
			}
			return(0);
		case 0x27:
			address = ea(info,code);
			/*verify_area(address,2);*/
			put_fs_word(I387.cwd,(short *) address);
			return(0);
		case 0x62:
			put_long_int(PST(0),info,code);
			return(0);
		case 0x63:
			put_long_int(PST(0),info,code);
			fpop();
			return(0);
		case 0x65:
			fpush();
			get_temp_real(&tmp,info,code);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0x67:
			put_temp_real(PST(0),info,code);
			fpop();
			return(0);
		case 0xa2:
			put_long_real(PST(0),info,code);
			return(0);
		case 0xa3:
			put_long_real(PST(0),info,code);
			fpop();
			return(0);
		case 0xa4:
			address = ea(info,code);
			for (code = 0 ; code < 27 ; code++) {
				((long *) & I387)[code] =
				   get_fs_long((unsigned long *) address);
				address += 4;
			}
			return(0);
		case 0xa6:
			address = ea(info,code);
			/*verify_area(address,108);*/
			for (code = 0 ; code < 27 ; code++) {
				put_fs_long( ((long *) & I387)[code],
					(unsigned long *) address);
				address += 4;
			}
			I387.cwd = 0x037f;
			I387.swd = 0x0000;
			I387.twd = 0x0000;
			return(0);
		case 0xa7:
			address = ea(info,code);
			/*verify_area(address,2);*/
			put_fs_word(I387.swd,(short *) address);
			return(0);
		case 0xe2:
			put_short_int(PST(0),info,code);
			return(0);
		case 0xe3:
			put_short_int(PST(0),info,code);
			fpop();
			return(0);
		case 0xe4:
			fpush();
			get_BCD(&tmp,info,code);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0xe5:
			fpush();
			get_longlong_int(&tmp,info,code);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 0xe6:
			put_BCD(PST(0),info,code);
			fpop();
			return(0);
		case 0xe7:
			put_longlong_int(PST(0),info,code);
			fpop();
			return(0);
	}
	switch (code >> 9) {
		case 0:
			get_short_real(&tmp,info,code);
			break;
		case 1:
			get_long_int(&tmp,info,code);
			break;
		case 2:
			get_long_real(&tmp,info,code);
			break;
		case 4:
			get_short_int(&tmp,info,code);
	}
	switch ((code>>3) & 0x27) {
		case 0:
			fadd(&tmp,PST(0),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 1:
			fmul(&tmp,PST(0),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 2:
			fcom(&tmp,PST(0));
			return(0);
		case 3:
			fcom(&tmp,PST(0));
			fpop();
			return(0);
		case 4:
			tmp.exponent ^= 0x8000;
			fadd(&tmp,PST(0),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 5:
			ST(0).exponent ^= 0x8000;
			fadd(&tmp,PST(0),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 6:
			fdiv(PST(0),&tmp,&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
		case 7:
			fdiv(&tmp,PST(0),&tmp);
			real_to_real(&tmp,&ST(0));
			return(0);
	}
	if ((code & 0x138) == 0x100) {
			fpush();
			real_to_real(&tmp,&ST(0));
			return(0);
	}
	printf("Unknown math-insns: %04x:%08x %04x\n",(u_short)info->tf_cs,
		info->tf_eip,code);
	math_abort(info,SIGFPE);
}

static void
fpop(void)
{
	unsigned long tmp;

	tmp = I387.swd & 0xffffc7ffUL;
	I387.swd += 0x00000800;
	I387.swd &= 0x00003800;
	I387.swd |= tmp;
}

static void
fpush(void)
{
	unsigned long tmp;

	tmp = I387.swd & 0xffffc7ffUL;
	I387.swd += 0x00003800;
	I387.swd &= 0x00003800;
	I387.swd |= tmp;
}

static void 
fxchg(temp_real_unaligned * a, temp_real_unaligned * b)
{
	temp_real_unaligned c;

	c = *a;
	*a = *b;
	*b = c;
}

static temp_real_unaligned *
__st(int i)
{
	i += I387.swd >> 11;
	i &= 7;
	return (temp_real_unaligned *) (i*10 + (char *)(I387.st_space));
}

/*
 * linux/kernel/math/ea.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * Calculate the effective address.
 */


static int __regoffset[] = {
	tEAX, tECX, tEDX, tEBX, tESP, tEBP, tESI, tEDI
};

#define REG(x) (curproc->p_md.md_regs[__regoffset[(x)]])

static char *
sib(struct trapframe * info, int mod)
{
	unsigned char ss,index,base;
	long offset = 0;

	base = get_fs_byte((char *) info->tf_eip);
	info->tf_eip++;
	ss = base >> 6;
	index = (base >> 3) & 7;
	base &= 7;
	if (index == 4)
		offset = 0;
	else
		offset = REG(index);
	offset <<= ss;
	if (mod || base != 5)
		offset += REG(base);
	if (mod == 1) {
		offset += (signed char) get_fs_byte((char *) info->tf_eip);
		info->tf_eip++;
	} else if (mod == 2 || base == 5) {
		offset += (signed) get_fs_long((unsigned long *) info->tf_eip);
		info->tf_eip += 4;
	}
	I387.foo = offset;
	I387.fos = 0x17;
	return (char *) offset;
}

static char *
ea(struct trapframe * info, unsigned short code)
{
	unsigned char mod,rm;
	long * tmp;
	int offset = 0;

	mod = (code >> 6) & 3;
	rm = code & 7;
	if (rm == 4 && mod != 3)
		return sib(info,mod);
	if (rm == 5 && !mod) {
		offset = get_fs_long((unsigned long *) info->tf_eip);
		info->tf_eip += 4;
		I387.foo = offset;
		I387.fos = 0x17;
		return (char *) offset;
	}
	tmp = (long *) &REG(rm);
	switch (mod) {
		case 0: offset = 0; break;
		case 1:
			offset = (signed char) get_fs_byte((char *) info->tf_eip);
			info->tf_eip++;
			break;
		case 2:
			offset = (signed) get_fs_long((unsigned long *) info->tf_eip);
			info->tf_eip += 4;
			break;
#ifdef notyet
		case 3:
			math_abort(info,1<<(SIGILL-1));
#endif
	}
	I387.foo = offset;
	I387.fos = 0x17;
	return offset + (char *) *tmp;
}
/*
 * linux/kernel/math/get_put.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * This file handles all accesses to user memory: getting and putting
 * ints/reals/BCD etc. This is the only part that concerns itself with
 * other than temporary real format. All other cals are strictly temp_real.
 */

static void 
get_short_real(temp_real * tmp, struct trapframe * info, unsigned short code)
{
	char * addr;
	short_real sr;

	addr = ea(info,code);
	sr = get_fs_long((unsigned long *) addr);
	short_to_temp(&sr,tmp);
}

static void
get_long_real(temp_real * tmp, struct trapframe * info, unsigned short code)
{
	char * addr;
	long_real lr;

	addr = ea(info,code);
	lr.a = get_fs_long((unsigned long *) addr);
	lr.b = get_fs_long(1 + (unsigned long *) addr);
	long_to_temp(&lr,tmp);
}

static void
get_temp_real(temp_real * tmp, struct trapframe * info, unsigned short code)
{
	char * addr;

	addr = ea(info,code);
	tmp->a = get_fs_long((unsigned long *) addr);
	tmp->b = get_fs_long(1 + (unsigned long *) addr);
	tmp->exponent = get_fs_word(4 + (unsigned short *) addr);
}

static void
get_short_int(temp_real * tmp, struct trapframe * info, unsigned short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	ti.a = (signed short) get_fs_word((unsigned short *) addr);
	ti.b = 0;
	if (ti.sign = (ti.a < 0))
		ti.a = - ti.a;
	int_to_real(&ti,tmp);
}

static void
get_long_int(temp_real * tmp, struct trapframe * info, unsigned short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	ti.a = get_fs_long((unsigned long *) addr);
	ti.b = 0;
	if (ti.sign = (ti.a < 0))
		ti.a = - ti.a;
	int_to_real(&ti,tmp);
}

static void 
get_longlong_int(temp_real * tmp, struct trapframe * info, unsigned short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	ti.a = get_fs_long((unsigned long *) addr);
	ti.b = get_fs_long(1 + (unsigned long *) addr);
	if (ti.sign = (ti.b < 0))
		__asm__("notl %0 ; notl %1\n\t"
			"addl $1,%0 ; adcl $0,%1"
			:"=r" (ti.a),"=r" (ti.b)
			:"0" (ti.a),"1" (ti.b));
	int_to_real(&ti,tmp);
}

#define MUL10(low,high) \
__asm__("addl %0,%0 ; adcl %1,%1\n\t" \
"movl %0,%%ecx ; movl %1,%%ebx\n\t" \
"addl %0,%0 ; adcl %1,%1\n\t" \
"addl %0,%0 ; adcl %1,%1\n\t" \
"addl %%ecx,%0 ; adcl %%ebx,%1" \
:"=a" (low),"=d" (high) \
:"0" (low),"1" (high):"cx","bx")

#define ADD64(val,low,high) \
__asm__("addl %4,%0 ; adcl $0,%1":"=r" (low),"=r" (high) \
:"0" (low),"1" (high),"r" ((unsigned long) (val)))

static void
get_BCD(temp_real * tmp, struct trapframe * info, unsigned short code)
{
	int k;
	char * addr;
	temp_int i;
	unsigned char c;

	addr = ea(info,code);
	addr += 9;
	i.sign = 0x80 & get_fs_byte(addr--);
	i.a = i.b = 0;
	for (k = 0; k < 9; k++) {
		c = get_fs_byte(addr--);
		MUL10(i.a, i.b);
		ADD64((c>>4), i.a, i.b);
		MUL10(i.a, i.b);
		ADD64((c&0xf), i.a, i.b);
	}
	int_to_real(&i,tmp);
}

static void 
put_short_real(const temp_real * tmp,
	struct trapframe * info, unsigned short code)
{
	char * addr;
	short_real sr;

	addr = ea(info,code);
	/*verify_area(addr,4);*/
	temp_to_short(tmp,&sr);
	put_fs_long(sr,(unsigned long *) addr);
}

static void
put_long_real(const temp_real * tmp,
	struct trapframe * info, unsigned short code)
{
	char * addr;
	long_real lr;

	addr = ea(info,code);
	/*verify_area(addr,8);*/
	temp_to_long(tmp,&lr);
	put_fs_long(lr.a, (unsigned long *) addr);
	put_fs_long(lr.b, 1 + (unsigned long *) addr);
}

static void
put_temp_real(const temp_real * tmp,
	struct trapframe * info, unsigned short code)
{
	char * addr;

	addr = ea(info,code);
	/*verify_area(addr,10);*/
	put_fs_long(tmp->a, (unsigned long *) addr);
	put_fs_long(tmp->b, 1 + (unsigned long *) addr);
	put_fs_word(tmp->exponent, 4 + (short *) addr);
}

static void
put_short_int(const temp_real * tmp,
	struct trapframe * info, unsigned short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	real_to_int(tmp,&ti);
	/*verify_area(addr,2);*/
	if (ti.sign)
		ti.a = -ti.a;
	put_fs_word(ti.a,(short *) addr);
}

static void
put_long_int(const temp_real * tmp,
	struct trapframe * info, unsigned short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	real_to_int(tmp,&ti);
	/*verify_area(addr,4);*/
	if (ti.sign)
		ti.a = -ti.a;
	put_fs_long(ti.a,(unsigned long *) addr);
}

static void
put_longlong_int(const temp_real * tmp,
	struct trapframe * info, unsigned short code)
{
	char * addr;
	temp_int ti;

	addr = ea(info,code);
	real_to_int(tmp,&ti);
	/*verify_area(addr,8);*/
	if (ti.sign)
		__asm__("notl %0 ; notl %1\n\t"
			"addl $1,%0 ; adcl $0,%1"
			:"=r" (ti.a),"=r" (ti.b)
			:"0" (ti.a),"1" (ti.b));
	put_fs_long(ti.a,(unsigned long *) addr);
	put_fs_long(ti.b,1 + (unsigned long *) addr);
}

#define DIV10(low,high,rem) \
__asm__("divl %6 ; xchgl %1,%2 ; divl %6" \
	:"=d" (rem),"=a" (low),"=r" (high) \
	:"0" (0),"1" (high),"2" (low),"c" (10))

static void
put_BCD(const temp_real * tmp,struct trapframe * info, unsigned short code)
{
	int k,rem;
	char * addr;
	temp_int i;
	unsigned char c;

	addr = ea(info,code);
	/*verify_area(addr,10);*/
	real_to_int(tmp,&i);
	if (i.sign)
		put_fs_byte(0x80, addr+9);
	else
		put_fs_byte(0, addr+9);
	for (k = 0; k < 9; k++) {
		DIV10(i.a,i.b,rem);
		c = rem;
		DIV10(i.a,i.b,rem);
		c += rem<<4;
		put_fs_byte(c,addr++);
	}
}

/*
 * linux/kernel/math/mul.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * temporary real multiplication routine.
 */


static void
shift(int * c)
{
	__asm__("movl (%0),%%eax ; addl %%eax,(%0)\n\t"
		"movl 4(%0),%%eax ; adcl %%eax,4(%0)\n\t"
		"movl 8(%0),%%eax ; adcl %%eax,8(%0)\n\t"
		"movl 12(%0),%%eax ; adcl %%eax,12(%0)"
		::"r" ((long) c):"ax");
}

static void
mul64(const temp_real * a, const temp_real * b, int * c)
{
	__asm__("movl (%0),%%eax\n\t"
		"mull (%1)\n\t"
		"movl %%eax,(%2)\n\t"
		"movl %%edx,4(%2)\n\t"
		"movl 4(%0),%%eax\n\t"
		"mull 4(%1)\n\t"
		"movl %%eax,8(%2)\n\t"
		"movl %%edx,12(%2)\n\t"
		"movl (%0),%%eax\n\t"
		"mull 4(%1)\n\t"
		"addl %%eax,4(%2)\n\t"
		"adcl %%edx,8(%2)\n\t"
		"adcl $0,12(%2)\n\t"
		"movl 4(%0),%%eax\n\t"
		"mull (%1)\n\t"
		"addl %%eax,4(%2)\n\t"
		"adcl %%edx,8(%2)\n\t"
		"adcl $0,12(%2)"
		::"S" ((long) a),"c" ((long) b),"D" ((long) c)
		:"ax","dx");
}

static void
fmul(const temp_real * src1, const temp_real * src2, temp_real * result)
{
	int i,sign;
	int tmp[4] = {0,0,0,0};

	sign = (src1->exponent ^ src2->exponent) & 0x8000;
	i = (src1->exponent & 0x7fff) + (src2->exponent & 0x7fff) - 16383 + 1;
	if (i<0) {
		result->exponent = sign;
		result->a = result->b = 0;
		return;
	}
	if (i>0x7fff) {
		set_OE();
		return;
	}
	mul64(src1,src2,tmp);
	if (tmp[0] || tmp[1] || tmp[2] || tmp[3])
		while (i && tmp[3] >= 0) {
			i--;
			shift(tmp);
		}
	else
		i = 0;
	result->exponent = i | sign;
	result->a = tmp[2];
	result->b = tmp[3];
}

/*
 * linux/kernel/math/div.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * temporary real division routine.
 */

static void 
shift_left(int * c)
{
	__asm__ __volatile__("movl (%0),%%eax ; addl %%eax,(%0)\n\t"
		"movl 4(%0),%%eax ; adcl %%eax,4(%0)\n\t"
		"movl 8(%0),%%eax ; adcl %%eax,8(%0)\n\t"
		"movl 12(%0),%%eax ; adcl %%eax,12(%0)"
		::"r" ((long) c):"ax");
}

static void
shift_right(int * c)
{
	__asm__("shrl $1,12(%0) ; rcrl $1,8(%0) ; rcrl $1,4(%0) ; rcrl $1,(%0)"
		::"r" ((long) c));
}

static int
try_sub(int * a, int * b)
{
	char ok;

	__asm__ __volatile__("movl (%1),%%eax ; subl %%eax,(%2)\n\t"
		"movl 4(%1),%%eax ; sbbl %%eax,4(%2)\n\t"
		"movl 8(%1),%%eax ; sbbl %%eax,8(%2)\n\t"
		"movl 12(%1),%%eax ; sbbl %%eax,12(%2)\n\t"
		"setae %%al":"=a" (ok):"c" ((long) a),"d" ((long) b));
	return ok;
}

static void
div64(int * a, int * b, int * c)
{
	int tmp[4];
	int i;
	unsigned int mask = 0;

	c += 4;
	for (i = 0 ; i<64 ; i++) {
		if (!(mask >>= 1)) {
			c--;
			mask = 0x80000000UL;
		}
		tmp[0] = a[0]; tmp[1] = a[1];
		tmp[2] = a[2]; tmp[3] = a[3];
		if (try_sub(b,tmp)) {
			*c |= mask;
			a[0] = tmp[0]; a[1] = tmp[1];
			a[2] = tmp[2]; a[3] = tmp[3];
		}
		shift_right(b);
	}
}

static void
fdiv(const temp_real * src1, const temp_real * src2, temp_real * result)
{
	int i,sign;
	int a[4],b[4],tmp[4] = {0,0,0,0};

	sign = (src1->exponent ^ src2->exponent) & 0x8000;
	if (!(src2->a || src2->b)) {
		set_ZE();
		return;
	}
	i = (src1->exponent & 0x7fff) - (src2->exponent & 0x7fff) + 16383;
	if (i<0) {
		set_UE();
		result->exponent = sign;
		result->a = result->b = 0;
		return;
	}
	a[0] = a[1] = 0;
	a[2] = src1->a;
	a[3] = src1->b;
	b[0] = b[1] = 0;
	b[2] = src2->a;
	b[3] = src2->b;
	while (b[3] >= 0) {
		i++;
		shift_left(b);
	}
	div64(a,b,tmp);
	if (tmp[0] || tmp[1] || tmp[2] || tmp[3]) {
		while (i && tmp[3] >= 0) {
			i--;
			shift_left(tmp);
		}
		if (tmp[3] >= 0)
			set_DE();
	} else
		i = 0;
	if (i>0x7fff) {
		set_OE();
		return;
	}
	if (tmp[0] || tmp[1])
		set_PE();
	result->exponent = i | sign;
	result->a = tmp[2];
	result->b = tmp[3];
}

/*
 * linux/kernel/math/add.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * temporary real addition routine.
 *
 * NOTE! These aren't exact: they are only 62 bits wide, and don't do
 * correct rounding. Fast hack. The reason is that we shift right the
 * values by two, in order not to have overflow (1 bit), and to be able
 * to move the sign into the mantissa (1 bit). Much simpler algorithms,
 * and 62 bits (61 really - no rounding) accuracy is usually enough. The
 * only time you should notice anything weird is when adding 64-bit
 * integers together. When using doubles (52 bits accuracy), the
 * 61-bit accuracy never shows at all.
 */

#define NEGINT(a) \
__asm__("notl %0 ; notl %1 ; addl $1,%0 ; adcl $0,%1" \
	:"=r" (a->a),"=r" (a->b) \
	:"0" (a->a),"1" (a->b))

static void signify(temp_real * a)
{
	a->exponent += 2;
	__asm__("shrdl $2,%1,%0 ; shrl $2,%1"
		:"=r" (a->a),"=r" (a->b)
		:"0" (a->a),"1" (a->b));
	if (a->exponent < 0)
		NEGINT(a);
	a->exponent &= 0x7fff;
}

static void unsignify(temp_real * a)
{
	if (!(a->a || a->b)) {
		a->exponent = 0;
		return;
	}
	a->exponent &= 0x7fff;
	if (a->b < 0) {
		NEGINT(a);
		a->exponent |= 0x8000;
	}
	while (a->b >= 0) {
		a->exponent--;
		__asm__("addl %0,%0 ; adcl %1,%1"
			:"=r" (a->a),"=r" (a->b)
			:"0" (a->a),"1" (a->b));
	}
}

static void
fadd(const temp_real * src1, const temp_real * src2, temp_real * result)
{
	temp_real a,b;
	int x1,x2,shift;

	x1 = src1->exponent & 0x7fff;
	x2 = src2->exponent & 0x7fff;
	if (x1 > x2) {
		a = *src1;
		b = *src2;
		shift = x1-x2;
	} else {
		a = *src2;
		b = *src1;
		shift = x2-x1;
	}
	if (shift >= 64) {
		*result = a;
		return;
	}
	if (shift >= 32) {
		b.a = b.b;
		b.b = 0;
		shift -= 32;
	}
	__asm__("shrdl %4,%1,%0 ; shrl %4,%1"
		:"=r" (b.a),"=r" (b.b)
		:"0" (b.a),"1" (b.b),"c" ((char) shift));
	signify(&a);
	signify(&b);
	__asm__("addl %4,%0 ; adcl %5,%1"
		:"=r" (a.a),"=r" (a.b)
		:"0" (a.a),"1" (a.b),"g" (b.a),"g" (b.b));
	unsignify(&a);
	*result = a;
}

/*
 * linux/kernel/math/compare.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * temporary real comparison routines
 */


#define clear_Cx() (I387.swd &= ~0x4500)

static void 
normalize(temp_real * a)
{
	int i = a->exponent & 0x7fff;
	int sign = a->exponent & 0x8000;

	if (!(a->a || a->b)) {
		a->exponent = 0;
		return;
	}
	while (i && a->b >= 0) {
		i--;
		__asm__("addl %0,%0 ; adcl %1,%1"
			:"=r" (a->a),"=r" (a->b)
			:"0" (a->a),"1" (a->b));
	}
	a->exponent = i | sign;
}

static void
ftst(const temp_real * a)
{
	temp_real b;

	clear_Cx();
	b = *a;
	normalize(&b);
	if (b.a || b.b || b.exponent) {
		if (b.exponent < 0)
			set_C0();
	} else
		set_C3();
}

static void
fcom(const temp_real * src1, const temp_real * src2)
{
	temp_real a;

	a = *src1;
	a.exponent ^= 0x8000;
	fadd(&a,src2,&a);
	ftst(&a);
}

static void
fucom(const temp_real * src1, const temp_real * src2)
{
	fcom(src1,src2);
}

/*
 * linux/kernel/math/convert.c
 *
 * (C) 1991 Linus Torvalds
 */


/*
 * NOTE!!! There is some "non-obvious" optimisations in the temp_to_long
 * and temp_to_short conversion routines: don't touch them if you don't
 * know what's going on. They are the adding of one in the rounding: the
 * overflow bit is also used for adding one into the exponent. Thus it
 * looks like the overflow would be incorrectly handled, but due to the
 * way the IEEE numbers work, things are correct.
 *
 * There is no checking for total overflow in the conversions, though (ie
 * if the temp-real number simply won't fit in a short- or long-real.)
 */

static void
short_to_temp(const short_real * a, temp_real * b)
{
	if (!(*a & 0x7fffffff)) {
		b->a = b->b = 0;
		if (*a)
			b->exponent = 0x8000;
		else
			b->exponent = 0;
		return;
	}
	b->exponent = ((*a>>23) & 0xff)-127+16383;
	if (*a<0)
		b->exponent |= 0x8000;
	b->b = (*a<<8) | 0x80000000UL;
	b->a = 0;
}

static void
long_to_temp(const long_real * a, temp_real * b)
{
	if (!a->a && !(a->b & 0x7fffffff)) {
		b->a = b->b = 0;
		if (a->b)
			b->exponent = 0x8000;
		else
			b->exponent = 0;
		return;
	}
	b->exponent = ((a->b >> 20) & 0x7ff)-1023+16383;
	if (a->b<0)
		b->exponent |= 0x8000;
	b->b = 0x80000000UL | (a->b<<11) | (((unsigned long)a->a)>>21);
	b->a = a->a<<11;
}

static void 
temp_to_short(const temp_real * a, short_real * b)
{
	if (!(a->exponent & 0x7fff)) {
		*b = (a->exponent)?0x80000000UL:0;
		return;
	}
	*b = ((((long) a->exponent)-16383+127) << 23) & 0x7f800000;
	if (a->exponent < 0)
		*b |= 0x80000000UL;
	*b |= (a->b >> 8) & 0x007fffff;
	switch ((int)ROUNDING) {
		case ROUND_NEAREST:
			if ((a->b & 0xff) > 0x80)
				++*b;
			break;
		case ROUND_DOWN:
			if ((a->exponent & 0x8000) && (a->b & 0xff))
				++*b;
			break;
		case ROUND_UP:
			if (!(a->exponent & 0x8000) && (a->b & 0xff))
				++*b;
			break;
	}
}

static void
temp_to_long(const temp_real * a, long_real * b)
{
	if (!(a->exponent & 0x7fff)) {
		b->a = 0;
		b->b = (a->exponent)?0x80000000UL:0;
		return;
	}
	b->b = (((0x7fff & (long) a->exponent)-16383+1023) << 20) & 0x7ff00000;
	if (a->exponent < 0)
		b->b |= 0x80000000UL;
	b->b |= (a->b >> 11) & 0x000fffff;
	b->a = a->b << 21;
	b->a |= (a->a >> 11) & 0x001fffff;
	switch ((int)ROUNDING) {
		case ROUND_NEAREST:
			if ((a->a & 0x7ff) > 0x400)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
		case ROUND_DOWN:
			if ((a->exponent & 0x8000) && (a->b & 0xff))
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
		case ROUND_UP:
			if (!(a->exponent & 0x8000) && (a->b & 0xff))
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
	}
}

static void 
frndint(const temp_real * a, temp_real * b)
{
	int shift =  16383 + 63 - (a->exponent & 0x7fff);
	unsigned long underflow;

	if ((shift < 0) || (shift == 16383+63)) {
		*b = *a;
		return;
	}
	b->a = b->b = underflow = 0;
	b->exponent = a->exponent;
	if (shift < 32) {
		b->b = a->b; b->a = a->a;
	} else if (shift < 64) {
		b->a = a->b; underflow = a->a;
		shift -= 32;
		b->exponent += 32;
	} else if (shift < 96) {
		underflow = a->b;
		shift -= 64;
		b->exponent += 64;
	} else {
		underflow = 1;
		shift = 0;
	}
	b->exponent += shift;
	__asm__("shrdl %2,%1,%0"
		:"=r" (underflow),"=r" (b->a)
		:"c" ((char) shift),"0" (underflow),"1" (b->a));
	__asm__("shrdl %2,%1,%0"
		:"=r" (b->a),"=r" (b->b)
		:"c" ((char) shift),"0" (b->a),"1" (b->b));
	__asm__("shrl %1,%0"
		:"=r" (b->b)
		:"c" ((char) shift),"0" (b->b));
	switch ((int)ROUNDING) {
		case ROUND_NEAREST:
			__asm__("addl %4,%5 ; adcl $0,%0 ; adcl $0,%1"
				:"=r" (b->a),"=r" (b->b)
				:"0" (b->a),"1" (b->b)
				,"r" (0x7fffffff + (b->a & 1))
				,"m" (*&underflow));
			break;
		case ROUND_UP:
			if ((b->exponent >= 0) && underflow)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
		case ROUND_DOWN:
			if ((b->exponent < 0) && underflow)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
	}
	if (b->a || b->b)
		while (b->b >= 0) {
			b->exponent--;
			__asm__("addl %0,%0 ; adcl %1,%1"
				:"=r" (b->a),"=r" (b->b)
				:"0" (b->a),"1" (b->b));
		}
	else
		b->exponent = 0;
}

static void
Fscale(const temp_real *a, const temp_real *b, temp_real *c)
{
	temp_int ti;

	*c = *a;
	if(!c->a && !c->b) {				/* 19 Sep 92*/
		c->exponent = 0;
		return;
	}
	real_to_int(b, &ti);
	if(ti.sign)
		c->exponent -= ti.a;
	else
		c->exponent += ti.a;
}

static void
real_to_int(const temp_real * a, temp_int * b)
{
	int shift =  16383 + 63 - (a->exponent & 0x7fff);
	unsigned long underflow;

	b->a = b->b = underflow = 0;
	b->sign = (a->exponent < 0);
	if (shift < 0) {
		set_OE();
		return;
	}
	if (shift < 32) {
		b->b = a->b; b->a = a->a;
	} else if (shift < 64) {
		b->a = a->b; underflow = a->a;
		shift -= 32;
	} else if (shift < 96) {
		underflow = a->b;
		shift -= 64;
	} else {
		underflow = 1;
		shift = 0;
	}
	__asm__("shrdl %2,%1,%0"
		:"=r" (underflow),"=r" (b->a)
		:"c" ((char) shift),"0" (underflow),"1" (b->a));
	__asm__("shrdl %2,%1,%0"
		:"=r" (b->a),"=r" (b->b)
		:"c" ((char) shift),"0" (b->a),"1" (b->b));
	__asm__("shrl %1,%0"
		:"=r" (b->b)
		:"c" ((char) shift),"0" (b->b));
	switch ((int)ROUNDING) {
		case ROUND_NEAREST:
			__asm__("addl %4,%5 ; adcl $0,%0 ; adcl $0,%1"
				:"=r" (b->a),"=r" (b->b)
				:"0" (b->a),"1" (b->b)
				,"r" (0x7fffffff + (b->a & 1))
				,"m" (*&underflow));
			break;
		case ROUND_UP:
			if (!b->sign && underflow)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
		case ROUND_DOWN:
			if (b->sign && underflow)
				__asm__("addl $1,%0 ; adcl $0,%1"
					:"=r" (b->a),"=r" (b->b)
					:"0" (b->a),"1" (b->b));
			break;
	}
}

static void
int_to_real(const temp_int * a, temp_real * b)
{
	b->a = a->a;
	b->b = a->b;
	if (b->a || b->b)
		b->exponent = 16383 + 63 + (a->sign? 0x8000:0);
	else {
		b->exponent = 0;
		return;
	}
	while (b->b >= 0) {
		b->exponent--;
		__asm__("addl %0,%0 ; adcl %1,%1"
			:"=r" (b->a),"=r" (b->b)
			:"0" (b->a),"1" (b->b));
	}
}

#ifdef LKM
MOD_MISC(fpu);
static int
fpu_load(struct lkm_table *lkmtp, int cmd)
{
	if (pmath_emulate) {
		printf("Math emulator already present\n");
		return EBUSY;
	}
	pmath_emulate = math_emulate;
	return 0;
}

static int
fpu_unload(struct lkm_table *lkmtp, int cmd)
{
	if (pmath_emulate != math_emulate) {
		printf("Cannot unload another math emulator\n");
		return EACCES;
	}
	pmath_emulate = 0;
	return 0;
}

int
fpu(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, fpu_load, fpu_unload, lkm_nullcmd);
}
#else /* !LKM */

static void
fpu_init(void)
{
	if (pmath_emulate)
		printf("Another Math emulator already present\n");
	else
		pmath_emulate = math_emulate;
}

SYSINIT(fpu, SI_SUB_CPU, SI_ORDER_ANY, fpu_init, NULL);

#endif /* LKM */
