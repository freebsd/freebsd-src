/*
 * linux/include/linux/math_emu.h
 *
 * (C) 1991 Linus Torvalds
 *
 *	$Id: math_emu.h,v 1.5 1997/02/22 09:32:27 peter Exp $
 */
#ifndef _LINUX_MATH_EMU_H
#define _LINUX_MATH_EMU_H

/*#define math_abort(x,y) \
(((volatile void (*)(struct info *,unsigned int)) __math_abort)((x),(y)))*/

/*
 * Gcc forces this stupid alignment problem: I want to use only two int32_t's
 * for the temporary real 64-bit mantissa, but then gcc aligns out the
 * structure to 12 bytes which breaks things in math_emulate.c. Shit. I
 * want some kind of "no-alignt" pragma or something.
 */

typedef struct {
	int32_t a,b;
	short exponent;
} temp_real;

typedef struct {
	short m0,m1,m2,m3;
	short exponent;
} temp_real_unaligned;

#define real_to_real(a,b) \
((*(long long *) (b) = *(long long *) (a)),((b)->exponent = (a)->exponent))

typedef struct {
	int32_t a,b;
} long_real;

typedef int32_t short_real;

typedef struct {
	int32_t a,b;
	short sign;
} temp_int;

struct swd {
	int ie:1;
	int de:1;
	int ze:1;
	int oe:1;
	int ue:1;
	int pe:1;
	int sf:1;
	int ir:1;
	int c0:1;
	int c1:1;
	int c2:1;
	int top:3;
	int c3:1;
	int b:1;
};
struct i387_struct {
	int32_t	cwd;
	int32_t	swd;
	int32_t	twd;
	int32_t	fip;
	int32_t	fcs;
	int32_t	foo;
	int32_t	fos;
	int32_t	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

#define I387 (*(struct i387_struct *)&(((struct pcb *)curproc->p_addr)->pcb_savefpu))
#define SWD (*(struct swd *) &I387.swd)
#define ROUNDING ((I387.cwd >> 10) & 3)
#define PRECISION ((I387.cwd >> 8) & 3)

#define BITS24	0
#define BITS53	2
#define BITS64	3

#define ROUND_NEAREST	0
#define ROUND_DOWN	1
#define ROUND_UP	2
#define ROUND_0		3

#define CONSTZ   (temp_real_unaligned) {0x0000,0x0000,0x0000,0x0000,0x0000}
#define CONST1   (temp_real_unaligned) {0x0000,0x0000,0x0000,0x8000,0x3FFF}
#define CONSTPI  (temp_real_unaligned) {0xC235,0x2168,0xDAA2,0xC90F,0x4000}
#define CONSTLN2 (temp_real_unaligned) {0x79AC,0xD1CF,0x17F7,0xB172,0x3FFE}
#define CONSTLG2 (temp_real_unaligned) {0xF799,0xFBCF,0x9A84,0x9A20,0x3FFD}
#define CONSTL2E (temp_real_unaligned) {0xF0BC,0x5C17,0x3B29,0xB8AA,0x3FFF}
#define CONSTL2T (temp_real_unaligned) {0x8AFE,0xCD1B,0x784B,0xD49A,0x4000}

#define set_IE() (I387.swd |= 1)
#define set_DE() (I387.swd |= 2)
#define set_ZE() (I387.swd |= 4)
#define set_OE() (I387.swd |= 8)
#define set_UE() (I387.swd |= 16)
#define set_PE() (I387.swd |= 32)

#define set_C0() (I387.swd |= 0x0100)
#define set_C1() (I387.swd |= 0x0200)
#define set_C2() (I387.swd |= 0x0400)
#define set_C3() (I387.swd |= 0x4000)

/* ea.c */

static char * ea(struct trapframe *, unsigned short);

/* convert.c */

static void frndint(const temp_real * __a, temp_real * __b);
static void Fscale(const temp_real *, const temp_real *, temp_real *);
static void short_to_temp(const short_real * __a, temp_real * __b);
static void long_to_temp(const long_real * __a, temp_real * __b);
static void temp_to_short(const temp_real * __a, short_real * __b);
static void temp_to_long(const temp_real * __a, long_real * __b);
static void real_to_int(const temp_real * __a, temp_int * __b);
static void int_to_real(const temp_int * __a, temp_real * __b);

/* get_put.c */

static void get_short_real(temp_real *, struct trapframe *, unsigned short);
static void get_long_real(temp_real *, struct trapframe *, unsigned short);
static void get_temp_real(temp_real *, struct trapframe *, unsigned short);
static void get_short_int(temp_real *, struct trapframe *, unsigned short);
static void get_long_int(temp_real *, struct trapframe *, unsigned short);
static void get_longlong_int(temp_real *, struct trapframe *, unsigned short);
static void get_BCD(temp_real *, struct trapframe *, unsigned short);
static void put_short_real(const temp_real *, struct trapframe *, unsigned short);
static void put_long_real(const temp_real *, struct trapframe *, unsigned short);
static void put_temp_real(const temp_real *, struct trapframe *, unsigned short);
static void put_short_int(const temp_real *, struct trapframe *, unsigned short);
static void put_long_int(const temp_real *, struct trapframe *, unsigned short);
static void put_longlong_int(const temp_real *, struct trapframe *, unsigned short);
static void put_BCD(const temp_real *, struct trapframe *, unsigned short);

/* add.c */

static void fadd(const temp_real *, const temp_real *, temp_real *);

/* mul.c */

static void fmul(const temp_real *, const temp_real *, temp_real *);

/* div.c */

static void fdiv(const temp_real *, const temp_real *, temp_real *);

/* compare.c */

static void fcom(const temp_real *, const temp_real *);
static void fucom(const temp_real *, const temp_real *);
static void ftst(const temp_real *);

#endif
