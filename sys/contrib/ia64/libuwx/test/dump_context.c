#include <stdio.h>
#include <inttypes.h>

#define IP 0
#define SP 1
#define BSP 2
#define CFM 3
#define RP 4
#define PSP 5
#define PFS 6
#define PREDS 7
#define PRIUNAT 8
#define AR_BSPSTORE 9
#define AR_RNAT 10
#define AR_UNAT 11
#define AR_FPSR 12
#define AR_LC 13
#define AR_PFS 14
#define GR4 16
#define GR5 17
#define GR6 18
#define GR7 19
#define BR1 20
#define BR2 21
#define BR3 22
#define BR4 23
#define BR5 24

void dump_context(uint64_t *context)
{
    int i, j;
    unsigned int valid;
    uint64_t val;
    static char *names[] = {
	/*  0 */ "ip", "sp", "bsp", "cfm",
	/*  4 */ "rp", "psp", "pfs", "preds",
	/*  8 */ "priunat", "ar.bspstore", "ar.rnat", "ar.unat",
	/* 12 */ "ar.fpsr", "ar.lc", "ar.pfs", "(pad)",
	/* 16 */ "gr4", "gr5", "gr6", "gr7",
	/* 20 */ "br1", "br2", "br3", "br4", "br5"
    };
    static int col1[] = {
	IP,
	SP,
	BSP,
	CFM,
	RP,
	PSP,
	PFS,
	AR_RNAT,
	AR_UNAT,
	AR_FPSR,
	AR_LC,
	AR_PFS,
    };
    static int col2[] = {
	PREDS,
	PRIUNAT,
	GR4,
	GR5,
	GR6,
	GR7,
	BR1,
	BR2,
	BR3,
	BR4,
	BR5,
    };

#define NCOL1 (sizeof(col1)/sizeof(int))
#define NCOL2 (sizeof(col2)/sizeof(int))
#define NPRINT (NCOL1 > NCOL2 ? NCOL1 : NCOL2)

    valid = (unsigned int)(context[0] >> 32);
    printf("  valid_regs (%08lx):", valid);
    for (i = 0; i <= BR5; i++) {
	if (valid & 1) printf(" %s", names[i]);
	valid >>= 1;
    }
    printf("\n");
    for (i = 0; i < NPRINT; i++) {
	if (i < NCOL1) {
	    j = col1[i];
	    val = context[j+1];
	    printf("  %-8s %08x %08x", names[j],
			(unsigned int)(val >> 32),
			(unsigned int)val);
	}
	else
	    printf("                            ");
	if (i < NCOL2) {
	    j = col2[i];
	    val = context[j+1];
	    printf("      %-8s %08x %08x", names[j],
			(unsigned int)(val >> 32),
			(unsigned int)val);
	}
	putchar('\n');
    }
}
