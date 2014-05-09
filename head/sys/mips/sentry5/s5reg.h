/* $FreeBSD$ */

#ifndef _MIPS32_SENTRY5_SENTRY5REG_H_
#define _MIPS32_SENTRY5_SENTRY5REG_H_

#define	SENTRY5_UART0ADR	0x18000300
#define	SENTRY5_UART1ADR	0x18000400

/* Reset register implemented here in a PLD device. */
#define	SENTRY5_EXTIFADR	0x1F000000
#define	SENTRY5_DORESET		0x80

/*
 * Custom CP0 register macros.
 * XXX: This really needs the mips cpuregs.h file for the barrier.
 */
#define S5_RDRW32_C0P0_CUST22(n,r)				\
static __inline u_int32_t					\
s5_rd_ ## n (void)						\
{								\
	int v0;							\
	__asm __volatile ("mfc0 %[v0], $22, "__XSTRING(r)" ;"	\
			  : [v0] "=&r"(v0));			\
	/*mips_barrier();*/					\
	return (v0);						\
}								\
static __inline void						\
s5_wr_ ## n (u_int32_t a0)					\
{								\
	__asm __volatile ("mtc0 %[a0], $22, "__XSTRING(r)" ;"	\
			 __XSTRING(COP0_SYNC)";"		\
			 "nop;"					\
			 "nop;"					\
			 :					\
			 : [a0] "r"(a0));			\
	/*mips_barrier();*/					\
} struct __hack

/*
 * All 5 of these sub-registers are used by Linux.
 * There is a further custom register at 25 which is not used.
 */
#define	S5_CP0_DIAG	0
#define	S5_CP0_CLKCFG1	1
#define	S5_CP0_CLKCFG2	2
#define	S5_CP0_SYNC	3
#define	S5_CP0_CLKCFG3	4
#define	S5_CP0_RESET	5

/* s5_[rd|wr]_xxx() */
S5_RDRW32_C0P0_CUST22(diag, S5_CP0_DIAG);
S5_RDRW32_C0P0_CUST22(clkcfg1, S5_CP0_CLKCFG1);
S5_RDRW32_C0P0_CUST22(clkcfg2, S5_CP0_CLKCFG2);
S5_RDRW32_C0P0_CUST22(sync, S5_CP0_SYNC);
S5_RDRW32_C0P0_CUST22(clkcfg3, S5_CP0_CLKCFG3);
S5_RDRW32_C0P0_CUST22(reset, S5_CP0_RESET);

#endif /* _MIPS32_SENTRY5_SENTRY5REG_H_ */
