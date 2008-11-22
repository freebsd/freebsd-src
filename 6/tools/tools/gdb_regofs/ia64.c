/*
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stddef.h>
#include <machine/reg.h>

#define PRESERVED(x)    offsetof(struct reg, r_preserved)       \
                        + offsetof(struct _callee_saved, x)
#define SCRATCH(x)      offsetof(struct reg, r_scratch)         \
                        + offsetof(struct _caller_saved, x)
#define SPECIAL(x)      offsetof(struct reg, r_special)         \
                        + offsetof(struct _special, x)

#define HIGH_FP(x)      offsetof(struct fpreg, fpr_high)        \
                        + offsetof(struct _high_fp, x)
#define PRESERVED_FP(x) offsetof(struct fpreg, fpr_preserved)   \
                        + offsetof(struct _callee_saved_fp, x)
#define SCRATCH_FP(x)   offsetof(struct fpreg, fpr_scratch)     \
                        + offsetof(struct _caller_saved_fp, x)

static int regofs[] = {
        /*
	 * General registers (0-127)
	 */
        -1,             /* gr0 */
        SPECIAL(gp),
        SCRATCH(gr2),   SCRATCH(gr3),
        PRESERVED(gr4), PRESERVED(gr5), PRESERVED(gr6), PRESERVED(gr7),
        SCRATCH(gr8),   SCRATCH(gr9),   SCRATCH(gr10),  SCRATCH(gr11),
        SPECIAL(sp),    SPECIAL(tp),
        SCRATCH(gr14),  SCRATCH(gr15),  SCRATCH(gr16),  SCRATCH(gr17),
        SCRATCH(gr18),  SCRATCH(gr19),  SCRATCH(gr20),  SCRATCH(gr21),
        SCRATCH(gr22),  SCRATCH(gr23),  SCRATCH(gr24),  SCRATCH(gr25),
        SCRATCH(gr26),  SCRATCH(gr27),  SCRATCH(gr28),  SCRATCH(gr29),
        SCRATCH(gr30),  SCRATCH(gr31),
        /*
         * gr32 through gr127 are not directly available as they are
         * stacked registers.
         */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr32-gr39 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr40-gr47 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr48-gr55 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr56-gr63 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr64-gr71 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr72-gr79 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr80-gr87 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr88-gr95 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr96-gr103 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr104-gr111 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr112-gr119 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* gr120-gr127 */
        /*
         * Floating-point registers (128-255)
         */
        -1,             /* fr0: constant 0.0 */
        -1,             /* fr1: constant 1.0 */
        PRESERVED_FP(fr2),      PRESERVED_FP(fr3),
        PRESERVED_FP(fr4),      PRESERVED_FP(fr5),
        SCRATCH_FP(fr6),        SCRATCH_FP(fr7),
        SCRATCH_FP(fr8),        SCRATCH_FP(fr9),
        SCRATCH_FP(fr10),       SCRATCH_FP(fr11),
        SCRATCH_FP(fr12),       SCRATCH_FP(fr13),
        SCRATCH_FP(fr14),       SCRATCH_FP(fr15),
        PRESERVED_FP(fr16),     PRESERVED_FP(fr17),
        PRESERVED_FP(fr18),     PRESERVED_FP(fr19),
        PRESERVED_FP(fr20),     PRESERVED_FP(fr21),
        PRESERVED_FP(fr22),     PRESERVED_FP(fr23),
        PRESERVED_FP(fr24),     PRESERVED_FP(fr25),
        PRESERVED_FP(fr26),     PRESERVED_FP(fr27),
        PRESERVED_FP(fr28),     PRESERVED_FP(fr29),
        PRESERVED_FP(fr30),     PRESERVED_FP(fr31),
        HIGH_FP(fr32),  HIGH_FP(fr33),  HIGH_FP(fr34),  HIGH_FP(fr35),
        HIGH_FP(fr36),  HIGH_FP(fr37),  HIGH_FP(fr38),  HIGH_FP(fr39),
        HIGH_FP(fr40),  HIGH_FP(fr41),  HIGH_FP(fr42),  HIGH_FP(fr43),
        HIGH_FP(fr44),  HIGH_FP(fr45),  HIGH_FP(fr46),  HIGH_FP(fr47),
        HIGH_FP(fr48),  HIGH_FP(fr49),  HIGH_FP(fr50),  HIGH_FP(fr51),
        HIGH_FP(fr52),  HIGH_FP(fr53),  HIGH_FP(fr54),  HIGH_FP(fr55),
        HIGH_FP(fr56),  HIGH_FP(fr57),  HIGH_FP(fr58),  HIGH_FP(fr59),
        HIGH_FP(fr60),  HIGH_FP(fr61),  HIGH_FP(fr62),  HIGH_FP(fr63),
        HIGH_FP(fr64),  HIGH_FP(fr65),  HIGH_FP(fr66),  HIGH_FP(fr67),
        HIGH_FP(fr68),  HIGH_FP(fr69),  HIGH_FP(fr70),  HIGH_FP(fr71),
        HIGH_FP(fr72),  HIGH_FP(fr73),  HIGH_FP(fr74),  HIGH_FP(fr75),
        HIGH_FP(fr76),  HIGH_FP(fr77),  HIGH_FP(fr78),  HIGH_FP(fr79),
        HIGH_FP(fr80),  HIGH_FP(fr81),  HIGH_FP(fr82),  HIGH_FP(fr83),
        HIGH_FP(fr84),  HIGH_FP(fr85),  HIGH_FP(fr86),  HIGH_FP(fr87),
        HIGH_FP(fr88),  HIGH_FP(fr89),  HIGH_FP(fr90),  HIGH_FP(fr91),
        HIGH_FP(fr92),  HIGH_FP(fr93),  HIGH_FP(fr94),  HIGH_FP(fr95),
        HIGH_FP(fr96),  HIGH_FP(fr97),  HIGH_FP(fr98),  HIGH_FP(fr99),
        HIGH_FP(fr100), HIGH_FP(fr101), HIGH_FP(fr102), HIGH_FP(fr103),
        HIGH_FP(fr104), HIGH_FP(fr105), HIGH_FP(fr106), HIGH_FP(fr107),
        HIGH_FP(fr108), HIGH_FP(fr109), HIGH_FP(fr110), HIGH_FP(fr111),
        HIGH_FP(fr112), HIGH_FP(fr113), HIGH_FP(fr114), HIGH_FP(fr115),
        HIGH_FP(fr116), HIGH_FP(fr117), HIGH_FP(fr118), HIGH_FP(fr119),
        HIGH_FP(fr120), HIGH_FP(fr121), HIGH_FP(fr122), HIGH_FP(fr123),
        HIGH_FP(fr124), HIGH_FP(fr125), HIGH_FP(fr126), HIGH_FP(fr127),
        /*
         * Predicate registers (256-319)
	 * These are not individually available. Predicates are
	 * in the pr register.
         */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* pr0-pr7 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* pr8-pr15 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* pr16-pr23 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* pr24-pr31 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* pr32-pr39 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* pr40-pr47 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* pr48-pr55 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* pr56-pr63 */
        /*
         * Branch registers (320-327)
         */
        SPECIAL(rp),
        PRESERVED(br1), PRESERVED(br2), PRESERVED(br3), PRESERVED(br4),
        PRESERVED(br5),
        SCRATCH(br6),   SCRATCH(br7),
        /*
         * Misc other registers (328-333)
         */
        -1, -1,
        SPECIAL(pr),
        SPECIAL(iip),
        SPECIAL(psr),
        SPECIAL(cfm),
        /*
         * Application registers (334-461)
         */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar.k0-ar.k7 */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar8-ar15 (reserved) */
        SPECIAL(rsc),                           /* ar.rsc */
        SPECIAL(ndirty),			/* ar.bsp !!YEDI!! */
        SPECIAL(bspstore),                      /* ar.bspstore */
        SPECIAL(rnat),                          /* ar.rnat */
        -1,                                     /* ar20 (reserved) */
        -1,                                     /* ar.fcr */
        -1, -1,                                 /* ar22-ar23 (reserved) */
        -1,                                     /* ar.eflag */
        SCRATCH(csd),                           /* ar.csd */
        SCRATCH(ssd),                           /* ar.ssd */
        -1,                                     /* ar.cflg */
        -1,                                     /* ar.fsr */
        -1,                                     /* ar.fir */
        -1,                                     /* ar.fdr */
        -1,                                     /* ar31 (reserved) */
        SCRATCH(ccv),                           /* ar.ccv */
        -1, -1, -1,                             /* ar33-ar35 (reserved) */
        SPECIAL(unat),                          /* ar.unat */
        -1, -1, -1,                             /* ar37-ar39 (reserved) */
        SPECIAL(fpsr),                          /* ar.fpsr */
        -1, -1, -1,                             /* ar41-ar43 (reserved) */
        -1,                                     /* ar.itc */
        -1, -1, -1,                             /* ar45-ar47 (reserved) */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar48-ar55 (ignored) */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar56-ar63 (ignored) */
        SPECIAL(pfs),                           /* ar.pfs */
        PRESERVED(lc),                          /* ar.lc */
        -1,                                     /* ar.ec */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar67-ar74 (reserved) */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar75-ar82 (reserved) */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar83-ar90 (reserved) */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar91-ar98 (reserved) */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar99-ar106 (reserved) */
        -1, -1, -1, -1, -1,                     /* ar107-ar111 (reserved) */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar112-ar119 (ignored) */
        -1, -1, -1, -1, -1, -1, -1, -1,         /* ar120-ar127 (ignored) */
};

int
main()
{
	int elem, nelems;

	nelems = sizeof(regofs)/sizeof(regofs[0]);
	printf("static int reg_offset[%d] = {", nelems);
	for (elem = 0; elem < nelems; elem++) {
		if ((elem & 7) == 0)
			printf("\n  ");
		printf("%4d", regofs[elem]);
		if (elem < nelems - 1)
			putchar(',');
		if ((elem & 7) != 7)
			putchar(' ');
		else
			printf("\t/* Regs %d-%d. */", elem - 7, elem);
	}
	printf("\n};");
	return (0);
}
