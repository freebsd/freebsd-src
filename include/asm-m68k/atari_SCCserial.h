#ifndef _ATARI_SCCSERIAL_H
#define _ATARI_SCCSERIAL_H

/* Special configuration ioctls for the Atari SCC5380 Serial
 * Communications Controller
 */

/* ioctl command codes */

#define TIOCGATSCC	0x54c0	/* get SCC configuration */
#define TIOCSATSCC	0x54c1	/* set SCC configuration */
#define TIOCDATSCC	0x54c2	/* reset configuration to defaults */

/* Clock sources */

#define CLK_RTxC	0
#define CLK_TRxC	1
#define CLK_PCLK	2

/* baud_bases for the common clocks in the Atari. These are the real
 * frequencies divided by 16.
 */
   
#define SCC_BAUD_BASE_TIMC	19200	/* 0.3072 MHz from TT-MFP, Timer C */
#define SCC_BAUD_BASE_BCLK	153600	/* 2.4576 MHz */
#define SCC_BAUD_BASE_PCLK4	229500	/* 3.6720 MHz */
#define SCC_BAUD_BASE_PCLK	503374	/* 8.0539763 MHz */
#define SCC_BAUD_BASE_NONE	0		/* for not connected or unused
						 * clock sources */

#define SCC_BAUD_BASE_M147_PCLK	312500	/* 5 MHz */
#define SCC_BAUD_BASE_M147	312500	/* 5 MHz */
#define SCC_BAUD_BASE_MVME_PCLK	781250	/* 12.5 MHz */
#define SCC_BAUD_BASE_MVME	625000	/* 10.000 MHz */
#define SCC_BAUD_BASE_BVME_PCLK	781250	/* 12.5 MHz */   /* XXX ??? */
#define SCC_BAUD_BASE_BVME	460800	/* 7.3728 MHz */

/* The SCC configuration structure */

struct atari_SCCserial {
	unsigned	RTxC_base;	/* base_baud of RTxC */
	unsigned	TRxC_base;	/* base_baud of TRxC */
	unsigned	PCLK_base;	/* base_baud of PCLK, for both channels! */
	struct {
		unsigned clksrc;	/* CLK_RTxC, CLK_TRxC or CLK_PCLK */
		unsigned divisor;	/* divisor for base baud, valid values:
					 * see below */
	} baud_table[17];		/* For 50, 75, 110, 135, 150, 200, 300,
					 * 600, 1200, 1800, 2400, 4800, 9600,
					 * 19200, 38400, 57600 and 115200 bps. The
					 * last two could be replaced by other
					 * rates > 38400 if they're not possible.
					 */
};

/* The following divisors are valid:
 *
 *   - CLK_RTxC: 1 or even (1, 2 and 4 are the direct modes, > 4 use
 *               the BRG)
 *
 *   - CLK_TRxC: 1, 2 or 4 (no BRG, only direct modes possible)
 *
 *   - CLK_PCLK: >= 4 and even (no direct modes, only BRG)
 *
 */

#endif /* _ATARI_SCCSERIAL_H */
