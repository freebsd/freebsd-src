/*
 * $Id: softisr.h,v 1.2 1997/08/22 09:46:09 ShimonR Exp $
 */

#ifndef	_SOFTISR_H_
#define	_SOFTISR_H_

/*
 *	The following manifest constants are named w/o
 *	regard to imagination
 */
#define	DPTISR_DPT	 0
#define	DPTISR_1	 1
#define	DPTISR_2	 2
#define	DPTISR_3	 3
#define	DPTISR_4	 4
#define	DPTISR_5	 5
#define	DPTISR_6	 6
#define	DPTISR_7	 7
#define	DPTISR_8	 8
#define	DPTISR_9	 9
#define	DPTISR_10	10
#define	DPTISR_11	11
#define	DPTISR_12	12
#define	DPTISR_13	13
#define	DPTISR_14	14
#define	DPTISR_15	15
#define	DPTISR_16	16
#define	DPTISR_17	17
#define	DPTISR_18	18
#define	DPTISR_19	19
#define	DPTISR_20	20
#define	DPTISR_21	21
#define	DPTISR_22	22
#define	DPTISR_23	23
#define	DPTISR_24	24
#define	DPTISR_25	25
#define	DPTISR_26	26
#define	DPTISR_27	27
#define	DPTISR_28	28
#define	DPTISR_29	29
#define	DPTISR_30	30
#define	DPTISR_31	31

/*
 *	equivalent to schednetisr() for the DPT driver
 */

#ifndef setsoftdpt
extern void setsoftdpt(void);
#endif

#define	scheddptisr(anisr)	{ dptisr |= 1<<(anisr); setsoftdpt(); }

#ifndef	LOCORE
#ifdef	KERNEL
extern	volatile unsigned int dptisr; /* Scheduling bits for DPT driver */

typedef void dptisr_t(void);

struct dptisrtab {
	int sint_num;
	dptisr_t *sint_isr;
};

#define DPTISR_SET(num, isr) \
	static struct dptisrtab mod_sint = { num, isr}; \
	DATA_SET(dptisr_set, mod_sint);

int register_dptisr __P((int, dptisr_t *));

#endif	/* #ifdef  KERNEL */
#endif	/* #ifndef LOCORE */

#endif	/* #ifndef _SOFTISR_H_ */
