
/* Common Flash Interface structures 
 * See http://support.intel.com/design/flash/technote/index.htm
 * $Id: cfi.h,v 1.32 2002/09/05 05:15:32 acurtis Exp $
 */

#ifndef __MTD_CFI_H__
#define __MTD_CFI_H__

#include <linux/config.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/mtd/flashchip.h>
#include <linux/mtd/cfi_endian.h>

/*
 * You can optimize the code size and performance by defining only 
 * the geometry(ies) available on your hardware.
 * CFIDEV_INTERLEAVE_n, where  represents the interleave (number of chips to fill the bus width)
 * CFIDEV_BUSWIDTH_n, where n is the bus width in bytes (1, 2, 4 or 8 bytes)
 *
 * By default, all (known) geometries are supported.
 */

#ifndef CONFIG_MTD_CFI_GEOMETRY

/* The default case - support all but 64-bit, which has
   a performance penalty */

#define CFIDEV_INTERLEAVE_1 (1)
#define CFIDEV_INTERLEAVE_2 (2)
#define CFIDEV_INTERLEAVE_4 (4)

#define CFIDEV_BUSWIDTH_1 (1)
#define CFIDEV_BUSWIDTH_2 (2)
#define CFIDEV_BUSWIDTH_4 (4)

typedef __u32 cfi_word;

#else

/* Explicitly configured buswidth/interleave support */

#ifdef CONFIG_MTD_CFI_I1
#define CFIDEV_INTERLEAVE_1 (1)
#endif
#ifdef CONFIG_MTD_CFI_I2
#define CFIDEV_INTERLEAVE_2 (2)
#endif
#ifdef CONFIG_MTD_CFI_I4
#define CFIDEV_INTERLEAVE_4 (4)
#endif
#ifdef CONFIG_MTD_CFI_I8
#define CFIDEV_INTERLEAVE_8 (8)
#endif

#ifdef CONFIG_MTD_CFI_B1
#define CFIDEV_BUSWIDTH_1 (1)
#endif
#ifdef CONFIG_MTD_CFI_B2
#define CFIDEV_BUSWIDTH_2 (2)
#endif
#ifdef CONFIG_MTD_CFI_B4
#define CFIDEV_BUSWIDTH_4 (4)
#endif
#ifdef CONFIG_MTD_CFI_B8
#define CFIDEV_BUSWIDTH_8 (8)
#endif

/* pick the largest necessary */
#ifdef CONFIG_MTD_CFI_B8
typedef __u64 cfi_word;

/* This only works if asm/io.h is included first */
#ifndef __raw_readll
#define __raw_readll(addr)	(*(volatile __u64 *)(addr))
#endif
#ifndef __raw_writell
#define __raw_writell(v, addr)	(*(volatile __u64 *)(addr) = (v))
#endif
#define CFI_WORD_64
#else  /* CONFIG_MTD_CFI_B8 */
/* All others can use 32-bits. It's probably more efficient than
   the smaller types anyway */
typedef __u32 cfi_word;
#endif /* CONFIG_MTD_CFI_B8 */

#endif

/*
 * The following macros are used to select the code to execute:
 *   cfi_buswidth_is_*()
 *   cfi_interleave_is_*()
 *   [where * is either 1, 2, 4, or 8]
 * Those macros should be used with 'if' statements.  If only one of few
 * geometry arrangements are selected, they expand to constants thus allowing
 * the compiler (most of them being 0) to optimize away all the unneeded code,
 * while still validating the syntax (which is not possible with embedded 
 * #if ... #endif constructs).
 * The exception to this is the 64-bit versions, which need an extension
 * to the cfi_word type, and cause compiler warnings about shifts being
 * out of range.
 */

#ifdef CFIDEV_INTERLEAVE_1
# ifdef CFIDEV_INTERLEAVE
#  undef CFIDEV_INTERLEAVE
#  define CFIDEV_INTERLEAVE (cfi->interleave)
# else
#  define CFIDEV_INTERLEAVE CFIDEV_INTERLEAVE_1
# endif
# define cfi_interleave_is_1() (CFIDEV_INTERLEAVE == CFIDEV_INTERLEAVE_1)
#else
# define cfi_interleave_is_1() (0)
#endif

#ifdef CFIDEV_INTERLEAVE_2
# ifdef CFIDEV_INTERLEAVE
#  undef CFIDEV_INTERLEAVE
#  define CFIDEV_INTERLEAVE (cfi->interleave)
# else
#  define CFIDEV_INTERLEAVE CFIDEV_INTERLEAVE_2
# endif
# define cfi_interleave_is_2() (CFIDEV_INTERLEAVE == CFIDEV_INTERLEAVE_2)
#else
# define cfi_interleave_is_2() (0)
#endif

#ifdef CFIDEV_INTERLEAVE_4
# ifdef CFIDEV_INTERLEAVE
#  undef CFIDEV_INTERLEAVE
#  define CFIDEV_INTERLEAVE (cfi->interleave)
# else
#  define CFIDEV_INTERLEAVE CFIDEV_INTERLEAVE_4
# endif
# define cfi_interleave_is_4() (CFIDEV_INTERLEAVE == CFIDEV_INTERLEAVE_4)
#else
# define cfi_interleave_is_4() (0)
#endif

#ifdef CFIDEV_INTERLEAVE_8
# ifdef CFIDEV_INTERLEAVE
#  undef CFIDEV_INTERLEAVE
#  define CFIDEV_INTERLEAVE (cfi->interleave)
# else
#  define CFIDEV_INTERLEAVE CFIDEV_INTERLEAVE_8
# endif
# define cfi_interleave_is_8() (CFIDEV_INTERLEAVE == CFIDEV_INTERLEAVE_8)
#else
# define cfi_interleave_is_8() (0)
#endif

#ifndef CFIDEV_INTERLEAVE
#error You must define at least one interleave to support!
#endif

#ifdef CFIDEV_BUSWIDTH_1
# ifdef CFIDEV_BUSWIDTH
#  undef CFIDEV_BUSWIDTH
#  define CFIDEV_BUSWIDTH (map->buswidth)
# else
#  define CFIDEV_BUSWIDTH CFIDEV_BUSWIDTH_1
# endif
# define cfi_buswidth_is_1() (CFIDEV_BUSWIDTH == CFIDEV_BUSWIDTH_1)
#else
# define cfi_buswidth_is_1() (0)
#endif

#ifdef CFIDEV_BUSWIDTH_2
# ifdef CFIDEV_BUSWIDTH
#  undef CFIDEV_BUSWIDTH
#  define CFIDEV_BUSWIDTH (map->buswidth)
# else
#  define CFIDEV_BUSWIDTH CFIDEV_BUSWIDTH_2
# endif
# define cfi_buswidth_is_2() (CFIDEV_BUSWIDTH == CFIDEV_BUSWIDTH_2)
#else
# define cfi_buswidth_is_2() (0)
#endif

#ifdef CFIDEV_BUSWIDTH_4
# ifdef CFIDEV_BUSWIDTH
#  undef CFIDEV_BUSWIDTH
#  define CFIDEV_BUSWIDTH (map->buswidth)
# else
#  define CFIDEV_BUSWIDTH CFIDEV_BUSWIDTH_4
# endif
# define cfi_buswidth_is_4() (CFIDEV_BUSWIDTH == CFIDEV_BUSWIDTH_4)
#else
# define cfi_buswidth_is_4() (0)
#endif

#ifdef CFIDEV_BUSWIDTH_8
# ifdef CFIDEV_BUSWIDTH
#  undef CFIDEV_BUSWIDTH
#  define CFIDEV_BUSWIDTH (map->buswidth)
# else
#  define CFIDEV_BUSWIDTH CFIDEV_BUSWIDTH_8
# endif
# define cfi_buswidth_is_8() (CFIDEV_BUSWIDTH == CFIDEV_BUSWIDTH_8)
#else
# define cfi_buswidth_is_8() (0)
#endif

#ifndef CFIDEV_BUSWIDTH
#error You must define at least one bus width to support!
#endif

/* NB: these values must represents the number of bytes needed to meet the 
 *     device type (x8, x16, x32).  Eg. a 32 bit device is 4 x 8 bytes. 
 *     These numbers are used in calculations.
 */
#define CFI_DEVICETYPE_X8  (8 / 8)
#define CFI_DEVICETYPE_X16 (16 / 8)
#define CFI_DEVICETYPE_X32 (32 / 8)
#define CFI_DEVICETYPE_X64 (64 / 8)

/* NB: We keep these structures in memory in HOST byteorder, except
 * where individually noted.
 */

/* Basic Query Structure */
struct cfi_ident {
  __u8  qry[3];
  __u16 P_ID;
  __u16 P_ADR;
  __u16 A_ID;
  __u16 A_ADR;
  __u8  VccMin;
  __u8  VccMax;
  __u8  VppMin;
  __u8  VppMax;
  __u8  WordWriteTimeoutTyp;
  __u8  BufWriteTimeoutTyp;
  __u8  BlockEraseTimeoutTyp;
  __u8  ChipEraseTimeoutTyp;
  __u8  WordWriteTimeoutMax;
  __u8  BufWriteTimeoutMax;
  __u8  BlockEraseTimeoutMax;
  __u8  ChipEraseTimeoutMax;
  __u8  DevSize;
  __u16 InterfaceDesc;
  __u16 MaxBufWriteSize;
  __u8  NumEraseRegions;
  __u32 EraseRegionInfo[0]; /* Not host ordered */
} __attribute__((packed));

/* Extended Query Structure for both PRI and ALT */

struct cfi_extquery {
  __u8  pri[3];
  __u8  MajorVersion;
  __u8  MinorVersion;
} __attribute__((packed));

/* Vendor-Specific PRI for Intel/Sharp Extended Command Set (0x0001) */

struct cfi_pri_intelext {
  __u8  pri[3];
  __u8  MajorVersion;
  __u8  MinorVersion;
  __u32 FeatureSupport;
  __u8  SuspendCmdSupport;
  __u16 BlkStatusRegMask;
  __u8  VccOptimal;
  __u8  VppOptimal;
  __u8  NumProtectionFields;
  __u16 ProtRegAddr;
  __u8  FactProtRegSize;
  __u8  UserProtRegSize;
} __attribute__((packed));

struct cfi_pri_query {
  __u8  NumFields;
  __u32 ProtField[1]; /* Not host ordered */
} __attribute__((packed));

struct cfi_bri_query {
  __u8  PageModeReadCap;
  __u8  NumFields;
  __u32 ConfField[1]; /* Not host ordered */
} __attribute__((packed));

#define P_ID_NONE 0
#define P_ID_INTEL_EXT 1
#define P_ID_AMD_STD 2
#define P_ID_INTEL_STD 3
#define P_ID_AMD_EXT 4
#define P_ID_MITSUBISHI_STD 256
#define P_ID_MITSUBISHI_EXT 257
#define P_ID_RESERVED 65535


#define CFI_MODE_CFI	1
#define CFI_MODE_JEDEC	0

struct cfi_private {
	__u16 cmdset;
	void *cmdset_priv;
	int interleave;
	int device_type;
	int cfi_mode;		/* Are we a JEDEC device pretending to be CFI? */
	int addr_unlock1;
	int addr_unlock2;
	int fast_prog;
	struct mtd_info *(*cmdset_setup)(struct map_info *);
	struct cfi_ident *cfiq; /* For now only one. We insist that all devs
				  must be of the same type. */
	int mfr, id;
	int numchips;
	unsigned long chipshift; /* Because they're of the same type */
	const char *im_name;	 /* inter_module name for cmdset_setup */
	struct flchip chips[0];  /* per-chip data structure for each chip */
};

#define MAX_CFI_CHIPS 8 /* Entirely arbitrary to avoid realloc() */

/*
 * Returns the command address according to the given geometry.
 */
static inline __u32 cfi_build_cmd_addr(__u32 cmd_ofs, int interleave, int type)
{
	return (cmd_ofs * type) * interleave;
}

/*
 * Transforms the CFI command for the given geometry (bus width & interleave.
 */
static inline cfi_word cfi_build_cmd(u_char cmd, struct map_info *map, struct cfi_private *cfi)
{
	cfi_word val = 0;

	if (cfi_buswidth_is_1()) {
		/* 1 x8 device */
		val = cmd;
	} else if (cfi_buswidth_is_2()) {
		if (cfi_interleave_is_1()) {
			/* 1 x16 device in x16 mode */
			val = cpu_to_cfi16(cmd);
		} else if (cfi_interleave_is_2()) {
			/* 2 (x8, x16 or x32) devices in x8 mode */
			val = cpu_to_cfi16((cmd << 8) | cmd);
		}
	} else if (cfi_buswidth_is_4()) {
		if (cfi_interleave_is_1()) {
			/* 1 x32 device in x32 mode */
			val = cpu_to_cfi32(cmd);
		} else if (cfi_interleave_is_2()) {
			/* 2 x16 device in x16 mode */
			val = cpu_to_cfi32((cmd << 16) | cmd);
		} else if (cfi_interleave_is_4()) {
			/* 4 (x8, x16 or x32) devices in x8 mode */
			val = (cmd << 16) | cmd;
			val = cpu_to_cfi32((val << 8) | val);
		}
#ifdef CFI_WORD_64
	} else if (cfi_buswidth_is_8()) {
		if (cfi_interleave_is_1()) {
			/* 1 x64 device in x64 mode */
			val = cpu_to_cfi64(cmd);
		} else if (cfi_interleave_is_2()) {
			/* 2 x32 device in x32 mode */
			val = cmd;
			val = cpu_to_cfi64((val << 32) | val);
		} else if (cfi_interleave_is_4()) {
			/* 4 (x16, x32 or x64) devices in x16 mode */
			val = (cmd << 16) | cmd;
			val = cpu_to_cfi64((val << 32) | val);
		} else if (cfi_interleave_is_8()) {
			/* 8 (x8, x16 or x32) devices in x8 mode */
			val = (cmd << 8) | cmd;
			val = (val << 16) | val;
			val = (val << 32) | val;
			val = cpu_to_cfi64(val);
		}
#endif /* CFI_WORD_64 */
	}
	return val;
}
#define CMD(x)  cfi_build_cmd((x), map, cfi)

/*
 * Read a value according to the bus width.
 */

static inline cfi_word cfi_read(struct map_info *map, __u32 addr)
{
	if (cfi_buswidth_is_1()) {
		return map->read8(map, addr);
	} else if (cfi_buswidth_is_2()) {
		return map->read16(map, addr);
	} else if (cfi_buswidth_is_4()) {
		return map->read32(map, addr);
	} else if (cfi_buswidth_is_8()) {
		return map->read64(map, addr);
	} else {
		return 0;
	}
}

/*
 * Write a value according to the bus width.
 */

static inline void cfi_write(struct map_info *map, cfi_word val, __u32 addr)
{
	if (cfi_buswidth_is_1()) {
		map->write8(map, val, addr);
	} else if (cfi_buswidth_is_2()) {
		map->write16(map, val, addr);
	} else if (cfi_buswidth_is_4()) {
		map->write32(map, val, addr);
	} else if (cfi_buswidth_is_8()) {
		map->write64(map, val, addr);
	}
}

/*
 * Sends a CFI command to a bank of flash for the given geometry.
 *
 * Returns the offset in flash where the command was written.
 * If prev_val is non-null, it will be set to the value at the command address,
 * before the command was written.
 */
static inline __u32 cfi_send_gen_cmd(u_char cmd, __u32 cmd_addr, __u32 base,
				struct map_info *map, struct cfi_private *cfi,
				int type, cfi_word *prev_val)
{
	cfi_word val;
	__u32 addr = base + cfi_build_cmd_addr(cmd_addr, CFIDEV_INTERLEAVE, type);

	val = cfi_build_cmd(cmd, map, cfi);

	if (prev_val)
		*prev_val = cfi_read(map, addr);

	cfi_write(map, val, addr);

	return addr - base;
}

static inline __u8 cfi_read_query(struct map_info *map, __u32 addr)
{
	if (cfi_buswidth_is_1()) {
		return map->read8(map, addr);
	} else if (cfi_buswidth_is_2()) {
		return cfi16_to_cpu(map->read16(map, addr));
	} else if (cfi_buswidth_is_4()) {
		return cfi32_to_cpu(map->read32(map, addr));
	} else if (cfi_buswidth_is_8()) {
		return cfi64_to_cpu(map->read64(map, addr));
	} else {
		return 0;
	}
}

static inline void cfi_udelay(int us)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
	unsigned long t = us * HZ / 1000000;
	if (t) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(t);
		return;
	}
#endif
	udelay(us);
	cond_resched();
}

static inline void cfi_spin_lock(spinlock_t *mutex)
{
	spin_lock_bh(mutex);
}

static inline void cfi_spin_unlock(spinlock_t *mutex)
{
	spin_unlock_bh(mutex);
}


#endif /* __MTD_CFI_H__ */
