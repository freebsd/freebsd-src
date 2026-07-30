/*
 * Copyright (c) 2026 Justin Hibbits <jhibbits@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/endian.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <opencrypto/cryptodev.h>
#include <opencrypto/xform_auth.h>

#include "sec_var.h"
#include "cryptodev_if.h"

/*
 * Most of this work is based on the T2080 Security (SEC) Reference Manual.
 *
 * The driver uses the Job Ring interface for all jobs.  The QI interface can be
 * added if IPSec, OVPN, or kTLS acceleration is added.
 */

/* From T2080 Security Reference Manual */
#define	SEC_MAX_SHDESC_WORDS	62

#define	SEC_MAX_JR	4	/* T2080 exposes four Job Rings */

/* CCSR register offsets. */
#define	SEC_MCFGR		0x0004
#define	  MCFGR_SWRST		  0x80000000	/* Software reset */
#define	  MCFGR_WDE		  0x40000000	/* DECO watchdog enable */
#define	  MCFGR_WDF		  0x20000000	/* Watchdog fast (test only) */
#define	  MCFGR_DMARST		  0x10000000	/* DMA reset (with SWRST) */
#define	  MCFGR_WRHD		  0x08000000	/* Write handoff disable */
#define	  MCFGR_DJPC		  0x00200000	/* Disable job perf ctrs */
#define	  MCFGR_DBPC		  0x00100000	/* Disable byte perf ctrs */
#define	  MCFGR_PS		  0x00010000	/* Large pointers */
#define	  MCFGR_ARCACHE_M	  0x0000f000	/* AXI read cache attrs */
#define	  MCFGR_AWCACHE_M	  0x00000f00	/* AXI write cache attrs */
#define	  MCFGR_AXIPRI		  0x00000008	/* AXI master priority */
#define	  MCFGR_LARGE_BURST	  0x00000004	/* Enable 256B bursts */
#define	SEC_SCFGR		0x000c
#define	  SCFGR_VIRT_EN		  0x00008000	/* Virtualization enabled */

#define	SEC_RDSTA		0x06c0		/* RNG DRNG Status */
#define	  RDSTA_IF0		  0x00000001	/* State handle 0 up */
#define	  RDSTA_IF1		  0x00000002	/* State handle 1 up */
#define	  RDSTA_ERRCODE_M	  0x000f0000
#define	  RDSTA_ERRCODE_S	  16
#define	  RDSTA_CE		  0x00100000	/* Catastrophic error */

/* DECO direct-access registers */
#define	SEC_DECORR		0x009c		/* DECO Request Register */
#define	  DECORR_DEN0		  0x00010000	/* DECO0 enable (RO, bit 16) */
#define	  DECORR_RQD0		  0x00000001	/* DECO0 request */
#define	SEC_D0LIODNR_MS		0x00a0
#define	SEC_D0LIODNR_LS		0x00a4
#define	SEC_D0JQCR_MS		0x8800		/* JQCR upper: WHL/FOUR/SOB */
#define	  DAJQCR_MS_WHL		  0x20000000	/* Whole descriptor loaded */
#define	  DAJQCR_MS_FOUR	  0x10000000	/* >= 4 words in first burst */
#define	  DAJQCR_MS_SOB		  0x00010000	/* Shared/burst loaded */
#define	  DAJQCR_MS_SRC_M	  0x00000700	/* Job source */
#define	  DAJQCR_MS_SRC_S	  8
#define	SEC_D0JQCR_LS		0x8804
#define	SEC_D0DAR_MS		0x8808		/* Descriptor address, upper */
#define	SEC_D0DAR_LS		0x880c
#define	SEC_D0DESB(n)		(0x8a00 + (n) * 4)	/* n = 0..63 */
#define	SEC_D0DDR		0x8e04		/* Debug status */
#define	  DADDR_VALID		  0x80000000	/* Job currently running */
#define	  DADDR_DECO_STATE_M	  0x00f00000	/* Main state machine */
#define	  DADDR_DECO_STATE_S	  20

/* Fault-address registers. */
#define	SEC_FAR_HI		0x0fc0		/* Fault Address, upper */
#define	SEC_FAR_LO		0x0fc4		/* Fault Address, lower */
#define	SEC_FALR		0x0fc8		/* Fault Address LIODN */
#define	SEC_FADR		0x0fcc		/* Fault Address Detail */
#define	  FADR_FERR_M		  0xc0000000	/* AXI error response */
#define	  FADR_FERR_S		  30
#define	  FADR_FSZ_EXT_M	  0x00070000	/* Transfer size high 3 bits */
#define	  FADR_FSZ_EXT_S	  16
#define	  FADR_DTYP		  0x00008000	/* 0=message, 1=control */
#define	  FADR_JSRC_M		  0x00007000	/* Job source */
#define	  FADR_JSRC_S		  12
#define	  FADR_BLKID_M		  0x00000f00	/* SEC internal block ID */
#define	  FADR_BLKID_S		  8
#define	  FADR_TYP		  0x00000080	/* 0=read, 1=write */
#define	  FADR_FSZ_M		  0x0000007f	/* Transfer size low 7 bits */

#define	SEC_RD4(sc, off)	bus_read_4((sc)->sc_rres, (off))
#define	SEC_WR4(sc, off, v)	bus_write_4((sc)->sc_rres, (off), (v))

/* Descriptor command components */
/* SEQ commands are intended for network protocols */
#define	CMD_DESC(n)		((n) << 27)
#define	CMD_KEY			0x00	/* Pointer/key follows descriptor */
#define	CMD_SEQ_KEY		0x01
#define	  KEY_CLASS_M		  0x06000000
#define	  KEY_CLASS_1		  0x02000000
#define	  KEY_CLASS_2		  0x04000000
#define	  KEY_SGF		  0x01000000	/* KEY - Pointer to SGT */
#define	  KEY_VLF		  0x01000000	/* SK - variable length */
#define	  KEY_IMM		  0x00800000	/* KEY - Key follows descriptor */
#define	  KEY_AIDF		  0x00800000	/* SK - Already in Input FIFO */
#define	  KEY_ENC		  0x00400000	/* Key is encrypted */
#define	  KEY_NWB		  0x00200000	/* No write back */
#define	  KEY_EKT		  0x00100000	/* Encrypted Key Type:
						 * 0 - AES-CCB
						 * 1 - AES-CCM
						 */
#define	  KEY_KDEST_M		  0x00030000	/* Key Destination */
#define	  KEY_KDEST_REG		  0x00000000	/* Dest is Key register */
#define	  KEY_KDEST_PKHA	  0x00010000	/* Dest is PKHA E-memory */
#define	  KEY_KDEST_AFHA	  0x00020000	/* Dest is AFHA S-Box */
#define	  KEY_KDEST_MDHA_SPLIT	  0x00030000	/* Key is MDHA split key */
#define	  KEY_TK		  0x00008000	/* Trusted Key */
#define	  KEY_LENGTH_M		  0x000003ff	/* Key length */
#define	CMD_LOAD		0x02
#define	CMD_SEQ_LOAD		0x03
#define	  LOAD_CLASS_M		  0x06000000
#define	  LOAD_CLASS_1		  0x02000000
#define	  LOAD_CLASS_2		  0x04000000
#define	  LOAD_CLASS_3		  0x06000000
#define	  LOAD_SGF		  0x01000000	/* LOAD - Pointer to SGT */
#define	  LOAD_VLF		  0x01000000	/* SL - variable length */
#define	  LOAD_IMM		  0x00800000	/* LOAD - Data follows descriptor */
#define	  LOAD_DST_M		  0x007f0000	/* Destination register */
#define	  LOAD_DST_S		  16
#define	  LOAD_KSR		  0x00010000	/* Key Size Register (C1/C2) */
#define	  LOAD_DSR		  0x00020000	/* Data Size Register (C1/C2) */
#define	  LOAD_ICVS		  0x00030000	/* ICV Size Register (C1/C2) */
#define	  LOAD_LSR		  0x00040000	/* LIODN Status Register (C3) */
#define	  LOAD_DCTRL2		  0x00050000	/* DECO Control Register 2(C3) */
#define	  LOAD_CCTRL		  0x00060000	/* CHA Control Register (C1) */
#define	  LOAD_DCTRL		  0x00060000	/* DECO Control Register (C3) */
#define	  LOAD_ICTRL		  0x00070000	/* IRQ Control Register (C0) */
#define	  LOAD_DPOVRD		  0x00070000	/* DECO Protocol Override (C3) */
#define	  LOAD_CLRW		  0x00080000	/* Clear Written Register (C0) */
#define	  LOAD_MATH0W		  0x00080000	/* DECO Math Register 0 (C3) */
#define	  LOAD_MATH1W		  0x00090000	/* DECO Math Register 1 (C3) */
#define	  LOAD_MATH2W		  0x000a0000	/* DECO Math Register 2 (C3) */
#define	  LOAD_CISEL		  0x000a0000	/* CHA Instance Select Reg (C0) */
#define	  LOAD_AADSZ		  0x000b0000	/* AAD Size Register (C1) */
#define	  LOAD_MAT3W		  0x000b0000	/* DECO Math Register 3 (C3) */
#define	  LOAD_C1VSZ		  0x000c0000	/* Class 1 IV SIze Register (C1) */
#define	  LOAD_ALTDS1		  0x000f0000	/* Alternate Data Size C1 (C1) */
#define	  LOAD_PKASZ		  0x00100000	/* PKHA A Size Register (C1) */
#define	  LOAD_PKBSZ		  0x00110000	/* PKHA B Size Register (C1) */
#define	  LOAD_PKNSZ		  0x00120000	/* PKHA N Size Register (C1) */
#define	  LOAD_PKESZ		  0x00130000	/* PKHA E Size Register (C1) */
#define	  LOAD_CTX		  0x00200000	/* Context Register (C1/C2) */
#define	  LOAD_KEY		  0x00400000	/* Key Register (C1/C2) */
#define	  LOAD_DESC_BUF		  0x00400000	/* DECO Descriptor Buffer (C3) */
#define	  LOAD_NFSL		  0x00700000	/* NFIFO and size registers (C0) */
#define	  LOAD_NFSM		  0x00710000	/* NFIFO and size registers (C0) */
#define	  LOAD_NFL		  0x00720000	/* NFIFO (C0) */
#define	  LOAD_NFM		  0x00730000	/* NFIFO (C0) */
#define	  LOAD_SL		  0x00740000	/* Size register(s) (C0) */
#define	  LOAD_SM		  0x00750000	/* Size register(s) (C0) */
#define	  LOAD_IDFNS		  0x00760000	/* Input Data FIFO Nibble Shift (C0) */
#define	  LOAD_ODFNS		  0x00770000	/* Output Data FIFO Nibble Shift (C0) */
#define	  LOAD_AUXDATA		  0x00780000	/* Aux Data FIFO (C0) */
#define	  LOAD_NFIFO		  0x007a0000	/* NFIFO (C0) */
#define	  LOAD_IFIFO		  0x007c0000	/* Input Data FIFO (C0) */
#define	  LOAD_OFIFO		  0x007e0000	/* Output Data FIFO (C0) */
#define	  LOAD_LENGTH_M		  0x000000ff	/* Data length (8 bits) */
#define	  LOAD_OFFSET_S		  8		/* OFFSET field shift (bits 8-15) */
#define	CMD_FIFO_LOAD		0x04
#define	CMD_SEQ_FIFO_LOAD	0x05
#define	CMD_STORE		0x0a
#define	CMD_SEQ_STORE		0x0b
#define	CMD_FIFO_STORE		0x0c
#define	CMD_SEQ_FIFO_STORE	0x0d
#define	CMD_MOVE		0x0e
#define	CMD_MOVE_LEN		0x0f
#define	CMD_OPERATION		0x10
#define	  OPTYPE_M		  0x07000000
#define	  OPTYPE_S		  24
#define	  OPTYPE_CLASS1_ALG	  0x02000000
#define	  OPTYPE_CLASS2_ALG	  0x04000000
#define	  ALG_S			  16
#define	  CMD_ALGORITHM(m, n)	  ((m) | ((n) << ALG_S))
/* Class 1 algorithms */
#define	  ALG_AES		  CMD_ALGORITHM(OPTYPE_CLASS1_ALG, 0x10)
#define	  ALG_DES		  CMD_ALGORITHM(OPTYPE_CLASS1_ALG, 0x20)
#define	  ALG_3DES		  CMD_ALGORITHM(OPTYPE_CLASS1_ALG, 0x21)
#define	  ALG_ARC4		  CMD_ALGORITHM(OPTYPE_CLASS1_ALG, 0x30)
#define	  ALG_RNG		  CMD_ALGORITHM(OPTYPE_CLASS1_ALG, 0x50)
#define	  ALG_SNOW3G_F8		  CMD_ALGORITHM(OPTYPE_CLASS1_ALG, 0x60)
#define	  ALG_KASUMI		  CMD_ALGORITHM(OPTYPE_CLASS1_ALG, 0x70)
#define	  ALG_ZUC_ENC		  CMD_ALGORITHM(OPTYPE_CLASS1_ALG, 0xb0)
/* Class 2 algorithms */
#define	  ALG_MD5		  CMD_ALGORITHM(OPTYPE_CLASS2_ALG, 0x40)
#define	  ALG_SHA1		  CMD_ALGORITHM(OPTYPE_CLASS2_ALG, 0x41)
#define	  ALG_SHA224		  CMD_ALGORITHM(OPTYPE_CLASS2_ALG, 0x42)
#define	  ALG_SHA256		  CMD_ALGORITHM(OPTYPE_CLASS2_ALG, 0x43)
#define	  ALG_SHA384		  CMD_ALGORITHM(OPTYPE_CLASS2_ALG, 0x44)
#define	  ALG_SHA512		  CMD_ALGORITHM(OPTYPE_CLASS2_ALG, 0x45)
#define	  ALG_CRC		  CMD_ALGORITHM(OPTYPE_CLASS2_ALG, 0x90)
#define	  ALG_SNOW3G_F9		  CMD_ALGORITHM(OPTYPE_CLASS2_ALG, 0xa0)
#define	  ALG_ZUC_AUTH		  CMD_ALGORITHM(OPTYPE_CLASS2_ALG, 0xc0)
/* AAI (Additional Algorithm Information) codes. */
#define	  AAI_S			  4
/* AES modes */
#define	  AAI_AES_CTR		  (0x00 << AAI_S)
#define	  AAI_AES_CBC		  (0x10 << AAI_S)
#define	  AAI_AES_ECB		  (0x20 << AAI_S)
#define	  AAI_AES_CFB		  (0x30 << AAI_S)
#define	  AAI_AES_OFB		  (0x40 << AAI_S)
#define	  AAI_AES_XTS		  (0x50 << AAI_S)
#define	  AAI_AES_CMAC		  (0x60 << AAI_S)
#define	  AAI_AES_XCBC_MAC	  (0x70 << AAI_S)
#define	  AAI_AES_CCM		  (0x80 << AAI_S)
#define	  AAI_AES_GCM		  (0x90 << AAI_S)
#define	  AAI_AES_DK		  (0x100 << AAI_S) /* Decrypt-key derive */
/* DES/3DES modes */
#define	  AAI_DES_CBC		  (0x10 << AAI_S)
#define	  AAI_DES_ECB		  (0x20 << AAI_S)
/* MDHA modes */
#define	  AAI_HASH		  (0x00 << AAI_S)
#define	  AAI_HMAC		  (0x01 << AAI_S)
#define	  AAI_HMAC_PRECOMP	  (0x04 << AAI_S) /* Precomputed IPAD/OPAD */
/* Algorithm State field (bits 2-3): what phase to run */
#define	  AS_S			  2
#define	  AS_UPDATE		  (0x0 << AS_S)
#define	  AS_INIT		  (0x1 << AS_S)
#define	  AS_FINAL		  (0x2 << AS_S)
#define	  AS_INIT_FINAL		  (0x3 << AS_S)
/* RNG-specific: State-Handle field. */
#define	  OP_RNG_SH_S		  4
#define	  OP_RNG_SH(n)		  ((n) << OP_RNG_SH_S)
/* Direction / ICV */
#define	  OP_ICV		  0x00000002
#define	  OP_ENC		  0x00000001

/* SEQ FIFO LOAD command bits. */
#define	  FIFOLD_CLASS_1	  0x02000000	/* CLASS = 01b (Class 1) */
#define	  FIFOLD_CLASS_2	  0x04000000	/* CLASS = 10b (Class 2) */
#define	  FIFOLD_CLASS_BOTH	  0x06000000	/* CLASS = 11b (snooping) */
#define	  FIFOLD_VLF		  0x01000000	/* Variable-length flag */
/*
 * Input data type: top 3 bits = type,
 * bottom 3 bits = LC2/LC1/FC1 flags.
 */
#define	  FIFOLD_TYPE_S		  16
#define	  FIFOLD_TYPE_MSG	  (0x10 << FIFOLD_TYPE_S)	/* 010_000 */
/* Class 1 output fed straight into Class 2, i.e. MAC over ciphertext. */
#define	  FIFOLD_TYPE_MSG_C1OUT	  (0x18 << FIFOLD_TYPE_S)	/* 011_000 */
#define	  FIFOLD_TYPE_IV	  (0x20 << FIFOLD_TYPE_S)	/* 100_000 */
#define	  FIFOLD_TYPE_AAD	  (0x30 << FIFOLD_TYPE_S)	/* 110_000 */
#define	  FIFOLD_TYPE_ICV	  (0x38 << FIFOLD_TYPE_S)	/* 111_000 */
#define	  FIFOLD_FC1		  (0x01 << FIFOLD_TYPE_S)	/* Flush class 1 */
#define	  FIFOLD_LC1		  (0x02 << FIFOLD_TYPE_S)	/* Last for Class 1 */
#define	  FIFOLD_LC2		  (0x04 << FIFOLD_TYPE_S)	/* Last for Class 2 */
/* Length moves to a 32-bit word after the command. */
#define	  FIFO_EXT		  0x00400000

/* SEQ FIFO STORE command bits. */
#define	  FIFOST_VLF		  0x01000000
#define	  FIFOST_TYPE_S		  16
#define	  FIFOST_TYPE_MSG_DATA	  (0x30 << FIFOST_TYPE_S)

#define	CMD_SIGNATURE		0x12
#define	CMD_JUMP		0x14
#define	CMD_MATH		0x15
#define	  MATH_FN_ADD		  (0x0 << 20)	/* SRC0 + SRC1 */
#define	  MATH_SRC0_SIL		  (0x8 << 16)	/* Sequence In Length */
#define	  MATH_SRC1_ZERO	  (0xF << 12)	/* Constant zero */
#define	  MATH_DEST_VSIL	  (0xA << 8)	/* Variable SIL */
#define	  MATH_DEST_VSOL	  (0xB << 8)	/* Variable SOL */
#define	  MATH_LEN_4		  0x4
/* J - Job Descriptor, S - Shared Descriptor */
#define	CMD_DESC_HEADER		0x16
#define	  HEADER_EXT		  0x04000000	/* Has Extension (J) */
#define	  HEADER_RSL		  0x02000000	/* Require SEQ LIODN (J) */
#define	  HEADER_DNR		  0x01000000	/* Do Not Run (J/S) */
#define	  HEADER_ONE		  0x00800000	/* Must be 1 (J/S) */
#define	  HEADER_START_INDEX(n)	  ((n) << 16)	/* Start Index (J/S) */
#define	  HEADER_SHR_DESC_L(n)	  ((n) << 16)	/* Shared Desc len (J) */
/* Bit 16 must be 0 */
#define	  HEADER_TDES_M		  0x00006000	/* Trusted Descriptor Mask (J) */
#define	  HEADER_TDES		  0x00004000	/* Trusted Descriptor (J) */
#define	  HEADER_TDES_CAND	  0x00006000	/* Candidate Trust Desc (J) */
#define	  HEADER_SHR		  0x00001000	/* Has Shared Descriptor (J) */
#define	  HEADER_REO		  0x00000800	/* Reverse Execution Order (J) */
#define	  HEADER_SHARE_M	  0x00000700	/* Share State (J/S) */
#define	  HEADER_SHARE_WAIT	  0x00000100	/* Wait to share (J/S) */
#define	  HEADER_SHARE_SERIAL	  0x00000200	/* Serialize (J/S) */
#define	  HEADER_SHARE_ALWAYS	  0x00000300	/* Always share (stateless) (J/S) */
#define	  HEADER_SHARE_DEFER	  0x00000400	/* Defer to shared desc (J) */
#define	  HEADER_DESCLEN_M	  0x0000007f	/* Descriptor length */
#define	  HEADER_DESCLEN_S	  0
#define	  HEADER_EXT_FTD	  0x00000100	/* Fake Trusted Descriptor */
#define	  HEADER_EXT_DSELVALID	  0x00000080	/* DECO_SELECT field valid */
#define	  HEADER_EXT_DSEL_M	  0x0000000f	/* DECO Select */
#define	CMD_SHARED_HEADER	0x17
#define	  HEADER_RIF		  0x02000000	/* Read Input Frame */
#define	  HEADER_CIF		  0x00002000	/* Clear Input FIFO */
#define	  HEADER_SC		  0x00001000	/* Save Context */
#define	  HEADER_PD		  0x00000800	/* Propagate DNR */
#define	CMD_MATHI		0x1d
#define	CMD_SEQ_IN_PTR		0x1e
#define	  SEQ_SGF		  0x01000000	/* Pointer is SGT (bit 7 NXP) */
#define	  SEQ_EXT		  0x00400000	/* 32-bit extended length (bit 9 NXP) */
#define	CMD_SEQ_OUT_PTR		0x1f

/* Shared descriptor container. */
struct sec_context {
	uint32_t		shd[SEC_MAX_SHDESC_WORDS];
};


/*
 * Session state: one shared descriptor per direction.  The shared
 * descriptor holds just KEY + OPERATION; the per-job JD adds LOAD-IV
 * and SEQ_IN_PTR / SEQ_OUT_PTR inline.
 */
#define	SEC_MAX_SPLIT_KEY	128	/* SHA-512 AES-ECB encrypted */

#define	SEC_CCM_AAD_MAX		0xfeff

struct sec_session {
	struct sec_softc	*sess_sc;
	struct sec_context	 ctx[2];	/* [0]=dec, [1]=enc */
	uint32_t		 sdlen[2];	/* words per direction */
	uint8_t			 digestlen;	/* HMAC output size (0 if none) */
	uint8_t			 skeylen;	/* HMAC split key size (0 if none) */
	uint8_t			 skey[SEC_MAX_SPLIT_KEY];
};

static device_probe_t		sec_probe;
static device_attach_t		sec_attach;
static device_detach_t		sec_detach;
static cryptodev_probesession_t	sec_probe_session;
static cryptodev_newsession_t	sec_new_session;
static cryptodev_freesession_t	sec_free_session;
static cryptodev_process_t	sec_process;

static void	sec_intr(void *);

/* Register-level bring-up.  Filled in from the SEC reference manual. */
static int	sec_reset(struct sec_softc *);
static int	sec_rng_init(struct sec_softc *);

static struct ofw_compat_data compats[] = {
	{ "fsl,sec-v5.2", 52 },
	{ "fsl,sec-v5.0", 50 },
	{ "fsl,sec-v4.0", 40 },
	{ NULL, 0 }
};

static device_method_t	sec_methods[] = {
	/* Device methods */
	DEVMETHOD(device_probe,			sec_probe),
	DEVMETHOD(device_attach,		sec_attach),
	DEVMETHOD(device_detach,		sec_detach),

	/* Cryptodev methods */
	DEVMETHOD(cryptodev_probesession,	sec_probe_session),
	DEVMETHOD(cryptodev_newsession,		sec_new_session),
	DEVMETHOD(cryptodev_freesession,	sec_free_session),
	DEVMETHOD(cryptodev_process,		sec_process),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(sec, sec_driver, sec_methods, sizeof(struct sec_softc));
DRIVER_MODULE(sec, simplebus, sec_driver, NULL, NULL);
MODULE_DEPEND(sec, crypto, 1, 1, 1);

MALLOC_DEFINE(M_SEC, "sec", "SEC driver");

static int
sec_probe(device_t dev)
{
	const struct ofw_compat_data *cd;

	cd = ofw_bus_search_compatible(dev, compats);
	if (cd->ocd_data == 0)
		return (ENXIO);

	device_set_descf(dev, "Freescale Security Engine v%d.%d",
	    (int)cd->ocd_data / 10, (int)cd->ocd_data % 10);

	return (BUS_PROBE_DEFAULT);
}

static int
sec_attach(device_t dev)
{
	struct sec_softc *sc = device_get_softc(dev);
	const struct ofw_compat_data *cd;

	sc->sc_dev = dev;
	sc->sc_cid = -1;

	cd = ofw_bus_search_compatible(dev, compats);
	sc->sc_version = cd->ocd_data;

	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->sc_rrid,
	    RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		device_printf(dev, "could not allocate register resource\n");
		goto fail;
	}

	/* TODO: Error IRQ handling. */
	sc->sc_irid = 0;
	sc->sc_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->sc_irid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_ires == NULL) {
		device_printf(dev, "could not allocate error interrupt\n");
		goto fail;
	}

	if (bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    SEC_MAX_SIZE, SEC_MAX_SEGMENTS, SEC_MAX_SIZE, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->sc_dmatag) != 0) {
		device_printf(dev, "could not create DMA tag\n");
		goto fail;
	}

	if (sec_reset(sc) != 0) {
		device_printf(dev, "SEC reset failed\n");
		goto fail;
	}
	if (sec_rng_init(sc) != 0) {
		device_printf(dev, "SEC RNG instantiation failed\n");
		goto fail;
	}
	if (sec_init_rings(sc) == 0) {
		device_printf(dev, "SEC job ring init failed\n");
		goto fail;
	}

	/*
	 * Clear any fault-address latch left over from the bootloader before
	 * enabling the error IRQ.  FADR, FAR_HI/LO, and FALR must all be read
	 * before they're all cleared, per the RM.
	 */
	(void)SEC_RD4(sc, SEC_FADR);
	(void)SEC_RD4(sc, SEC_FAR_HI);
	(void)SEC_RD4(sc, SEC_FAR_LO);
	(void)SEC_RD4(sc, SEC_FALR);

	if (bus_setup_intr(dev, sc->sc_ires, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, sec_intr, sc, &sc->sc_icookie) != 0) {
		device_printf(dev, "could not install error interrupt\n");
		goto fail;
	}

	sc->sc_cid = crypto_get_driverid(dev, sizeof(struct sec_session),
	    CRYPTOCAP_F_HARDWARE);
	if (sc->sc_cid < 0) {
		device_printf(dev, "could not get crypto driver id\n");
		goto fail;
	}

	return (0);

fail:
	sec_detach(dev);
	return (ENXIO);
}

static int
sec_detach(device_t dev)
{
	struct sec_softc *sc = device_get_softc(dev);
	u_int i;

	if (sc->sc_cid >= 0)
		crypto_unregister_all(sc->sc_cid);

	/* Silence the rings before halting them. */
	for (i = 0; i < sc->sc_njr; i++) {
		struct sec_jr *jr = &sc->sc_jr[i];

		if (jr->jr_icookie != NULL)
			bus_teardown_intr(dev, jr->jr_ires, jr->jr_icookie);
		if (jr->jr_ires != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, jr->jr_irid,
			    jr->jr_ires);
		sec_jr_teardown(sc, jr);
	}
	free(sc->sc_jr, M_SEC);

	if (sc->sc_dmatag != NULL)
		bus_dma_tag_destroy(sc->sc_dmatag);
	if (sc->sc_icookie != NULL)
		bus_teardown_intr(dev, sc->sc_ires, sc->sc_icookie);
	if (sc->sc_ires != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irid,
		    sc->sc_ires);
	if (sc->sc_rres != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rrid,
		    sc->sc_rres);

	return (0);
}

static const char *sec_ferr_str[] = {
	"OKAY", "reserved", "SLVERR", "DECERR",
};

static const char *sec_jsrc_str[] = {
	"JR0", "JR1", "JR2", "JR3", "RTIC", "QI", "rsvd6", "rsvd7",
};

static void
sec_intr(void *arg)
{
	struct sec_softc *sc = arg;
	uint32_t fadr, falr;
	uint64_t far;

	fadr = SEC_RD4(sc, SEC_FADR);
	if ((fadr & FADR_FERR_M) != 0) {
		/*
		 * All fault registers are latched by hardware until all are
		 * read, in any order.
		 */
		far = (uint64_t)SEC_RD4(sc, SEC_FAR_HI) << 32;
		far |= SEC_RD4(sc, SEC_FAR_LO);
		falr = SEC_RD4(sc, SEC_FALR);

		device_printf(sc->sc_dev,
		    "bus fault: FADR=%#x FAR=%#jx FALR=%#x "
		    "(%s, %s, src=%s, blkid=%#x, %s, size=%u)\n",
		    fadr, (uintmax_t)far, falr,
		    sec_ferr_str[(fadr & FADR_FERR_M) >> FADR_FERR_S],
		    (fadr & FADR_DTYP) ? "control" : "message",
		    sec_jsrc_str[(fadr & FADR_JSRC_M) >> FADR_JSRC_S],
		    (fadr & FADR_BLKID_M) >> FADR_BLKID_S,
		    (fadr & FADR_TYP) ? "write" : "read",
		    (unsigned)(((fadr & FADR_FSZ_EXT_M) >>
		    (FADR_FSZ_EXT_S - 7)) | (fadr & FADR_FSZ_M)));
	}

}

/*
 * Decode a SEC job termination status word.
 *
 * Bits 0-3 (MSB) are the "source" of the report; the remaining bits are
 * source-specific.  Zero means clean completion.
 *
 * Two cases we care to distinguish:
 *   - CCB (source 2), ERRID field bits 28-31
 *     value 0xA is "ICV check failed" -> EBADMSG.
 *   - DECO (source 4), Error Code bits 24-31
 *     values F0h/F1h/FFh are informational warnings (IPsec TTL,
 *     3GPP HFN, output-length rollover).  The job actually completed,
 *     so map those to success.
 *
 * Everything else is logged and reported as EIO.  Real per-code
 * decoding of DECO/QI errors can be layered on as we hit them.
 */
#define	SEC_STAT_SOURCE(s)	(((s) >> 28) & 0xf)
#define	  SEC_SRC_NONE		  0x0
#define	  SEC_SRC_CCB		  0x2
#define	  SEC_SRC_DECO		  0x4
#define	  SEC_SRC_QI		  0x5
#define	  SEC_SRC_JR		  0x6
#define	  SEC_CCB_ERR_ICV_FAIL	  0x0a
#define	  SEC_DECO_ERR_WARN_MIN	  0xf0

static int
sec_decode_status(struct sec_softc *sc, uint32_t status)
{
	uint32_t source;

	if (status == 0)
		return (0);

	source = SEC_STAT_SOURCE(status);

	switch (source) {
	case SEC_SRC_CCB:
		if ((status & 0xf) == SEC_CCB_ERR_ICV_FAIL)
			return (EBADMSG);
		break;
	case SEC_SRC_DECO:
		if ((status & 0xff) >= SEC_DECO_ERR_WARN_MIN)
			return (0);
		break;
	}

	device_printf(sc->sc_dev,
	    "job termination status %#x (source %#x)\n", status, source);
	return (EIO);
}

/*
 * Complete one job that SEC has finished processing.
 */
void
sec_complete_one(struct sec_softc *sc, uint64_t desc_pa, uint32_t status)
{
	struct sec_job *job;
	struct cryptop *crp;
	const struct crypto_session_params *csp;
	uint8_t expected[SEC_MAX_DIGEST];
	int dlen;

	job = (struct sec_job *)PHYS_TO_DMAP((vm_paddr_t)desc_pa);
	crp = job->crp;

	crp->crp_etype = sec_decode_status(sc, status);

	bus_dmamap_sync(sc->sc_dmatag, job->map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmatag, job->map);
	bus_dmamap_destroy(sc->sc_dmatag, job->map);

	if (crp->crp_etype == 0) {
		csp = crypto_get_params(crp->crp_session);
		dlen = csp->csp_auth_mlen != 0 ? csp->csp_auth_mlen :
		    job->sess->digestlen;
		switch (csp->csp_mode) {
		case CSP_MODE_DIGEST:
			if ((crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) != 0) {
				crypto_copydata(crp, crp->crp_digest_start,
				    dlen, expected);
				if (timingsafe_bcmp(job->digest, expected,
				    dlen) != 0)
					crp->crp_etype = EBADMSG;
			} else {
				crypto_copyback(crp, crp->crp_digest_start,
				    dlen, job->digest);
			}
			break;
		case CSP_MODE_ETA:
			if ((crp->crp_op & CRYPTO_OP_ENCRYPT) != 0) {
				crypto_copyback(crp, crp->crp_digest_start,
				    dlen, job->digest);
				break;
			}
			crypto_copydata(crp, crp->crp_digest_start, dlen,
			    expected);
			if (timingsafe_bcmp(job->digest, expected, dlen) != 0)
				crp->crp_etype = EBADMSG;
			break;
		case CSP_MODE_AEAD:
			if ((crp->crp_op & CRYPTO_OP_ENCRYPT) != 0)
				crypto_copyback(crp, crp->crp_digest_start,
				    dlen, job->digest);
			break;
		}
	}

	crypto_done(crp);
	free(job, M_SEC);
}

static bool
check_cipher(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
	case CRYPTO_AES_ICM:
		if (csp->csp_ivlen != AES_BLOCK_LEN)
			return (false);
		return (csp->csp_cipher_klen == 16 ||
		    csp->csp_cipher_klen == 24 ||
		    csp->csp_cipher_klen == 32);
	case CRYPTO_AES_XTS:
		if (csp->csp_ivlen != AES_XTS_IV_LEN)
			return (false);
		return (csp->csp_cipher_klen == 32 ||
		    csp->csp_cipher_klen == 64);
	default:
		return (false);
	}
}

static bool
check_aead(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_NIST_GCM_16:
		if (csp->csp_auth_mlen != 0 &&
		    csp->csp_auth_mlen != AES_GMAC_HASH_LEN)
			return (false);
		return (csp->csp_cipher_klen == 16 ||
		    csp->csp_cipher_klen == 24 ||
		    csp->csp_cipher_klen == 32);
	case CRYPTO_AES_CCM_16:
		return (csp->csp_cipher_klen == 16 ||
		    csp->csp_cipher_klen == 24 ||
		    csp->csp_cipher_klen == 32);
	default:
		return (false);
	}
}

/*
 * Map an opencrypto auth_alg to its SEC selector and digest length.
 * skeylen is zero for a plain hash, which is what tells the two apart.
 */
static bool
sec_hash_params(int auth_alg, uint32_t *alg, uint8_t *dlen, uint8_t *skeylen)
{

	switch (auth_alg) {
	case CRYPTO_SHA1_HMAC:
		*alg = ALG_SHA1;   *dlen = 20; *skeylen = 40;  return (true);
	case CRYPTO_SHA2_224_HMAC:
		*alg = ALG_SHA224; *dlen = 28; *skeylen = 64;  return (true);
	case CRYPTO_SHA2_256_HMAC:
		*alg = ALG_SHA256; *dlen = 32; *skeylen = 64;  return (true);
	case CRYPTO_SHA2_384_HMAC:
		*alg = ALG_SHA384; *dlen = 48; *skeylen = 128; return (true);
	case CRYPTO_SHA2_512_HMAC:
		*alg = ALG_SHA512; *dlen = 64; *skeylen = 128; return (true);
	case CRYPTO_SHA1:
		*alg = ALG_SHA1;   *dlen = 20; *skeylen = 0;   return (true);
	case CRYPTO_SHA2_224:
		*alg = ALG_SHA224; *dlen = 28; *skeylen = 0;   return (true);
	case CRYPTO_SHA2_256:
		*alg = ALG_SHA256; *dlen = 32; *skeylen = 0;   return (true);
	case CRYPTO_SHA2_384:
		*alg = ALG_SHA384; *dlen = 48; *skeylen = 0;   return (true);
	case CRYPTO_SHA2_512:
		*alg = ALG_SHA512; *dlen = 64; *skeylen = 0;   return (true);
	}
	return (false);
}

static bool
check_digest(const struct crypto_session_params *csp)
{
	uint32_t alg;
	uint8_t dlen, skeylen;

	/* GMAC is AESA rather than MDHA, so it has its own constraints. */
	if (csp->csp_auth_alg == CRYPTO_AES_NIST_GMAC) {
		if (csp->csp_ivlen != AES_GCM_IV_LEN)
			return (false);
		if (csp->csp_auth_mlen > AES_GMAC_HASH_LEN)
			return (false);
		return (csp->csp_auth_klen == 16 ||
		    csp->csp_auth_klen == 24 ||
		    csp->csp_auth_klen == 32);
	}

	if (!sec_hash_params(csp->csp_auth_alg, &alg, &dlen, &skeylen))
		return (false);
	/* Keyed variants require a key; plain hashes must not carry one. */
	if ((skeylen != 0) != (csp->csp_auth_klen != 0))
		return (false);
	return (csp->csp_auth_mlen <= dlen);
}

static bool
check_eta(const struct crypto_session_params *csp)
{

	/*
	 * ESN appends four bytes from crp_esn to the MAC input, which the
	 * descriptor has no way to splice in, so refuse rather than
	 * authenticate the wrong span.
	 */
	if ((csp->csp_flags & CSP_F_ESN) != 0)
		return (false);
	/*
	 * XTS carries its tweak in the class 1 context and pairs with no
	 * MAC; its shared descriptor is shaped differently.
	 */
	if (csp->csp_cipher_alg == CRYPTO_AES_XTS)
		return (false);
	/* The MAC half has to be keyed; a bare hash authenticates nothing. */
	if (csp->csp_auth_klen == 0)
		return (false);
	return (check_cipher(csp) && check_digest(csp));
}

/*
 * Software split-key generator: computes the HMAC ipad/opad hash-state
 * halves in software and packs them big-endian for SEC's Class 2 KEY
 * register.
 *
 * Runs the CPU through one SHA block per pad (two total).  Much cheaper than
 * the round trip through the job ring for setup.
 */
static void
sec_pack_state32(uint8_t *dst, const uint32_t *src, unsigned int nbytes)
{
	unsigned int i;

	for (i = 0; i < nbytes; i += 4)
		be32enc(dst + i, src[i / 4]);
}

static void
sec_pack_state64(uint8_t *dst, const uint64_t *src, unsigned int nbytes)
{
	unsigned int i;

	for (i = 0; i < nbytes; i += 8)
		be64enc(dst + i, src[i / 8]);
}

static void
sec_sw_gen_split_key(const struct crypto_session_params *csp,
    uint8_t *out, size_t out_len)
{
	union authctx ictx, octx;
	const struct auth_hash *axf;
	uint8_t half;

	axf = crypto_auth_hash(csp);
	hmac_init_ipad(axf, csp->csp_auth_key, csp->csp_auth_klen, &ictx);
	hmac_init_opad(axf, csp->csp_auth_key, csp->csp_auth_klen, &octx);

	KASSERT(out_len % 2 == 0, ("split key len must be even"));
	half = out_len / 2;

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
		sec_pack_state32(out,        ictx.sha1ctx.h.b32, half);
		sec_pack_state32(out + half, octx.sha1ctx.h.b32, half);
		break;
	case CRYPTO_SHA2_224_HMAC:
		sec_pack_state32(out,        ictx.sha224ctx.state, half);
		sec_pack_state32(out + half, octx.sha224ctx.state, half);
		break;
	case CRYPTO_SHA2_256_HMAC:
		sec_pack_state32(out,        ictx.sha256ctx.state, half);
		sec_pack_state32(out + half, octx.sha256ctx.state, half);
		break;
	case CRYPTO_SHA2_384_HMAC:
		sec_pack_state64(out,        ictx.sha384ctx.state, half);
		sec_pack_state64(out + half, octx.sha384ctx.state, half);
		break;
	case CRYPTO_SHA2_512_HMAC:
		sec_pack_state64(out,        ictx.sha512ctx.state, half);
		sec_pack_state64(out + half, octx.sha512ctx.state, half);
		break;
	}

	explicit_bzero(&ictx, sizeof(ictx));
	explicit_bzero(&octx, sizeof(octx));
}

/*
 * Descriptor builder.  Word 0 is the HEADER and is filled in last, since its
 * length field is only known once the body has been emitted.
 */
struct sec_desc_builder {
	uint32_t	*desc;
	unsigned int	 idx;	/* next word to write */
	unsigned int	 max;
	int		 err;
};

static inline void
sec_desc_init(struct sec_desc_builder *b, uint32_t *desc, unsigned int max)
{

	b->desc = desc;
	b->idx = 1;	/* reserve word 0 for the HEADER */
	b->max = max;
	b->err = 0;
}

static inline void
sec_desc_word(struct sec_desc_builder *b, uint32_t w)
{

	if (b->err != 0)
		return;
	if (b->idx >= b->max) {
		b->err = ENOSPC;
		return;
	}
	b->desc[b->idx++] = w;
}

/* Emit a KEY command with the key inline after it. */
static inline void
sec_desc_key_imm(struct sec_desc_builder *b, uint32_t class,
    const void *key, unsigned int keylen)
{
	unsigned int nwords = howmany(keylen, sizeof(uint32_t));

	if (b->err != 0)
		return;
	if (b->idx + 1 + nwords > b->max) {
		b->err = ENOSPC;
		return;
	}
	b->desc[b->idx++] = CMD_DESC(CMD_KEY) | class | KEY_IMM |
	    (keylen & KEY_LENGTH_M);
	memcpy(&b->desc[b->idx], key, keylen);
	b->idx += nwords;
}

static int
sec_desc_finalize_shared(struct sec_desc_builder *b, uint32_t flags,
    uint32_t *sdlenp)
{

	if (b->err != 0)
		return (b->err);
	if (b->idx > SEC_MAX_SHDESC_WORDS)
		return (ENOSPC);
	b->desc[0] = CMD_DESC(CMD_SHARED_HEADER) | HEADER_ONE |
	    (flags & (HEADER_SHARE_M | HEADER_SC)) |
	    (b->idx & HEADER_DESCLEN_M);
	*sdlenp = b->idx;
	return (0);
}

static int
sec_desc_finalize_job(struct sec_desc_builder *b, uint32_t word,
    uint32_t *dlenp)
{

	if (b->err != 0)
		return (b->err);
	if (b->idx > SEC_MAX_DESC_WORDS)
		return (ENOSPC);
	b->desc[0] = CMD_DESC(CMD_DESC_HEADER) | HEADER_ONE |
	    word | (b->idx & HEADER_DESCLEN_M);
	*dlenp = b->idx;
	return (0);
}


/*
 * Job descriptor builder conveniences.
 */

static inline void
sec_jd_ptr(struct sec_desc_builder *b, vm_paddr_t pa)
{
	sec_desc_word(b, (uint32_t)(pa >> 32));
	sec_desc_word(b, (uint32_t)pa);
}

/* Build a SEQ_IN/SEQ_OUT descriptor command. */
static inline void
sec_jd_seq(struct sec_desc_builder *b, bool inout, uint32_t flags,
    vm_paddr_t ptr, uint32_t len)
{
	sec_desc_word(b,
	    CMD_DESC(inout ? CMD_SEQ_OUT_PTR : CMD_SEQ_IN_PTR) | flags);
	sec_jd_ptr(b, ptr);
	sec_desc_word(b, len);
}

static inline void
sec_jd_load(struct sec_desc_builder *b, bool seq, uint32_t class,
    uint32_t flags, uint32_t dst, uint32_t off, uint32_t len, vm_paddr_t ptr)
{
	uint32_t cmd = seq ? CMD_SEQ_LOAD : CMD_LOAD;

	sec_desc_word(b, CMD_DESC(cmd) | class | flags | dst |
	    (off << LOAD_OFFSET_S) | (len & LOAD_LENGTH_M));
	if (!seq)
		sec_jd_ptr(b, ptr);
}

static inline void
sec_jd_store(struct sec_desc_builder *b, bool seq, uint32_t class, uint32_t src,
    uint32_t off, uint32_t len, vm_paddr_t ptr)
{
	uint32_t cmd = seq ? CMD_SEQ_STORE : CMD_STORE;

	sec_desc_word(b, CMD_DESC(cmd) | class | src |
	    (off << LOAD_OFFSET_S) | (len & LOAD_LENGTH_M));
	if (!seq)
		sec_jd_ptr(b, ptr);
}

static inline void
sec_jd_fifo(struct sec_desc_builder *b, uint32_t cmd, uint32_t len)
{

	if (len > 0xffff) {
		sec_desc_word(b, cmd | FIFO_EXT);
		sec_desc_word(b, len);
	} else {
		sec_desc_word(b, cmd | len);
	}
}

/*
 * AES-XTS Class 1 context layout (byte offsets into the CTX register).
 * The 16-byte tweak is split either side of the sector-size field.
 */
#define	SEC_XTS_CTX_TWEAK_LO	0x20
#define	SEC_XTS_CTX_SECTOR	0x28
#define	SEC_XTS_CTX_TWEAK_HI	0x30

/*
 * Sector size tells the hardware how often to re-derive the tweak.
 * opencrypto's XTS runs one continuous tweak over the whole request, so
 * this only needs to exceed any payload we accept; sec_jd_build_cipher
 * rejects requests that would cross the boundary.
 */
#define	SEC_XTS_SECTOR_SIZE	0x8000

/*
 * Build the CCM context block and formatted-AAD length prefix.
 *
 * The hardware wants B0 in context dwords 0-1 and the initial counter
 * CTR0 in dwords 2-3, with dwords 4-6 zeroed because AS is
 * INITIALIZE/FINALIZE.  Both blocks are laid out per RFC 3610: with a
 * nonce of n bytes, the length field occupies the trailing L = 15 - n
 * bytes and the flags byte carries L-1 plus, for B0, the encoded tag
 * size and an AAD-present flag.
 *
 * The AAD itself is prefixed with its length and then zero-padded to a
 * 16-byte boundary by the hardware, which pads AAD and IV FIFO loads
 * when the flush-class-1 bit is set.
 */
static int
sec_ccm_prep(struct sec_job *job, const struct crypto_session_params *csp)
{
	uint8_t *b0 = job->ccm_ctx;
	uint8_t *ctr0 = job->ccm_ctx + 16;
	uint32_t aadlen = job->crp->crp_aad_length;
	uint64_t paylen = job->crp->crp_payload_length;
	u_int i, lfield = 15 - csp->csp_ivlen;

	if (aadlen > SEC_CCM_AAD_MAX)
		return (EOPNOTSUPP);
	/*
	 * B0 carries the payload length in its trailing lfield bytes, so
	 * the nonce is what really caps the payload: a 13-byte nonce
	 * leaves two bytes and stops at 64 KB, while the usual 12-byte one
	 * leaves three and reaches 16 MB.
	 */
	if (lfield < sizeof(paylen) && paylen >= (uint64_t)1 << (8 * lfield))
		return (EOPNOTSUPP);

	memset(job->ccm_ctx, 0, sizeof(job->ccm_ctx));

	b0[0] = (aadlen > 0 ? 0x40 : 0x00) |
	    (((job->sess->digestlen - 2) / 2) << 3) | (lfield - 1);
	memcpy(b0 + 1, job->iv, csp->csp_ivlen);
	for (i = 0; i < lfield; i++)
		b0[15 - i] = (paylen >> (8 * i)) & 0xff;

	ctr0[0] = lfield - 1;
	memcpy(ctr0 + 1, job->iv, csp->csp_ivlen);

	be16enc(job->ccm_alen, aadlen);
	return (0);
}

/*
 * Bytes of IV the input sequence carries.  XTS is the odd one out:
 * opencrypto's IV is just the 8-byte block number, but the hardware
 * loads both halves of the 16-byte tweak from the sequence.
 */
static uint32_t
sec_cipher_ivlen(const struct crypto_session_params *csp)
{

	if (csp->csp_cipher_alg == CRYPTO_AES_XTS)
		return (AES_BLOCK_LEN);
	return (csp->csp_ivlen);
}

/*
 * Expand opencrypto's 8-byte XTS IV in place into the 16-byte tweak the
 * hardware expects.  The IV holds a block number in host order
 * (xform_aes_xts.c:aes_xts_reinit) and the tweak is that number's
 * little-endian encoding followed by zeroes.
 */
static void
sec_xts_tweak(uint8_t *iv)
{
	uint64_t blocknum;

	memcpy(&blocknum, iv, sizeof(blocknum));
	le64enc(iv, blocknum);
	memset(iv + sizeof(blocknum), 0, AES_BLOCK_LEN - sizeof(blocknum));
}

static uint32_t
sec_cipher_ctx_offset(uint32_t cipher_alg)
{
	switch (cipher_alg) {
	case CRYPTO_AES_ICM:
		return (16);
	};

	return (0);
}

/* Per-mode shared-descriptor builders. */
/*
 * Cipher shared descriptor has the following format:
 * [0] - Header
 * [1..klen] - KEY descriptor + key
 * [XTS:..5] -- XTS specific
 *   [0..2] - LOAD XTS context
 *   [3..4] - LOAD XTS tweak
 * [!XTS:1] -- Load IV into Context register
 * [] - Operation
 * [] - MATH - Move SIL register to VSIL for FIFO IN
 * [] - MATH - Move SOL register to VSOL for FIFO OUT
 * [] - FIFO LOAD
 * [] - FIFO STORE
 */
static int
sec_shd_build_cipher(struct sec_session *sess,
    const struct crypto_session_params *csp, int enc, uint32_t *sdlenp)
{
	struct sec_desc_builder b;
	uint32_t flags, op;
	uint32_t ctx_offset;

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		op = CMD_DESC(CMD_OPERATION) | ALG_AES |
		    AAI_AES_CBC | AS_INIT_FINAL;
		break;
	case CRYPTO_AES_ICM:
		op = CMD_DESC(CMD_OPERATION) | ALG_AES |
		    AAI_AES_CTR | AS_INIT_FINAL;
		break;
	case CRYPTO_AES_XTS:
		op = CMD_DESC(CMD_OPERATION) | ALG_AES |
		    AAI_AES_XTS | AS_INIT_FINAL;
		break;
	default:
		return (EOPNOTSUPP);
	}

	ctx_offset = sec_cipher_ctx_offset(csp->csp_cipher_alg);

	if (enc)
		op |= OP_ENC;

	sec_desc_init(&b, sess->ctx[enc].shd, SEC_MAX_SHDESC_WORDS);

	if (csp->csp_cipher_klen > 0)
		sec_desc_key_imm(&b, KEY_CLASS_1, csp->csp_cipher_key,
		    csp->csp_cipher_klen);

	if (csp->csp_cipher_alg == CRYPTO_AES_XTS) {
		sec_jd_load(&b, false, LOAD_CLASS_1, LOAD_IMM, LOAD_CTX,
		    SEC_XTS_CTX_SECTOR, 8, SEC_XTS_SECTOR_SIZE);

		sec_jd_load(&b, true, LOAD_CLASS_1, 0, LOAD_CTX,
		    SEC_XTS_CTX_TWEAK_LO, 8, 0);
		sec_jd_load(&b, true, LOAD_CLASS_1, 0, LOAD_CTX,
		    SEC_XTS_CTX_TWEAK_HI, 8, 0);
	} else {
		sec_jd_load(&b, true, LOAD_CLASS_1, 0, LOAD_CTX, ctx_offset,
		    csp->csp_ivlen, 0);
	}

	sec_desc_word(&b, op);

	/*
	 * Copy SIL into VSIL and VSOL so the following VLF-flagged FIFO
	 * commands know how many bytes to move.  VLF reads the VS*L
	 * registers, so we need to get the values from the SEQ registers
	 * the SEQ IN/OUT PTR descriptors populate.
	 */
	sec_desc_word(&b, CMD_DESC(CMD_MATH) | MATH_FN_ADD | MATH_SRC0_SIL |
	    MATH_SRC1_ZERO | MATH_DEST_VSIL | MATH_LEN_4);
	sec_desc_word(&b, CMD_DESC(CMD_MATH) | MATH_FN_ADD | MATH_SRC0_SIL |
	    MATH_SRC1_ZERO | MATH_DEST_VSOL | MATH_LEN_4);

	sec_desc_word(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) | FIFOLD_CLASS_1 |
	    FIFOLD_VLF | FIFOLD_TYPE_MSG | FIFOLD_LC1);

	sec_desc_word(&b, CMD_DESC(CMD_SEQ_FIFO_STORE) | FIFOST_VLF |
	    FIFOST_TYPE_MSG_DATA);

	/* XTS keeps its tweak in the context, so the CCB has to save it. */
	flags = HEADER_SHARE_SERIAL;
	if (csp->csp_cipher_alg == CRYPTO_AES_XTS)
		flags |= HEADER_SC;

	return (sec_desc_finalize_shared(&b, flags, sdlenp));
}

/*
 * Digest shared descriptor.  The split key is computed in software at
 * session setup, so MDHA is told it is precomputed and skips the
 * ipad/opad expansion.
 */
static int
sec_shd_build_digest(struct sec_session *sess,
    const struct crypto_session_params *csp, int enc, uint32_t *sdlenp)
{
	struct sec_desc_builder b;
	uint32_t alg;
	uint8_t dlen, skeylen;

	/*
	 * GMAC runs on AESA, not MDHA: the shared descriptor is just the
	 * class 1 key, and the JD drives it as GCM with no message.
	 */
	if (csp->csp_auth_alg == CRYPTO_AES_NIST_GMAC) {
		sess->digestlen = csp->csp_auth_mlen != 0 ?
		    csp->csp_auth_mlen : AES_GMAC_HASH_LEN;
		sec_desc_init(&b, sess->ctx[enc].shd, SEC_MAX_SHDESC_WORDS);
		sec_desc_key_imm(&b, KEY_CLASS_1, csp->csp_auth_key,
		    csp->csp_auth_klen);
		return (sec_desc_finalize_shared(&b, HEADER_SHARE_SERIAL,
		    sdlenp));
	}

	if (!sec_hash_params(csp->csp_auth_alg, &alg, &dlen, &skeylen))
		return (EOPNOTSUPP);
	sess->digestlen = dlen;

	sec_desc_init(&b, sess->ctx[enc].shd, SEC_MAX_SHDESC_WORDS);

	/*
	 * A plain hash takes no key at all; the keyed variants load the
	 * precomputed ipad || opad blob as an MDHA split key, which is
	 * what AAI_HMAC_PRECOMP tells MDHA to expect.
	 */
	if (skeylen != 0) {
		unsigned int nwords = howmany(skeylen, 4);

		b.desc[b.idx++] = CMD_DESC(CMD_KEY) | KEY_CLASS_2 |
		    KEY_KDEST_MDHA_SPLIT | KEY_IMM |
		    (skeylen & KEY_LENGTH_M);
		memcpy(&b.desc[b.idx], sess->skey, skeylen);
		if (skeylen % 4 != 0)
			memset((uint8_t *)&b.desc[b.idx] + skeylen, 0,
			    nwords * 4 - skeylen);
		b.idx += nwords;
	}

	sec_desc_word(&b, CMD_DESC(CMD_OPERATION) | alg |
	    (skeylen != 0 ? AAI_HMAC_PRECOMP : AAI_HASH) | AS_INIT_FINAL);

	/* VLF FIFO_LOAD needs VSIL, which SEQ_IN_PTR doesn't populate. */
	sec_desc_word(&b, CMD_DESC(CMD_MATH) | MATH_FN_ADD | MATH_SRC0_SIL |
	    MATH_SRC1_ZERO | MATH_DEST_VSIL | MATH_LEN_4);

	sec_desc_word(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) | FIFOLD_CLASS_2 |
	    FIFOLD_VLF | FIFOLD_TYPE_MSG | FIFOLD_LC2);

	/*
	 * Drain the completed hash from the Class 2 CCB Context register.
	 * SEQ_STORE with class 2 + SRC=CTX (0x20) blocks until MDHA is
	 * done.
	 */
	sec_jd_store(&b, true, LOAD_CLASS_2, LOAD_CTX, 0, dlen, 0);

	return (sec_desc_finalize_shared(&b, HEADER_SHARE_SERIAL, sdlenp));
}

/*
 * AEAD shared descriptor.  AAD and payload lengths vary per job, so
 * everything but the key lives in the JD.  Execution order is not
 * reversed here: the key has to be loaded before the JD drives data.
 *
 * When ICV is set the ENC bit must be clear, which is the only
 * difference between the two direction slots.
 */
static int
sec_shd_build_aead(struct sec_session *sess,
    const struct crypto_session_params *csp, int enc, uint32_t *sdlenp)
{
	struct sec_desc_builder b;

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_NIST_GCM_16:
	case CRYPTO_AES_CCM_16:
		break;
	default:
		return (EOPNOTSUPP);
	}
	(void)enc;

	/* Cache tag length once (both directions share). */
	sess->digestlen = csp->csp_auth_mlen != 0 ? csp->csp_auth_mlen : 16;

	/*
	 * SHD holds just the AES key.
	 */
	sec_desc_init(&b, sess->ctx[enc].shd, SEC_MAX_SHDESC_WORDS);
	if (csp->csp_cipher_klen > 0)
		sec_desc_key_imm(&b, KEY_CLASS_1, csp->csp_cipher_key,
		    csp->csp_cipher_klen);
	return (sec_desc_finalize_shared(&b, HEADER_SHARE_SERIAL, sdlenp));
}

/*
 * Shared descriptor for encrypt-then-auth: both keys and both mode
 * registers, nothing else.
 */
static int
sec_shd_build_eta(struct sec_session *sess,
    const struct crypto_session_params *csp, int enc, uint32_t *sdlenp)
{
	struct sec_desc_builder b;
	uint32_t alg, op;
	uint8_t dlen, skeylen;

	if (!sec_hash_params(csp->csp_auth_alg, &alg, &dlen, &skeylen))
		return (EOPNOTSUPP);
	sess->digestlen = csp->csp_auth_mlen != 0 ? csp->csp_auth_mlen : dlen;

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		op = CMD_DESC(CMD_OPERATION) | ALG_AES | AAI_AES_CBC |
		    AS_INIT_FINAL;
		break;
	case CRYPTO_AES_ICM:
		op = CMD_DESC(CMD_OPERATION) | ALG_AES | AAI_AES_CTR |
		    AS_INIT_FINAL;
		break;
	default:
		return (EOPNOTSUPP);
	}
	if (enc)
		op |= OP_ENC;

	sec_desc_init(&b, sess->ctx[enc].shd, SEC_MAX_SHDESC_WORDS);

	sec_desc_key_imm(&b, KEY_CLASS_1, csp->csp_cipher_key,
	    csp->csp_cipher_klen);

	/* Class 2 takes the precomputed ipad/opad blob, as for plain HMAC. */
	{
		unsigned int nwords = howmany(skeylen, 4);

		b.desc[b.idx++] = CMD_DESC(CMD_KEY) | KEY_CLASS_2 |
		    KEY_KDEST_MDHA_SPLIT | KEY_IMM | (skeylen & KEY_LENGTH_M);
		memcpy(&b.desc[b.idx], sess->skey, skeylen);
		if (skeylen % 4 != 0)
			memset((uint8_t *)&b.desc[b.idx] + skeylen, 0,
			    nwords * 4 - skeylen);
		b.idx += nwords;
	}

	sec_desc_word(&b, CMD_DESC(CMD_OPERATION) | alg | AAI_HMAC_PRECOMP |
	    AS_INIT_FINAL);
	sec_desc_word(&b, op);

	return (sec_desc_finalize_shared(&b, HEADER_SHARE_SERIAL, sdlenp));
}

static int
sec_probe_session(device_t dev, const struct crypto_session_params *csp)
{

	switch (csp->csp_mode) {
	case CSP_MODE_CIPHER:
		if (!check_cipher(csp))
			return (EINVAL);
		break;
	case CSP_MODE_DIGEST:
		if (!check_digest(csp))
			return (EINVAL);
		break;
	case CSP_MODE_AEAD:
		if (!check_aead(csp))
			return (EINVAL);
		break;
	case CSP_MODE_ETA:
		if (!check_eta(csp))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}
	return (CRYPTODEV_PROBE_HARDWARE);
}

static int
sec_new_session(device_t dev, crypto_session_t session,
    const struct crypto_session_params *csp)
{
	struct sec_softc *sc = device_get_softc(dev);
	struct sec_session *sess;
	uint32_t sdlen;
	int enc, error;

	sess = crypto_get_driver_session(session);
	sess->sess_sc = sc;

	if ((csp->csp_mode == CSP_MODE_DIGEST ||
	    csp->csp_mode == CSP_MODE_ETA) && csp->csp_auth_klen > 0 &&
	    csp->csp_auth_alg != CRYPTO_AES_NIST_GMAC) {
		uint32_t alg;
		uint8_t dlen, skeylen;

		if (!sec_hash_params(csp->csp_auth_alg, &alg, &dlen, &skeylen))
			return (EOPNOTSUPP);
		(void)alg;
		sec_sw_gen_split_key(csp, sess->skey, skeylen);
		sess->skeylen = skeylen;
	}

	for (enc = 0; enc <= 1; enc++) {
		switch (csp->csp_mode) {
		case CSP_MODE_CIPHER:
			error = sec_shd_build_cipher(sess, csp, enc, &sdlen);
			break;
		case CSP_MODE_DIGEST:
			error = sec_shd_build_digest(sess, csp, enc, &sdlen);
			break;
		case CSP_MODE_AEAD:
			error = sec_shd_build_aead(sess, csp, enc, &sdlen);
			break;
		case CSP_MODE_ETA:
			error = sec_shd_build_eta(sess, csp, enc, &sdlen);
			break;
		default:
			return (EINVAL);
		}
		if (error != 0)
			return (error);
		sess->sdlen[enc] = sdlen;
	}
	return (0);
}

static void
sec_free_session(device_t dev, crypto_session_t session)
{
	/* Nothing to do here. */
}

static void
sec_load_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct sec_job *job = arg;

	if (error != 0) {
		job->nsegs = 0;
		return;
	}
	KASSERT(nsegs <= SEC_MAX_SEGMENTS,
	    ("SEC job segment overflow: %d > %d", nsegs, SEC_MAX_SEGMENTS));
	memcpy(job->segs, segs, nsegs * sizeof(segs[0]));
	job->nsegs = nsegs;
}

/*
 * Append the segments covering [start, start + len) of the mapped buffer.
 * Returns the next free index, or -1 if the table would overflow or the
 * range runs past the mapping.
 */
static int
dpaa_sgte_append(struct sec_job *job, struct dpaa_sgte *sgt, int i, int max,
    uint32_t start, uint32_t len)
{
	int s;

	for (s = 0; s < job->nsegs && len > 0; s++) {
		bus_addr_t addr = job->segs[s].ds_addr;
		bus_size_t seglen = job->segs[s].ds_len;
		uint32_t take;

		if (start >= seglen) {
			start -= seglen;
			continue;
		}
		addr += start;
		seglen -= start;
		start = 0;

		take = seglen > len ? len : seglen;
		len -= take;

		if (i >= max)
			return (-1);
		sgt[i].addr = addr;
		sgt[i].extension = 0;
		sgt[i].final = 0;
		sgt[i].length = take;
		sgt[i].bpid = 0;
		sgt[i].offset = 0;
		i++;
	}
	if (len != 0)
		return (-1);
	return (i);
}

/*
 * Populate the SGTs from the DMA-loaded segment list.  Entry order is
 * what the descriptor's SEQ commands consume, so it is fixed per mode:
 *   CIPHER:
 *     in_sgt[0]        = job->iv
 *     in_sgt[1..n]     = payload segments
 *     out_sgt[0..n-1]  = payload segments (in-place)
 *   DIGEST (HMAC, no IV):
 *     in_sgt[0..n-1]   = payload segments
 *     out_sgt unused (JD points SEQ_OUT_PTR directly at job->digest).
 *   AEAD and ETA:
 *     in_sgt[0]        = job->iv, or job->ccm_ctx for CCM
 *     in_sgt[1..A]     = AAD (crp_aad, or crp_buf at crp_aad_start)
 *     in_sgt[A+1..N]   = payload segments
 *     in_sgt[N+1..]    = (decrypt only) received tag from crp_buf
 *     out_sgt[0..]     = payload segments (in-place); the tag goes to
 *                        job->digest via a separate STORE.
 * The final SGT entry in each populated table gets F=1.
 */
static int
sec_job_build_sgts(struct sec_job *job, const struct crypto_session_params *csp)
{
	struct cryptop *crp = job->crp;
	uint32_t skip = crp->crp_payload_start;
	uint32_t left = crp->crp_payload_length;
	int i, out_i;
	int iv_slot = csp->csp_ivlen > 0 ? 1 : 0;

	if (csp->csp_mode == CSP_MODE_AEAD ||
	    csp->csp_mode == CSP_MODE_ETA) {
		const int inmax = 1 + SEC_MAX_SEGMENTS;
		bool encrypt = (crp->crp_op & CRYPTO_OP_ENCRYPT) != 0;
		bool ccm = csp->csp_mode == CSP_MODE_AEAD &&
		    csp->csp_cipher_alg == CRYPTO_AES_CCM_16;
		int in_i = 0, npay, pay_i;

		/* IV, or for CCM the B0 || CTR0 context block. */
		if (ccm) {
			job->in_sgt[in_i].addr =
			    pmap_kextract((vm_offset_t)job->ccm_ctx);
			job->in_sgt[in_i].length = SEC_CCM_CTX_LEN;
		} else {
			job->in_sgt[in_i].addr =
			    pmap_kextract((vm_offset_t)job->iv);
			job->in_sgt[in_i].length = csp->csp_ivlen;
		}
		job->in_sgt[in_i].extension = 0;
		job->in_sgt[in_i].final = 0;
		job->in_sgt[in_i].bpid = 0;
		job->in_sgt[in_i].offset = 0;
		in_i++;

		if (crp->crp_aad_length > 0) {
			/* CCM feeds the AAD length ahead of the AAD. */
			if (ccm) {
				job->in_sgt[in_i].addr = pmap_kextract(
				    (vm_offset_t)job->ccm_alen);
				job->in_sgt[in_i].extension = 0;
				job->in_sgt[in_i].final = 0;
				job->in_sgt[in_i].length =
				    sizeof(job->ccm_alen);
				job->in_sgt[in_i].bpid = 0;
				job->in_sgt[in_i].offset = 0;
				in_i++;
			}
			if (crp->crp_aad != NULL) {
				/*
				 * A dedicated AAD buffer is not part of the
				 * crp mapping; it is small enough that one
				 * entry always covers it.
				 */
				job->in_sgt[in_i].addr = pmap_kextract(
				    (vm_offset_t)crp->crp_aad);
				job->in_sgt[in_i].extension = 0;
				job->in_sgt[in_i].final = 0;
				job->in_sgt[in_i].length = crp->crp_aad_length;
				job->in_sgt[in_i].bpid = 0;
				job->in_sgt[in_i].offset = 0;
				in_i++;
			} else {
				in_i = dpaa_sgte_append(job, job->in_sgt, in_i,
				    inmax, crp->crp_aad_start,
				    crp->crp_aad_length);
				if (in_i < 0)
					return (E2BIG);
			}
		}

		pay_i = in_i;
		in_i = dpaa_sgte_append(job, job->in_sgt, in_i, inmax, skip,
		    left);
		if (in_i < 0)
			return (E2BIG);
		npay = in_i - pay_i;
		if (npay == 0)
			return (EINVAL);

		/*
		 * AEAD decrypt hands the received tag to the CHA for its
		 * own compare; ETA drains the MAC to job->digest instead
		 * and compares in software, so it needs no entry here.
		 */
		if (!encrypt && csp->csp_mode == CSP_MODE_AEAD) {
			in_i = dpaa_sgte_append(job, job->in_sgt, in_i, inmax,
			    crp->crp_digest_start, job->sess->digestlen);
			if (in_i < 0)
				return (E2BIG);
		}
		job->in_sgt[in_i - 1].final = 1;

		/* Output mirrors the payload segments, in place. */
		memcpy(job->out_sgt, &job->in_sgt[pay_i],
		    npay * sizeof(job->out_sgt[0]));
		job->out_sgt[npay - 1].final = 1;
		return (0);
	}

	if (iv_slot) {
		job->in_sgt[0].addr = pmap_kextract((vm_offset_t)job->iv);
		job->in_sgt[0].extension = 0;
		job->in_sgt[0].final = 0;
		job->in_sgt[0].length = sec_cipher_ivlen(csp);
		job->in_sgt[0].bpid = 0;
		job->in_sgt[0].offset = 0;
	}

	out_i = 0;
	for (i = 0; i < job->nsegs && left > 0; i++) {
		bus_addr_t addr = job->segs[i].ds_addr;
		bus_size_t len = job->segs[i].ds_len;
		uint32_t take;

		if (skip >= len) {
			skip -= len;
			continue;
		}
		addr += skip;
		len -= skip;
		skip = 0;

		take = (len > left) ? left : len;
		left -= take;

		if (out_i >= SEC_MAX_SEGMENTS)
			return (E2BIG);

		job->in_sgt[iv_slot + out_i].addr = addr;
		job->in_sgt[iv_slot + out_i].extension = 0;
		job->in_sgt[iv_slot + out_i].final = 0;
		job->in_sgt[iv_slot + out_i].length = take;
		job->in_sgt[iv_slot + out_i].bpid = 0;
		job->in_sgt[iv_slot + out_i].offset = 0;

		if (iv_slot)
			job->out_sgt[out_i] = job->in_sgt[iv_slot + out_i];
		out_i++;
	}
	if (left != 0)
		return (EINVAL);
	if (out_i == 0)
		return (EINVAL);

	job->in_sgt[iv_slot + out_i - 1].final = 1;
	if (iv_slot)
		job->out_sgt[out_i - 1].final = 1;
	return (0);
}

/*
 * JD for the cipher modes.  The shared descriptor runs the pipeline, so
 * the JD only points at the SGTs.
 *
 * Both sequences use SGF and EXT unconditionally.  Always using a table
 * keeps the builder from caring how many segments there are, and the
 * 16-bit length in the command word is too small for the payloads geli
 * and kTLS hand down.
 */
static int
sec_jd_build_cipher(struct sec_job *job,
    const struct crypto_session_params *csp)
{
	struct sec_desc_builder b;
	struct sec_session *sess = job->sess;
	int enc = CRYPTO_OP_IS_ENCRYPT(job->crp->crp_op);
	uint32_t sdlen = sess->sdlen[enc];
	uint32_t desclen;
	uint32_t in_len = sec_cipher_ivlen(csp) + job->crp->crp_payload_length;
	vm_paddr_t shd_pa, in_sgt_pa, out_sgt_pa;

	/*
	 * The hardware restarts the tweak every SEC_XTS_SECTOR_SIZE bytes;
	 * opencrypto expects one continuous tweak, so anything that would
	 * cross the boundary has to go back to software.
	 */
	if (csp->csp_cipher_alg == CRYPTO_AES_XTS &&
	    job->crp->crp_payload_length > SEC_XTS_SECTOR_SIZE)
		return (EOPNOTSUPP);

	shd_pa = pmap_kextract((vm_offset_t)sess->ctx[enc].shd);
	in_sgt_pa = pmap_kextract((vm_offset_t)job->in_sgt);
	out_sgt_pa = pmap_kextract((vm_offset_t)job->out_sgt);

	sec_desc_init(&b, job->jd, SEC_MAX_DESC_WORDS);
	sec_jd_ptr(&b, shd_pa);

	sec_jd_seq(&b, true, SEQ_SGF | SEQ_EXT, out_sgt_pa,
	    job->crp->crp_payload_length);

	sec_jd_seq(&b, false, SEQ_SGF | SEQ_EXT, in_sgt_pa, in_len);

	/*
	 * HEADER_REO (Reverse Execution Order) makes SEC run the JD
	 * commands FIRST (SEQ_OUT_PTR / SEQ_IN_PTR set up the input and
	 * output sequences), then fall into the shared descriptor.  The
	 * shared descriptor's SEQ_LOAD / SEQ_FIFO_LOAD / SEQ_FIFO_STORE
	 * commands depend on those sequences being programmed.  Without
	 * this bit the shared desc runs first and SEQ_LOAD hits an
	 * uninitialized input sequence, and DECO reports an invalid
	 * sequence command (error 0x10).
	 */
	return (sec_desc_finalize_job(&b, HEADER_SHR |
	    HEADER_REO | HEADER_SHR_DESC_L(sdlen) | HEADER_SHARE_DEFER,
	    &desclen));
}

/*
 * JD for the digest modes.  There is no IV to prepend and the output is
 * a small fixed buffer, so SEQ_OUT_PTR addresses it directly.
 */
static int
sec_jd_build_digest(struct sec_job *job)
{
	struct sec_desc_builder b;
	struct sec_session *sess = job->sess;
	uint32_t sdlen = sess->sdlen[0];
	uint32_t desclen;
	vm_paddr_t shd_pa, in_sgt_pa, digest_pa;

	shd_pa = pmap_kextract((vm_offset_t)sess->ctx[0].shd);
	in_sgt_pa = pmap_kextract((vm_offset_t)job->in_sgt);
	digest_pa = pmap_kextract((vm_offset_t)job->digest);

	sec_desc_init(&b, job->jd, SEC_MAX_DESC_WORDS);
	sec_jd_ptr(&b, shd_pa);

	sec_jd_seq(&b, true, SEQ_EXT, digest_pa, sess->digestlen);
	sec_jd_seq(&b, false, SEQ_SGF | SEQ_EXT, in_sgt_pa,
	    job->crp->crp_payload_length);

	return (sec_desc_finalize_job(&b,
	    HEADER_SHR | HEADER_REO | HEADER_SHR_DESC_L(sdlen) |
	    HEADER_SHARE_DEFER, &desclen));
}

/*
 * JD for AEAD (AES-GCM).
 *
 * The shared descriptor holds only the class 1 key and runs first, so
 * the job descriptor sets up both sequences and drives all of the data.
 * The data size counts the IV and AAD rounded up to 16 bytes even though
 * the FIFO loads supply them unpadded; SEC pads them internally.
 *
 * [0] - Header
 * [1..2] - Shared descriptor pointer
 * [3..6] - SEQ OUT PTR - ciphertext only, the tag leaves via STORE
 * [7..10] - SEQ IN PTR - iv + aad + payload, and the tag when decrypting
 * [11] - Operation
 * [12] - LOAD Class 1 Data Size, which starts processing
 * [13] - FIFO LOAD IV
 * [14] - FIFO LOAD AAD
 * [15] - FIFO STORE ciphertext
 * [16] - FIFO LOAD message
 * [encrypt:17..19] - STORE the computed tag to job->digest
 * [decrypt:17] - FIFO LOAD received ICV
 */
static int
sec_jd_build_aead(struct sec_job *job)
{
	struct sec_desc_builder b;
	struct sec_session *sess = job->sess;
	const struct crypto_session_params *csp;
	uint32_t sdlen = sess->sdlen[0];
	uint32_t desclen;
	vm_paddr_t shd_pa, in_sgt_pa, out_sgt_pa;
	uint32_t ivlen, aadlen, paylen, taglen;
	uint32_t padded_iv, padded_aad, dsr_val;
	uint32_t in_len, out_len;
	vm_paddr_t digest_pa;
	int enc;

	csp = crypto_get_params(job->crp->crp_session);
	enc = (job->crp->crp_op & CRYPTO_OP_ENCRYPT) != 0;
	ivlen = csp->csp_ivlen;
	aadlen = job->crp->crp_aad_length;
	paylen = job->crp->crp_payload_length;
	taglen = sess->digestlen;

	padded_iv = roundup(ivlen, 16);
	padded_aad = roundup(aadlen, 16);
	dsr_val = padded_iv + padded_aad + paylen;
	in_len = ivlen + aadlen + paylen + (enc ? 0 : taglen);
	/* Output sequence is ciphertext only; tag goes via direct STORE. */
	out_len = paylen;
	digest_pa = pmap_kextract((vm_offset_t)job->digest);

	shd_pa = pmap_kextract((vm_offset_t)sess->ctx[enc].shd);
	in_sgt_pa = pmap_kextract((vm_offset_t)job->in_sgt);
	out_sgt_pa = pmap_kextract((vm_offset_t)job->out_sgt);

	sec_desc_init(&b, job->jd, SEC_MAX_DESC_WORDS);
	sec_jd_ptr(&b, shd_pa);

	sec_jd_seq(&b, true, SEQ_SGF | SEQ_EXT, out_sgt_pa, out_len);
	sec_jd_seq(&b, false, SEQ_SGF | SEQ_EXT, in_sgt_pa, in_len);

	/*
	 * Writing the data size starts processing, so OPERATION has to arm
	 * the CHA in GCM mode before the DSR load below.
	 */
	sec_desc_word(&b, CMD_DESC(CMD_OPERATION) | ALG_AES | AAI_AES_GCM |
	    AS_INIT_FINAL | (enc ? OP_ENC : OP_ICV));

	sec_jd_load(&b, false, LOAD_CLASS_1, LOAD_IMM, LOAD_DSR, 0, 8,
	    (uint64_t)dsr_val << 32);

	/* IV: FC1 so SEC pads to 16 without ending class 1 input. */
	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) | FIFOLD_CLASS_1 |
	    FIFOLD_TYPE_IV | FIFOLD_FC1, ivlen);
	/*
	 * Always emit an AAD FIFO_LOAD (even with length 0) so SEC gets
	 * an explicit "AAD phase done" signal via FC1.
	 */
	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) | FIFOLD_CLASS_1 |
	    FIFOLD_TYPE_AAD | FIFOLD_FC1, aadlen);
	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_STORE) |
	    FIFOST_TYPE_MSG_DATA, paylen);

	/* MSG: LC1 for encrypt (last class-1 input), FC1 for decrypt. */
	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) | FIFOLD_CLASS_1 |
	    FIFOLD_TYPE_MSG | (enc ? FIFOLD_LC1 : FIFOLD_FC1), paylen);
	if (enc)
		sec_jd_store(&b, false, LOAD_CLASS_1, LOAD_CTX, 0, taglen,
		    digest_pa);
	else
		sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) |
		    FIFOLD_CLASS_1 | FIFOLD_TYPE_ICV | FIFOLD_LC1, taglen);

	/* SHR=1, NO REO, so the shd (KEY only) runs first, then the JD. */
	return (sec_desc_finalize_job(&b, HEADER_SHR |
	    HEADER_SHR_DESC_L(sdlen) | HEADER_SHARE_SERIAL, &desclen));
}

/*
 * JD for AES-CCM.
 */
static int
sec_jd_build_ccm(struct sec_job *job)
{
	struct sec_desc_builder b;
	struct sec_session *sess = job->sess;
	uint32_t sdlen = sess->sdlen[0];
	uint32_t desclen, aadlen, paylen, taglen, in_len;
	vm_paddr_t shd_pa, in_sgt_pa, out_sgt_pa, digest_pa;
	int enc;

	enc = (job->crp->crp_op & CRYPTO_OP_ENCRYPT) != 0;
	aadlen = job->crp->crp_aad_length;
	paylen = job->crp->crp_payload_length;
	taglen = sess->digestlen;

	in_len = SEC_CCM_CTX_LEN + paylen + (enc ? 0 : taglen);
	if (aadlen > 0)
		in_len += sizeof(job->ccm_alen) + aadlen;

	shd_pa = pmap_kextract((vm_offset_t)sess->ctx[enc].shd);
	in_sgt_pa = pmap_kextract((vm_offset_t)job->in_sgt);
	out_sgt_pa = pmap_kextract((vm_offset_t)job->out_sgt);
	digest_pa = pmap_kextract((vm_offset_t)job->digest);

	sec_desc_init(&b, job->jd, SEC_MAX_DESC_WORDS);
	sec_jd_ptr(&b, shd_pa);

	sec_jd_seq(&b, true, SEQ_SGF | SEQ_EXT, out_sgt_pa, paylen);
	sec_jd_seq(&b, false, SEQ_SGF | SEQ_EXT, in_sgt_pa, in_len);

	/* B0 || CTR0 || zeroed result dwords, from the head of the input. */
	sec_desc_word(&b,  CMD_DESC(CMD_SEQ_LOAD) | LOAD_CLASS_1 | LOAD_CTX |
	    (SEC_CCM_CTX_LEN & LOAD_LENGTH_M));

	sec_desc_word(&b, CMD_DESC(CMD_OPERATION) | ALG_AES | AAI_AES_CCM |
	    AS_INIT_FINAL | (enc ? OP_ENC : OP_ICV));

	/* Writing the data size starts the operation. */
	sec_jd_load(&b, false, LOAD_CLASS_1, LOAD_IMM, LOAD_DSR, 0, 8,
	    (uint64_t)paylen << 32);

	/* Length-prefixed AAD; the hardware pads it out to 16 bytes. */
	if (aadlen > 0)
		sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) |
		    FIFOLD_CLASS_1 | FIFOLD_TYPE_AAD | FIFOLD_FC1,
		    sizeof(job->ccm_alen) + aadlen);

	/* Arm the drain before the message, as for GCM. */
	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_STORE) |
	    FIFOST_TYPE_MSG_DATA, paylen);

	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) | FIFOLD_CLASS_1 |
	    FIFOLD_TYPE_MSG | (enc ? FIFOLD_LC1 : FIFOLD_FC1), paylen);

	if (enc)
		sec_jd_store(&b, false, LOAD_CLASS_1, LOAD_CTX, 32, taglen,
		    digest_pa);
	else
		sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) |
		    FIFOLD_CLASS_1 | FIFOLD_TYPE_ICV | FIFOLD_LC1, taglen);

	return (sec_desc_finalize_job(&b, HEADER_SHR |
	    HEADER_SHR_DESC_L(sdlen) | HEADER_SHARE_SERIAL, &desclen));
}

/*
 * JD for encrypt-then-auth.
 */
static int
sec_jd_build_eta(struct sec_job *job)
{
	struct sec_desc_builder b;
	struct sec_session *sess = job->sess;
	const struct crypto_session_params *csp;
	uint32_t sdlen, desclen, aadlen, paylen, ivlen, in_len;
	uint32_t ctx_offset;
	vm_paddr_t shd_pa, in_sgt_pa, out_sgt_pa, digest_pa;
	int enc;

	csp = crypto_get_params(job->crp->crp_session);
	enc = CRYPTO_OP_IS_ENCRYPT(job->crp->crp_op);
	sdlen = sess->sdlen[enc];
	ivlen = csp->csp_ivlen;
	aadlen = job->crp->crp_aad_length;
	paylen = job->crp->crp_payload_length;

	in_len = ivlen + aadlen + paylen;

	shd_pa = pmap_kextract((vm_offset_t)sess->ctx[enc].shd);
	in_sgt_pa = pmap_kextract((vm_offset_t)job->in_sgt);
	out_sgt_pa = pmap_kextract((vm_offset_t)job->out_sgt);
	digest_pa = pmap_kextract((vm_offset_t)job->digest);

	sec_desc_init(&b, job->jd, SEC_MAX_DESC_WORDS);
	sec_jd_ptr(&b, shd_pa);

	sec_jd_seq(&b, true, SEQ_SGF | SEQ_EXT, out_sgt_pa, paylen);
	sec_jd_seq(&b, false, SEQ_SGF | SEQ_EXT, in_sgt_pa, in_len);

	ctx_offset = sec_cipher_ctx_offset(csp->csp_cipher_alg);

	/* IV into the class 1 context; also drops SIL by ivlen. */
	sec_jd_load(&b, true, LOAD_CLASS_1, 0, LOAD_CTX, ctx_offset,
	    ivlen, 0);

	/* AAD is authenticated only, so class 2 alone. */
	if (aadlen > 0)
		sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) |
		    FIFOLD_CLASS_2 | FIFOLD_TYPE_MSG, aadlen);

	/* Arm the ciphertext drain before feeding the message. */
	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_STORE) |
	    FIFOST_TYPE_MSG_DATA, paylen);

	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) |
	    FIFOLD_CLASS_BOTH | FIFOLD_LC1 | FIFOLD_LC2 |
	    (enc ? FIFOLD_TYPE_MSG_C1OUT : FIFOLD_TYPE_MSG), paylen);

	/* Drain the MAC to job->digest; the caller compares or copies back. */
	sec_jd_store(&b, false, LOAD_CLASS_2, LOAD_CTX, 0,
	    sess->digestlen, digest_pa);

	return (sec_desc_finalize_job(&b, HEADER_SHR |
	    HEADER_SHR_DESC_L(sdlen) | HEADER_SHARE_SERIAL, &desclen));
}

/*
 * JD for AES-GMAC: GCM with nothing to encrypt.
 */
static int
sec_jd_build_gmac(struct sec_job *job)
{
	struct sec_desc_builder b;
	struct sec_session *sess = job->sess;
	const struct crypto_session_params *csp;
	uint32_t sdlen = sess->sdlen[0];
	uint32_t desclen, ivlen, datalen, dsr_val, in_len;
	vm_paddr_t shd_pa, in_sgt_pa, digest_pa;

	csp = crypto_get_params(job->crp->crp_session);
	ivlen = csp->csp_ivlen;
	datalen = job->crp->crp_payload_length;

	/*
	 * The digest-mode SGT maps the payload only, so AAD has nowhere
	 * to come from.
	 */
	if (job->crp->crp_aad_length != 0)
		return (EOPNOTSUPP);

	dsr_val = roundup(ivlen, 16) + roundup(datalen, 16);
	in_len = ivlen + datalen;

	shd_pa = pmap_kextract((vm_offset_t)sess->ctx[0].shd);
	in_sgt_pa = pmap_kextract((vm_offset_t)job->in_sgt);
	digest_pa = pmap_kextract((vm_offset_t)job->digest);

	sec_desc_init(&b, job->jd, SEC_MAX_DESC_WORDS);
	sec_jd_ptr(&b, shd_pa);

	/* No output sequence: the tag leaves through an inline STORE. */
	sec_jd_seq(&b, false, SEQ_SGF | SEQ_EXT, in_sgt_pa, in_len);

	sec_desc_word(&b, CMD_DESC(CMD_OPERATION) | ALG_AES | AAI_AES_GCM |
	    AS_INIT_FINAL | OP_ENC);

	sec_jd_load(&b, false, LOAD_CLASS_1, LOAD_IMM, LOAD_DSR, 0, 8,
	    (uint64_t)dsr_val << 32);

	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) | FIFOLD_CLASS_1 |
	    FIFOLD_TYPE_IV | FIFOLD_FC1, ivlen);
	sec_jd_fifo(&b, CMD_DESC(CMD_SEQ_FIFO_LOAD) | FIFOLD_CLASS_1 |
	    FIFOLD_TYPE_AAD | FIFOLD_LC1, datalen);

	sec_jd_store(&b, false, LOAD_CLASS_1, LOAD_CTX, 0,
	    sess->digestlen, digest_pa);

	return (sec_desc_finalize_job(&b, HEADER_SHR |
	    HEADER_SHR_DESC_L(sdlen) | HEADER_SHARE_SERIAL, &desclen));
}

static int
sec_process(device_t dev, struct cryptop *crp, int hint)
{
	struct sec_softc *sc = device_get_softc(dev);
	struct sec_session *sess = crypto_get_driver_session(crp->crp_session);
	const struct crypto_session_params *csp;
	struct sec_job *job;
	struct sec_jr *jr;
	int error;

	job = malloc(sizeof(*job), M_SEC, M_NOWAIT | M_ZERO);
	if (job == NULL) {
		crp->crp_etype = ENOMEM;
		crypto_done(crp);
		return (0);
	}
	job->crp = crp;
	job->sess = sess;

	error = bus_dmamap_create(sc->sc_dmatag, 0, &job->map);
	if (error != 0)
		goto fail_free;

	error = bus_dmamap_load_crp(sc->sc_dmatag, job->map, crp,
	    sec_load_cb, job, BUS_DMA_NOWAIT);
	if (error != 0 || job->nsegs == 0) {
		if (error == 0)
			error = EIO;
		goto fail_destroy;
	}

	if (crp->crp_payload_length == 0) {
		error = EINVAL;
		goto fail_unload;
	}

	csp = crypto_get_params(crp->crp_session);
	if (csp->csp_ivlen > 0)
		crypto_read_iv(crp, job->iv);
	if (csp->csp_cipher_alg == CRYPTO_AES_XTS)
		sec_xts_tweak(job->iv);
	if (csp->csp_cipher_alg == CRYPTO_AES_CCM_16) {
		error = sec_ccm_prep(job, csp);
		if (error != 0)
			goto fail_unload;
	}

	error = sec_job_build_sgts(job, csp);
	if (error != 0)
		goto fail_unload;

	bus_dmamap_sync(sc->sc_dmatag, job->map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		if (csp->csp_auth_alg == CRYPTO_AES_NIST_GMAC)
			error = sec_jd_build_gmac(job);
		else
			error = sec_jd_build_digest(job);
		break;
	case CSP_MODE_AEAD:
		if (csp->csp_cipher_alg == CRYPTO_AES_CCM_16)
			error = sec_jd_build_ccm(job);
		else
			error = sec_jd_build_aead(job);
		break;
	case CSP_MODE_ETA:
		error = sec_jd_build_eta(job);
		break;
	default:
		error = sec_jd_build_cipher(job, csp);
		break;
	}
	if (error != 0)
		goto fail_unload;


	/*
	 * Hand the job to a ring and return; sec_jr_intr() completes it.
	 */
	jr = &sc->sc_jr[curcpu % sc->sc_njr];
	sec_jr_submit_job(sc, jr, job);
	return (0);

fail_unload:
	bus_dmamap_sync(sc->sc_dmatag, job->map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmatag, job->map);
fail_destroy:
	bus_dmamap_destroy(sc->sc_dmatag, job->map);
fail_free:
	free(job, M_SEC);
	/* ERESTART means opencrypto retries this crp, so don't complete it. */
	if (error == ERESTART)
		return (ERESTART);
	crp->crp_etype = error;
	crypto_done(crp);
	return (0);
}

static int
sec_reset(struct sec_softc *sc)
{
	uint32_t mcfgr;
	int i;

	/*
	 * Preserve cache-attribute fields (AWCACHE/ARCACHE) and burst
	 * settings across the reset.  The MCFGR write overwrites those
	 * along with SWRST.
	 */
	mcfgr = SEC_RD4(sc, SEC_MCFGR);
	SEC_WR4(sc, SEC_MCFGR, mcfgr | MCFGR_SWRST);

	/* Poll SWRST for self-clear. */
	for (i = 0; i < 10000; i++) {
		if ((SEC_RD4(sc, SEC_MCFGR) & MCFGR_SWRST) == 0)
			break;
		DELAY(10);
	}
	if ((SEC_RD4(sc, SEC_MCFGR) & MCFGR_SWRST) != 0) {
		device_printf(sc->sc_dev, "MCFGR.SWRST did not clear\n");
		return (EIO);
	}

	/*
	 * Post-reset configuration: 40-bit pointers, DECO watchdog on,
	 * large bursts.  Preserve whatever cache attributes the bootloader
	 * left in place.
	 */
	mcfgr &= (MCFGR_ARCACHE_M | MCFGR_AWCACHE_M);
	mcfgr |= MCFGR_PS | MCFGR_WDE | MCFGR_LARGE_BURST;
	SEC_WR4(sc, SEC_MCFGR, mcfgr);

	return (0);
}

/*
 * Instantiate one RNG state handle via DECO0 direct access.
 */
static int
sec_deco_rng_init(struct sec_softc *sc, int sh)
{
	uint32_t jd[2];
	uint32_t reg, decorr, scfgr;
	int i;

	jd[0] = CMD_DESC(CMD_DESC_HEADER) | HEADER_ONE |
	    (2 & HEADER_DESCLEN_M);
	jd[1] = CMD_DESC(CMD_OPERATION) | ALG_RNG | AS_INIT | OP_RNG_SH(sh);

	/* Request DECO0 and wait for the grant (DEN0=1). */
	SEC_WR4(sc, SEC_DECORR, DECORR_RQD0);
	decorr = SEC_RD4(sc, SEC_DECORR);
	for (i = 0; i < 10000; i++) {
		decorr = SEC_RD4(sc, SEC_DECORR);
		if ((decorr & DECORR_DEN0) != 0)
			break;
		DELAY(10);
	}
	if ((decorr & DECORR_DEN0) == 0) {
		scfgr = SEC_RD4(sc, SEC_SCFGR);
		device_printf(sc->sc_dev,
		    "DECO0 acquire timeout (DECORR=%#x SCFGR=%#x%s)\n",
		    decorr, scfgr,
		    (scfgr & SCFGR_VIRT_EN) ? " VIRT_EN" : "");
		SEC_WR4(sc, SEC_DECORR, 0);
		return (ETIMEDOUT);
	}

	SEC_WR4(sc, SEC_D0DESB(0), jd[0]);
	SEC_WR4(sc, SEC_D0DESB(1), jd[1]);

	SEC_WR4(sc, SEC_D0JQCR_MS, DAJQCR_MS_WHL);

	/* Wait for job completion */
	reg = 0;
	for (i = 0; i < 100000; i++) {
		reg = SEC_RD4(sc, SEC_D0DDR);
		if ((reg & DADDR_VALID) == 0)
			break;
		DELAY(10);
	}

	/* Release DECO0 either way. */
	SEC_WR4(sc, SEC_DECORR, 0);

	if ((reg & DADDR_VALID) != 0) {
		device_printf(sc->sc_dev,
		    "RNG SH%d instantiate timeout (D0DDR=%#x)\n", sh, reg);
		return (ETIMEDOUT);
	}
	if (((reg & DADDR_DECO_STATE_M) >> DADDR_DECO_STATE_S) != 0) {
		device_printf(sc->sc_dev,
		    "RNG SH%d instantiate error (D0DDR=%#x, DECO_STATE=%u)\n",
		    sh, reg,
		    (reg & DADDR_DECO_STATE_M) >> DADDR_DECO_STATE_S);
		return (EIO);
	}
	return (0);
}

static int
sec_rng_init(struct sec_softc *sc)
{
	uint32_t rdsta;
	int error, sh;

	/*
	 * SEC v4/v5 requires the DRNG state handles to be instantiated
	 * before any class-1 (AES/DES/RNG) job will execute.  This is typically
	 * done by the bootloader, but finish what it didn't.
	 */
	rdsta = SEC_RD4(sc, SEC_RDSTA);

	if ((rdsta & RDSTA_CE) != 0) {
		device_printf(sc->sc_dev,
		    "RNG catastrophic error (RDSTA=%#x, ERRCODE=%u)\n",
		    rdsta, (rdsta & RDSTA_ERRCODE_M) >> RDSTA_ERRCODE_S);
		return (EIO);
	}

	/* Instantiate anything the bootloader didn't. */
	for (sh = 0; sh <= 1; sh++) {
		uint32_t bit = (sh == 0) ? RDSTA_IF0 : RDSTA_IF1;

		if ((rdsta & bit) != 0)
			continue;
		error = sec_deco_rng_init(sc, sh);
		if (error != 0)
			return (error);
	}

	/* Verify the handles are now up. */
	rdsta = SEC_RD4(sc, SEC_RDSTA);
	if ((rdsta & (RDSTA_IF0 | RDSTA_IF1)) !=
	    (RDSTA_IF0 | RDSTA_IF1)) {
		device_printf(sc->sc_dev,
		    "RNG instantiation left RDSTA=%#x\n", rdsta);
		return (EIO);
	}
	return (0);
}
