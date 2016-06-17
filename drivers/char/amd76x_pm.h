/* 
 * Begin 765/766
 */
/* C2/C3/POS options in C3A50, page 63 in AMD-766 doc */
#define ZZ_CACHE_EN	1
#define DCSTOP_EN	(1 << 1)
#define STPCLK_EN	(1 << 2)
#define CPUSTP_EN	(1 << 3)
#define PCISTP_EN	(1 << 4)
#define CPUSLP_EN	(1 << 5)
#define SUSPND_EN	(1 << 6)
#define CPURST_EN	(1 << 7)

#define C2_REGS		0
#define C3_REGS		8
#define POS_REGS	16	
/*
 * End 765/766
 */


/*
 * Begin 768
 */
/* C2/C3 options in DevB:3x4F, page 100 in AMD-768 doc */
#define C2EN		1
#define C3EN		(1 << 1)
#define ZZ_C3EN		(1 << 2)
#define CSLP_C3EN	(1 << 3)
#define CSTP_C3EN	(1 << 4)

/* POS options in DevB:3x50, page 101 in AMD-768 doc */
#define POSEN	1
#define CSTP	(1 << 2)
#define PSTP	(1 << 3)
#define ASTP	(1 << 4)
#define DCSTP	(1 << 5)
#define CSLP	(1 << 6)
#define SUSP	(1 << 8)
#define MSRSM	(1 << 14)
#define PITRSM	(1 << 15)

/* NTH options DevB:3x40, pg 93 of 768 doc */
#define NTPER(x) (x << 3)
#define THMINEN(x) (x << 4)

/*
 * End 768
 */

/* NTH activate. PM10, pg 110 of 768 doc, pg 70 of 766 doc */
#define NTH_RATIO(x) (x << 1)
#define NTH_EN (1 << 4)

/* Sleep state. PM04, pg 109 of 768 doc, pg 69 of 766 doc */
#define SLP_EN (1 << 13)
#define SLP_TYP(x) (x << 10)

#define LAZY_IDLE_DELAY	800	/* 0: Best savings,  3000: More responsive */
