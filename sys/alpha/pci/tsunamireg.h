/* $FreeBSD$ */

/*
 * 21271 Chipset registers and constants.
 *
 * Taken from Tsunami/Typhoon Specification Rev. 1.2
 * and Compaq Professional Workstation XP1000: Technical
 * Information, both graciously provided by Don Rice 
 */


typedef struct {
	volatile u_int64_t reg __attribute__((aligned(64)));
} tsunami_reg;
					/* notes */
typedef struct { 
	tsunami_reg	csc;		/* rw */
	tsunami_reg	mtr;		/* rw */
	tsunami_reg	misc;		/* rw */
	tsunami_reg	mpd;		/* rw */
	tsunami_reg	aar0;		/* rw */
	tsunami_reg	aar1;		/* rw */
	tsunami_reg	aar2;		/* rw */
	tsunami_reg	aar3;		/* rw */
	tsunami_reg	dim0;		/* rw */
	tsunami_reg	dim1;		/* rw */
	tsunami_reg	dir0;		/* ro */
	tsunami_reg	dir1;		/* ro */
	tsunami_reg	drir;		/* ro */
	tsunami_reg	prben;		/* "special" */
	tsunami_reg	iic0;		/* rw */
	tsunami_reg	iic1;		/* rw */
	tsunami_reg	mpr0;		/* wo */
	tsunami_reg	mpr1;		/* wo */
	tsunami_reg	mpr2;		/* wo */
	tsunami_reg	mpr3;		/* wo */
	tsunami_reg	mctl;		/* rw, Tsunami only */
	tsunami_reg	ttr;		/* rw */
	tsunami_reg	tdr;		/* rw */
	tsunami_reg	dim2;		/* rw, Typhoon only */
	tsunami_reg	dim3;		/* rw, Typhoon only */
	tsunami_reg	dir2;		/* ro, Typhoon only */
	tsunami_reg	dir3;		/* ro, Typhoon only */
	tsunami_reg	iic2;		/* rw, Typhoon only */
	tsunami_reg	iic3;		/* rw, Typhoon only */
	tsunami_reg	pwr;		/* rw */
} tsunami_cchip;

/*
 *  cchip csc defines
 */
#define  CSC_P1P (1L << 14)     /* pchip1 present if this bit is set in 
				   chip->csc */

typedef struct {
	tsunami_reg	dsc;
	tsunami_reg	str;
	tsunami_reg	drev;
} tsunami_dchip;

typedef struct {
	tsunami_reg	wsba[4];	/* rw */
	tsunami_reg	wsm[4];		/* rw */
	tsunami_reg	tba[4];		/* rw */
	tsunami_reg	pctl;		/* rw */
	tsunami_reg	plat;		/* ro */
	tsunami_reg	reserved;	/* rw */
	tsunami_reg	perror;		/* rw */
	tsunami_reg	perrmask;	/* rw */
	tsunami_reg	perrset;	/* wo */
	tsunami_reg	tlbiv;		/* wo */
	tsunami_reg	tlbia;		/* wo */
	tsunami_reg	pmonctl;	/* rw */
	tsunami_reg	pmoncnt;	/* rw */
} tsunami_pchip;

/*
 * pchip window defines
 */
#define WINDOW_ENABLE           0x1
#define WINDOW_DISABLE          0x0
#define WINDOW_SCATTER_GATHER   0x2
#define WINDOW_DIRECT_MAPPED    0x0



#define KV(pa)          ALPHA_PHYS_TO_K0SEG(pa)

#define cchip		((tsunami_cchip *)(KV(0x101A0000000UL)))
#define dchip		((tsunami_dchip *)(KV(0x101B0000800UL)))
#define pchip0		((tsunami_pchip *)(KV(0x10180000000UL)))
#define pchip1		((tsunami_pchip *)(KV(0x10380000000UL)))

/*
 *   memory / i/o space macros
 *
 */
#define HOSE(h)			(((unsigned long)(h)) << 33)
#define TSUNAMI_MEM(h)		(0x10000000000UL + HOSE(h))
#define TSUNAMI_IACK_SC(h)	(0x101F8000000UL + HOSE(h))
#define TSUNAMI_IO(h)		(0x101FC000000UL + HOSE(h))
#define TSUNAMI_CONF(h)		(0x101FE000000UL + HOSE(h))


#define TSUNAMI_MAXHOSES 4
