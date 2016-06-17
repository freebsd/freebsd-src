/*
 * uninorth.h: definitions for using the "UniNorth" host bridge chip
 *             from Apple. This chip is used on "Core99" machines
 *
 */
#ifdef __KERNEL__
#ifndef __ASM_UNINORTH_H__
#define __ASM_UNINORTH_H__

/*
 * Uni-N config space reg. definitions
 *
 * (Little endian)
 */

/* Address ranges selection. This one should work with Bandit too */
#define UNI_N_ADDR_SELECT		0x48
#define UNI_N_ADDR_COARSE_MASK		0xffff0000	/* 256Mb regions at *0000000 */
#define UNI_N_ADDR_FINE_MASK		0x0000ffff	/*  16Mb regions at f*000000 */

/* AGP registers */
#define UNI_N_CFG_GART_BASE		0x8c
#define UNI_N_CFG_AGP_BASE		0x90
#define UNI_N_CFG_GART_CTRL		0x94
#define UNI_N_CFG_INTERNAL_STATUS	0x98

/* UNI_N_CFG_GART_CTRL bits definitions */
#define UNI_N_CFG_GART_INVAL		0x00000001
#define UNI_N_CFG_GART_ENABLE		0x00000100
#define UNI_N_CFG_GART_2xRESET		0x00010000
#define UNI_N_CFG_GART_DISSBADET	0x00020000

/* My understanding of UniNorth AGP as of UniNorth rev 1.0x,
 * revision 1.5 (x4 AGP) may need further changes.
 *
 * AGP_BASE register contains the base address of the AGP aperture on
 * the AGP bus. It doesn't seem to be visible to the CPU as of UniNorth 1.x,
 * even if decoding of this address range is enabled in the address select
 * register. Apparently, the only supported bases are 256Mb multiples
 * (high 4 bits of that register).
 *
 * GART_BASE register appear to contain the physical address of the GART
 * in system memory in the high address bits (page aligned), and the
 * GART size in the low order bits (number of GART pages)
 *
 * The GART format itself is one 32bits word per physical memory page.
 * This word contains, in little-endian format (!!!), the physical address
 * of the page in the high bits, and what appears to be an "enable" bit
 * in the LSB bit (0) that must be set to 1 when the entry is valid.
 *
 * Obviously, the GART is not cache coherent and so any change to it
 * must be flushed to memory (or maybe just make the GART space non
 * cachable). AGP memory itself doens't seem to be cache coherent neither.
 *
 * In order to invalidate the GART (which is probably necessary to inval
 * the bridge internal TLBs), the following sequence has to be written,
 * in order, to the GART_CTRL register:
 *
 *   UNI_N_CFG_GART_ENABLE | UNI_N_CFG_GART_INVAL
 *   UNI_N_CFG_GART_ENABLE
 *   UNI_N_CFG_GART_ENABLE | UNI_N_CFG_GART_2xRESET
 *   UNI_N_CFG_GART_ENABLE
 *
 * As far as AGP "features" are concerned, it looks like fast write may
 * not be supported but this has to be confirmed.
 *
 * Turning on AGP seem to require a double invalidate operation, one before
 * setting the AGP command register, on after.
 *
 * Turning off AGP seems to require the following sequence: first wait
 * for the AGP to be idle by reading the internal status register, then
 * write in that order to the GART_CTRL register:
 *
 *   UNI_N_CFG_GART_ENABLE | UNI_N_CFG_GART_INVAL
 *   0
 *   UNI_N_CFG_GART_2xRESET
 *   0
 */

/*
 * Uni-N memory mapped reg. definitions
 *
 * Those registers are Big-Endian !!
 *
 * Their meaning come from either Darwin and/or from experiments I made with
 * the bootrom, I'm not sure about their exact meaning yet
 *
 */

/* Version of the UniNorth chip */
#define UNI_N_VERSION			0x0000		/* Known versions: 3,7 and 8 */

/* This register is used to enable/disable various clocks */
#define UNI_N_CLOCK_CNTL		0x0020
#define UNI_N_CLOCK_CNTL_PCI		0x00000001	/* PCI2 clock control */
#define UNI_N_CLOCK_CNTL_GMAC		0x00000002	/* GMAC clock control */
#define UNI_N_CLOCK_CNTL_FW		0x00000004	/* FireWire clock control */
#define UNI_N_CLOCK_CNTL_ATA100		0x00000010	/* ATA-100 clock control (U2) */

/* Power Management control */
#define UNI_N_POWER_MGT			0x0030
#define UNI_N_POWER_MGT_NORMAL		0x00
#define UNI_N_POWER_MGT_IDLE2		0x01
#define UNI_N_POWER_MGT_SLEEP		0x02

/* This register is configured by Darwin depending on the UniN
 * revision
 */
#define UNI_N_ARB_CTRL			0x0040
#define UNI_N_ARB_CTRL_QACK_DELAY_SHIFT	15
#define UNI_N_ARB_CTRL_QACK_DELAY_MASK	0x0e1f8000
#define UNI_N_ARB_CTRL_QACK_DELAY	0x30
#define UNI_N_ARB_CTRL_QACK_DELAY105	0x00

/* This one _might_ return the CPU number of the CPU reading it;
 * the bootROM decides wether to boot or to sleep/spinloop depending
 * on this register beeing 0 or not
 */
#define UNI_N_CPU_NUMBER		0x0050

/* This register appear to be read by the bootROM to decide what
 *  to do on a non-recoverable reset (powerup or wakeup)
 */
#define UNI_N_HWINIT_STATE		0x0070
#define UNI_N_HWINIT_STATE_SLEEPING	0x01
#define UNI_N_HWINIT_STATE_RUNNING	0x02
/* This last bit appear to be used by the bootROM to know the second
 * CPU has started and will enter it's sleep loop with IP=0
 */
#define UNI_N_HWINIT_STATE_CPU1_FLAG	0x10000000

/* Uninorth 1.5 rev. has additional perf. monitor registers at 0xf00-0xf50 */

#endif /* __ASM_UNINORTH_H__ */
#endif /* __KERNEL__ */
