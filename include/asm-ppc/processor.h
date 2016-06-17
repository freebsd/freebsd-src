#ifdef __KERNEL__
#ifndef __ASM_PPC_PROCESSOR_H
#define __ASM_PPC_PROCESSOR_H

/*
 * The Book E definitions are hacked into here for 44x right
 * now.  This whole thing needs regorganized (maybe per-core
 * files) * so that it becomes readable. -Matt
 */

/*
 * Default implementation of macro that returns current
 * instruction pointer ("program counter").
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

#include <linux/config.h>
#include <linux/stringify.h>

#include <asm/ptrace.h>
#include <asm/types.h>
#include <asm/mpc8xx.h>

/* Machine State Register (MSR) Fields */

#ifdef CONFIG_PPC64BRIDGE
#define MSR_SF		(1<<63)
#define MSR_ISF		(1<<61)
#endif /* CONFIG_PPC64BRIDGE */
#define MSR_VEC		(1<<25)		/* Enable AltiVec */
#define MSR_POW		(1<<18)		/* Enable Power Management */
#define MSR_WE		(1<<18)		/* Wait State Enable */
#define MSR_TGPR	(1<<17)		/* TLB Update registers in use */
#define MSR_CE		(1<<17)		/* Critical Interrupt Enable */
#define MSR_ILE		(1<<16)		/* Interrupt Little Endian */
#define MSR_EE		(1<<15)		/* External Interrupt Enable */
#define MSR_PR		(1<<14)		/* Problem State / Privilege Level */
#define MSR_FP		(1<<13)		/* Floating Point enable */
#define MSR_ME		(1<<12)		/* Machine Check Enable */
#define MSR_FE0		(1<<11)		/* Floating Exception mode 0 */
#define MSR_SE		(1<<10)		/* Single Step */
#define MSR_DWE		(1<<10)		/* Debug Wait Enable (4xx) */
#define MSR_BE		(1<<9)		/* Branch Trace */
#define MSR_DE		(1<<9) 		/* Debug Exception Enable */
#define MSR_FE1		(1<<8)		/* Floating Exception mode 1 */
#define MSR_IP		(1<<6)		/* Exception prefix 0x000/0xFFF */
#define MSR_IR		(1<<5) 		/* Instruction Relocate */
#define MSR_DR		(1<<4) 		/* Data Relocate */
#define MSR_PE		(1<<3)		/* Protection Enable */
#define MSR_PX		(1<<2)		/* Protection Exclusive Mode */
#define MSR_RI		(1<<1)		/* Recoverable Exception */
#define MSR_LE		(1<<0) 		/* Little Endian */

#ifdef CONFIG_BOOKE
#define MSR_IS		MSR_IR		/* Instruction Space */
#define MSR_DS		MSR_DR		/* Data Space */
#endif

#ifdef CONFIG_APUS_FAST_EXCEPT
#define MSR_		MSR_ME|MSR_IP|MSR_RI
#else
#define MSR_		MSR_ME|MSR_RI
#endif
#define MSR_KERNEL      MSR_|MSR_IR|MSR_DR
#define MSR_USER	MSR_KERNEL|MSR_PR|MSR_EE

/* Floating Point Status and Control Register (FPSCR) Fields */

#define FPSCR_FX	0x80000000	/* FPU exception summary */
#define FPSCR_FEX	0x40000000	/* FPU enabled exception summary */
#define FPSCR_VX	0x20000000	/* Invalid operation summary */
#define FPSCR_OX	0x10000000	/* Overflow exception summary */
#define FPSCR_UX	0x08000000	/* Underflow exception summary */
#define FPSCR_ZX	0x04000000	/* Zero-devide exception summary */
#define FPSCR_XX	0x02000000	/* Inexact exception summary */
#define FPSCR_VXSNAN	0x01000000	/* Invalid op for SNaN */
#define FPSCR_VXISI	0x00800000	/* Invalid op for Inv - Inv */
#define FPSCR_VXIDI	0x00400000	/* Invalid op for Inv / Inv */
#define FPSCR_VXZDZ	0x00200000	/* Invalid op for Zero / Zero */
#define FPSCR_VXIMZ	0x00100000	/* Invalid op for Inv * Zero */
#define FPSCR_VXVC	0x00080000	/* Invalid op for Compare */
#define FPSCR_FR	0x00040000	/* Fraction rounded */
#define FPSCR_FI	0x00020000	/* Fraction inexact */
#define FPSCR_FPRF	0x0001f000	/* FPU Result Flags */
#define FPSCR_FPCC	0x0000f000	/* FPU Condition Codes */
#define FPSCR_VXSOFT	0x00000400	/* Invalid op for software request */
#define FPSCR_VXSQRT	0x00000200	/* Invalid op for square root */
#define FPSCR_VXCVI	0x00000100	/* Invalid op for integer convert */
#define FPSCR_VE	0x00000080	/* Invalid op exception enable */
#define FPSCR_OE	0x00000040	/* IEEE overflow exception enable */
#define FPSCR_UE	0x00000020	/* IEEE underflow exception enable */
#define FPSCR_ZE	0x00000010	/* IEEE zero divide exception enable */
#define FPSCR_XE	0x00000008	/* FP inexact exception enable */
#define FPSCR_NI	0x00000004	/* FPU non IEEE-Mode */
#define FPSCR_RN	0x00000003	/* FPU rounding control */

/* Special Purpose Registers (SPRNs)*/

#define	SPRN_CCR0	0x3B3	/* Core Configuration Register (4xx) */
#define	SPRN_CDBCR	0x3D7	/* Cache Debug Control Register */
#define	SPRN_CTR	0x009	/* Count Register */
#define	SPRN_DABR	0x3F5	/* Data Address Breakpoint Register */
#ifdef CONFIG_BOOKE
#define	SPRN_DAC1	0x13C	/* Book E Data Address Compare 1 */
#define	SPRN_DAC2	0x13D	/* Book E Data Address Compare 2 */
#else
#define	SPRN_DAC1	0x3F6	/* Data Address Compare 1 */
#define	SPRN_DAC2	0x3F7	/* Data Address Compare 2 */
#endif /* CONFIG_BOOKE */
#define	SPRN_DAR	0x013	/* Data Address Register */
#define	SPRN_DBAT0L	0x219	/* Data BAT 0 Lower Register */
#define	SPRN_DBAT0U	0x218	/* Data BAT 0 Upper Register */
#define	SPRN_DBAT1L	0x21B	/* Data BAT 1 Lower Register */
#define	SPRN_DBAT1U	0x21A	/* Data BAT 1 Upper Register */
#define	SPRN_DBAT2L	0x21D	/* Data BAT 2 Lower Register */
#define	SPRN_DBAT2U	0x21C	/* Data BAT 2 Upper Register */
#define	SPRN_DBAT3L	0x21F	/* Data BAT 3 Lower Register */
#define	SPRN_DBAT3U	0x21E	/* Data BAT 3 Upper Register */
#define	SPRN_DBAT4L	0x239	/* Data BAT 4 Lower Register */
#define	SPRN_DBAT4U	0x238	/* Data BAT 4 Upper Register */
#define	SPRN_DBAT5L	0x23B	/* Data BAT 5 Lower Register */
#define	SPRN_DBAT5U	0x23A	/* Data BAT 5 Upper Register */
#define	SPRN_DBAT6L	0x23D	/* Data BAT 6 Lower Register */
#define	SPRN_DBAT6U	0x23C	/* Data BAT 6 Upper Register */
#define	SPRN_DBAT7L	0x23F	/* Data BAT 7 Lower Register */
#define	SPRN_DBAT7U	0x23E	/* Data BAT 7 Upper Register */

#define	SPRN_DBCR	0x3F2	/* Debug Control Register */
#define	  DBCR_EDM	0x80000000
#define	  DBCR_IDM	0x40000000
#define	  DBCR_RST(x)	(((x) & 0x3) << 28)
#define	    DBCR_RST_NONE       	0
#define	    DBCR_RST_CORE       	1
#define	    DBCR_RST_CHIP       	2
#define	    DBCR_RST_SYSTEM		3
#define	  DBCR_IC	0x08000000	/* Instruction Completion Debug Evnt */
#define	  DBCR_BT	0x04000000	/* Branch Taken Debug Event */
#define	  DBCR_EDE	0x02000000	/* Exception Debug Event */
#define	  DBCR_TDE	0x01000000	/* TRAP Debug Event */
#define	  DBCR_FER	0x00F80000	/* First Events Remaining Mask */
#define	  DBCR_FT	0x00040000	/* Freeze Timers on Debug Event */
#define	  DBCR_IA1	0x00020000	/* Instr. Addr. Compare 1 Enable */
#define	  DBCR_IA2	0x00010000	/* Instr. Addr. Compare 2 Enable */
#define	  DBCR_D1R	0x00008000	/* Data Addr. Compare 1 Read Enable */
#define	  DBCR_D1W	0x00004000	/* Data Addr. Compare 1 Write Enable */
#define	  DBCR_D1S(x)	(((x) & 0x3) << 12)	/* Data Adrr. Compare 1 Size */
#define	    DAC_BYTE	0
#define	    DAC_HALF	1
#define	    DAC_WORD	2
#define	    DAC_QUAD	3
#define	  DBCR_D2R	0x00000800	/* Data Addr. Compare 2 Read Enable */
#define	  DBCR_D2W	0x00000400	/* Data Addr. Compare 2 Write Enable */
#define	  DBCR_D2S(x)	(((x) & 0x3) << 8)	/* Data Addr. Compare 2 Size */
#define	  DBCR_SBT	0x00000040	/* Second Branch Taken Debug Event */
#define	  DBCR_SED	0x00000020	/* Second Exception Debug Event */
#define	  DBCR_STD	0x00000010	/* Second Trap Debug Event */
#define	  DBCR_SIA	0x00000008	/* Second IAC Enable */
#define	  DBCR_SDA	0x00000004	/* Second DAC Enable */
#define	  DBCR_JOI	0x00000002	/* JTAG Serial Outbound Int. Enable */
#define	  DBCR_JII	0x00000001	/* JTAG Serial Inbound Int. Enable */
#ifdef CONFIG_BOOKE
#define	SPRN_DBCR0	0x134		/* Book E Debug Control Register 0 */
#else
#define	SPRN_DBCR0	0x3F2		/* Debug Control Register 0 */
#endif /* CONFIG_BOOKE */
#define   DBCR0_EDM         0x80000000  /* External Debug Mode             */
#define   DBCR0_IDM         0x40000000  /* Internal Debug Mode             */
#define   DBCR0_RST         0x30000000  /* all the bits in the RST field   */
#define   DBCR0_RST_SYSTEM  0x30000000  /* System Reset                    */
#define   DBCR0_RST_CHIP    0x20000000  /* Chip   Reset                    */
#define   DBCR0_RST_CORE    0x10000000  /* Core   Reset                    */
#define   DBCR0_RST_NONE    0x00000000  /* No     Reset                    */
#define   DBCR0_IC          0x08000000  /* Instruction Completion          */
#define   DBCR0_BT          0x04000000  /* Branch Taken                    */
#define   DBCR0_EDE         0x02000000  /* Exception Debug Event           */
#define   DBCR0_TDE         0x01000000  /* TRAP Debug Event                */
#define   DBCR0_IA1         0x00800000  /* Instr Addr compare 1 enable     */
#define   DBCR0_IA2         0x00400000  /* Instr Addr compare 2 enable     */
#define   DBCR0_IA12        0x00200000  /* Instr Addr 1-2 range enable     */
#define   DBCR0_IA12X       0x00100000  /* Instr Addr 1-2 range eXclusive  */
#define   DBCR0_IA3         0x00080000  /* Instr Addr compare 3 enable     */
#define   DBCR0_IA4         0x00040000  /* Instr Addr compare 4 enable     */
#define   DBCR0_IA34        0x00020000  /* Instr Addr 3-4 range Enable     */
#define   DBCR0_IA34X       0x00010000  /* Instr Addr 3-4 range eXclusive  */
#define   DBCR0_IA12T       0x00008000  /* Instr Addr 1-2 range Toggle     */
#define   DBCR0_IA34T       0x00004000  /* Instr Addr 3-4 range Toggle     */
#define   DBCR0_FT          0x00000001  /* Freeze Timers on debug event    */
#ifdef CONFIG_BOOKE
#define	SPRN_DBCR1	0x135		/* Book E Debug Control Register 1 */
#define	SPRN_DBSR	0x130		/* Book E Debug Status Register    */
#define   DBSR_IC	    0x08000000	/* Book E Instruction Completion   */
#define   DBSR_TIE	    0x01000000	/* Book E Trap Instruction debug Event*/
#else /* CONFIG_BOOKE */
#define	SPRN_DBCR1	0x3BD		/* Debug Control Register 1 */
#define	SPRN_DBSR	0x3F0		/* Debug Status Register */
#define   DBSR_IC	    0x80000000	/* Instruction Completion          */
#define   DBSR_TIE	    0x10000000	/* Trap Instruction debug Event    */
#endif /* CONFIG_BOOKE */
#define	SPRN_DCCR	0x3FA	/* Data Cache Cacheability Register */
#define	  DCCR_NOCACHE		0	/* Noncacheable */
#define	  DCCR_CACHE		1	/* Cacheable */
#define	SPRN_DCMP	0x3D1	/* Data TLB Compare Register */
#define	SPRN_DCWR	0x3BA	/* Data Cache Write-thru Register */
#define	  DCWR_COPY		0	/* Copy-back */
#define	  DCWR_WRITE		1	/* Write-through */
#ifdef CONFIG_BOOKE
#define	SPRN_DEAR	0x03D	/* Book E Data Error Address Register */
#else
#define	SPRN_DEAR	0x3D5	/* Data Error Address Register */
#endif /* CONFIG_BOOKE */
#define	SPRN_DEC	0x016	/* Decrement Register */
#define	SPRN_DER	0x095	/* Debug Enable Regsiter */
#define   DER_RSTE	0x40000000	/* Reset Interrupt */
#define   DER_CHSTPE	0x20000000	/* Check Stop */
#define   DER_MCIE	0x10000000	/* Machine Check Interrupt */
#define   DER_EXTIE	0x02000000	/* External Interrupt */
#define   DER_ALIE	0x01000000	/* Alignment Interrupt */
#define   DER_PRIE	0x00800000	/* Program Interrupt */
#define   DER_FPUVIE	0x00400000	/* FP Unavailable Interrupt */
#define   DER_DECIE	0x00200000	/* Decrementer Interrupt */
#define   DER_SYSIE	0x00040000	/* System Call Interrupt */
#define   DER_TRE	0x00020000	/* Trace Interrupt */
#define   DER_SEIE	0x00004000	/* FP SW Emulation Interrupt */
#define   DER_ITLBMSE	0x00002000	/* Imp. Spec. Instruction TLB Miss */
#define   DER_ITLBERE	0x00001000	/* Imp. Spec. Instruction TLB Error */
#define   DER_DTLBMSE	0x00000800	/* Imp. Spec. Data TLB Miss */
#define   DER_DTLBERE	0x00000400	/* Imp. Spec. Data TLB Error */
#define   DER_LBRKE	0x00000008	/* Load/Store Breakpoint Interrupt */
#define   DER_IBRKE	0x00000004	/* Instruction Breakpoint Interrupt */
#define   DER_EBRKE	0x00000002	/* External Breakpoint Interrupt */
#define   DER_DPIE	0x00000001	/* Dev. Port Nonmaskable Request */
#define	SPRN_DMISS	0x3D0		/* Data TLB Miss Register */
#define	SPRN_DSISR	0x012		/* Data Storage Interrupt Status Register */
#define	SPRN_EAR	0x11A		/* External Address Register */
#ifdef CONFIG_BOOKE
#define	SPRN_ESR	0x03E		/* Book E Exception Syndrome Register */
#else
#define	SPRN_ESR	0x3D4		/* Exception Syndrome Register */
#endif /* CONFIG_BOOKE */
#define	  ESR_IMCP	0x80000000	/* Instr. Machine Check - Protection */
#define	  ESR_IMCN	0x40000000	/* Instr. Machine Check - Non-config */
#define	  ESR_IMCB	0x20000000	/* Instr. Machine Check - Bus error */
#define	  ESR_IMCT	0x10000000	/* Instr. Machine Check - Timeout */
#define	  ESR_PIL	0x08000000	/* Program Exception - Illegal */
#define	  ESR_PPR	0x04000000	/* Program Exception - Priveleged */
#define	  ESR_PTR	0x02000000	/* Program Exception - Trap */
#define	  ESR_DST	0x00800000	/* Storage Exception - Data miss */
#define	  ESR_DIZ	0x00400000	/* Storage Exception - Zone fault */
#define	SPRN_EVPR	0x3D6	/* Exception Vector Prefix Register */
#define	SPRN_HASH1	0x3D2	/* Primary Hash Address Register */
#define	SPRN_HASH2	0x3D3	/* Secondary Hash Address Resgister */
#define	SPRN_HID0	0x3F0	/* Hardware Implementation Register 0 */
#define	  HID0_EMCP	(1<<31)		/* Enable Machine Check pin */
#define	  HID0_EBA	(1<<29)		/* Enable Bus Address Parity */
#define	  HID0_EBD	(1<<28)		/* Enable Bus Data Parity */
#define	  HID0_SBCLK	(1<<27)
#define	  HID0_EICE	(1<<26)
#define	  HID0_TBEN	(1<<26)		/* Timebase enable - 74xx and 82xx */
#define	  HID0_ECLK	(1<<25)
#define	  HID0_PAR	(1<<24)
#define	  HID0_STEN	(1<<24)		/* Software table search enable - 745x */
#define	  HID0_HIGH_BAT	(1<<23)		/* Enable high BATs - 7455 */
#define	  HID0_DOZE	(1<<23)
#define	  HID0_NAP	(1<<22)
#define	  HID0_SLEEP	(1<<21)
#define	  HID0_DPM	(1<<20)
#define	  HID0_BHTCLR	(1<<18)		/* Clear branch history table - 7450 */
#define	  HID0_XAEN	(1<<17)		/* Extended addressing enable - 7450 */
#define   HID0_NHR	(1<<16)		/* Not hard reset (software bit-7450)*/
#define	  HID0_ICE	(1<<15)		/* Instruction Cache Enable */
#define	  HID0_DCE	(1<<14)		/* Data Cache Enable */
#define	  HID0_ILOCK	(1<<13)		/* Instruction Cache Lock */
#define	  HID0_DLOCK	(1<<12)		/* Data Cache Lock */
#define	  HID0_ICFI	(1<<11)		/* Instr. Cache Flash Invalidate */
#define	  HID0_DCI	(1<<10)		/* Data Cache Invalidate */
#define   HID0_SPD	(1<<9)		/* Speculative disable */
#define   HID0_SGE	(1<<7)		/* Store Gathering Enable */
#define	  HID0_SIED	(1<<7)		/* Serial Instr. Execution [Disable] */
#define	  HID0_DFCA	(1<<6)		/* Data Cache Flush Assist */
#define   HID0_BTIC	(1<<5)		/* Branch Target Instruction Cache Enable */
#define   HID0_LRSTK	(1<<4)		/* Link register stack - 745x */
#define   HID0_ABE	(1<<3)		/* Address Broadcast Enable */
#define   HID0_FOLD	(1<<3)		/* Branch Folding enable - 745x */
#define	  HID0_BHTE	(1<<2)		/* Branch History Table Enable */
#define	  HID0_BTCD	(1<<1)		/* Branch target cache disable */
#define	  HID0_NOPDST	(1<<1)		/* No-op dst, dstt, etc. instr. */
#define	  HID0_NOPTI	(1<<0)		/* No-op dcbt and dcbst instr. */

#define	SPRN_HID1	0x3F1	/* Hardware Implementation Register 1 */
#define	  HID1_EMCP	(1<<31)		/* 7450 Machine Check Pin Enable */
#define   HID1_PC0	(1<<16)		/* 7450 PLL_CFG[0] */
#define   HID1_PC1	(1<<15)		/* 7450 PLL_CFG[1] */
#define   HID1_PC2	(1<<14)		/* 7450 PLL_CFG[2] */
#define   HID1_PC3	(1<<13)		/* 7450 PLL_CFG[3] */
#define	  HID1_SYNCBE	(1<<11)		/* 7450 ABE for sync, eieio */
#define	  HID1_ABE	(1<<10)		/* 7450 Address Broadcast Enable */
#define	SPRN_HID2	0x3F8	/* Hardware Implementation Register 2 */
#define	SPRN_IABR	0x3F2	/* Instruction Address Breakpoint Register */
#ifdef CONFIG_BOOKE
#define	SPRN_IAC1	0x138	/* Book E Instruction Address Compare 1 */
#define	SPRN_IAC2	0x139	/* Book E Instruction Address Compare 2 */
#else
#define	SPRN_IAC1	0x3F4	/* Instruction Address Compare 1 */
#define	SPRN_IAC2	0x3F5	/* Instruction Address Compare 2 */
#endif /* CONFIG_BOOKE */
#define	SPRN_IBAT0L	0x211	/* Instruction BAT 0 Lower Register */
#define	SPRN_IBAT0U	0x210	/* Instruction BAT 0 Upper Register */
#define	SPRN_IBAT1L	0x213	/* Instruction BAT 1 Lower Register */
#define	SPRN_IBAT1U	0x212	/* Instruction BAT 1 Upper Register */
#define	SPRN_IBAT2L	0x215	/* Instruction BAT 2 Lower Register */
#define	SPRN_IBAT2U	0x214	/* Instruction BAT 2 Upper Register */
#define	SPRN_IBAT3L	0x217	/* Instruction BAT 3 Lower Register */
#define	SPRN_IBAT3U	0x216	/* Instruction BAT 3 Upper Register */
#define	SPRN_IBAT4L	0x231	/* Instruction BAT 4 Lower Register */
#define	SPRN_IBAT4U	0x230	/* Instruction BAT 4 Upper Register */
#define	SPRN_IBAT5L	0x233	/* Instruction BAT 5 Lower Register */
#define	SPRN_IBAT5U	0x232	/* Instruction BAT 5 Upper Register */
#define	SPRN_IBAT6L	0x235	/* Instruction BAT 6 Lower Register */
#define	SPRN_IBAT6U	0x234	/* Instruction BAT 6 Upper Register */
#define	SPRN_IBAT7L	0x237	/* Instruction BAT 7 Lower Register */
#define	SPRN_IBAT7U	0x236	/* Instruction BAT 7 Upper Register */
#define	SPRN_ICCR	0x3FB	/* Instruction Cache Cacheability Register */
#define	  ICCR_NOCACHE		0	/* Noncacheable */
#define	  ICCR_CACHE		1	/* Cacheable */
#define	SPRN_ICDBDR	0x3D3	/* Instruction Cache Debug Data Register */
#define	SPRN_ICMP	0x3D5	/* Instruction TLB Compare Register */
#define	SPRN_ICTC	0x3FB	/* Instruction Cache Throttling Control Reg */
#define	SPRN_ICTRL 	0x3F3	/* 1011 7450 icache and interrupt ctrl */
#define   ICTRL_EICE		0x08000000	/* enable icache parity errs */
#define   ICTRL_EDCE		0x04000000	/* enable dcache parity errs */
#define   ICTRL_EICP		0x00000100	/* enable icache par. check */
#define	SPRN_IMISS	0x3D4	/* Instruction TLB Miss Register */
#define	SPRN_IMMR	0x27E  	/* Internal Memory Map Register */
#define	SPRN_L2CR	0x3F9	/* Level 2 Cache Control Regsiter */
#define L2CR_L2E		0x80000000	/* L2 enable */
#define L2CR_L2PE		0x40000000	/* L2 parity enable */
#define	L2CR_L2SIZ_MASK		0x30000000	/* L2 size mask */
#define L2CR_L2SIZ_256KB	0x10000000	/* L2 size 256KB */
#define L2CR_L2SIZ_512KB	0x20000000	/* L2 size 512KB */
#define L2CR_L2SIZ_1MB		0x30000000	/* L2 size 1MB */
#define L2CR_L2CLK_MASK		0x0e000000	/* L2 clock mask */
#define L2CR_L2CLK_DISABLED	0x00000000	/* L2 clock disabled */
#define L2CR_L2CLK_DIV1		0x02000000	/* L2 clock / 1 */
#define L2CR_L2CLK_DIV1_5	0x04000000	/* L2 clock / 1.5 */
#define L2CR_L2CLK_DIV2		0x08000000	/* L2 clock / 2 */
#define L2CR_L2CLK_DIV2_5	0x0a000000	/* L2 clock / 2.5 */
#define L2CR_L2CLK_DIV3		0x0c000000	/* L2 clock / 3 */
#define L2CR_L2RAM_MASK		0x01800000	/* L2 RAM type mask */
#define L2CR_L2RAM_FLOW		0x00000000	/* L2 RAM flow through */
#define L2CR_L2RAM_PIPE		0x01000000	/* L2 RAM pipelined */
#define L2CR_L2RAM_PIPE_LW	0x01800000	/* L2 RAM pipelined latewr */
#define	L2CR_L2DO		0x00400000	/* L2 data only */
#define L2CR_L2I		0x00200000	/* L2 global invalidate */
#define L2CR_L2CTL		0x00100000	/* L2 RAM control */
#define L2CR_L2WT		0x00080000	/* L2 write-through */
#define L2CR_L2TS		0x00040000	/* L2 test support */
#define L2CR_L2OH_MASK		0x00030000	/* L2 output hold mask */
#define L2CR_L2OH_0_5		0x00000000	/* L2 output hold 0.5 ns */
#define L2CR_L2OH_1_0		0x00010000	/* L2 output hold 1.0 ns */
#define L2CR_L2SL		0x00008000	/* L2 DLL slow */
#define L2CR_L2DF		0x00004000	/* L2 differential clock */
#define L2CR_L2BYP		0x00002000	/* L2 DLL bypass */
#define L2CR_L2IP		0x00000001	/* L2 GI in progress */
#define SPRN_L2CR2      0x3f8
#define	SPRN_L3CR	0x3FA	/* Level 3 Cache Control Regsiter (7450) */
#define L3CR_L3E		0x80000000	/* L3 enable */
#define L3CR_L3PE		0x40000000	/* L3 data parity enable */
#define L3CR_L3APE		0x20000000	/* L3 addr parity enable */
#define L3CR_L3SIZ		0x10000000	/* L3 size */
#define L3CR_L3CLKEN		0x08000000	/* L3 clock enable */
#define L3CR_L3RES		0x04000000	/* L3 special reserved bit */
#define L3CR_L3CLKDIV		0x03800000	/* L3 clock divisor */
#define L3CR_L3IO		0x00400000	/* L3 instruction only */
#define L3CR_L3SPO		0x00040000	/* L3 sample point override */
#define L3CR_L3CKSP		0x00030000	/* L3 clock sample point */
#define L3CR_L3PSP		0x0000e000	/* L3 P-clock sample point */
#define L3CR_L3REP		0x00001000	/* L3 replacement algorithm */
#define L3CR_L3HWF		0x00000800	/* L3 hardware flush */
#define L3CR_L3I		0x00000400	/* L3 global invalidate */
#define L3CR_L3RT		0x00000300	/* L3 SRAM type */
#define L3CR_L3NIRCA		0x00000080	/* L3 non-integer ratio clock adj. */
#define L3CR_L3DO		0x00000040	/* L3 data only mode */
#define L3CR_PMEN		0x00000004	/* L3 private memory enable */
#define L3CR_PMSIZ		0x00000001	/* L3 private memory size */
#define SPRN_MSSCR0	0x3f6	/* Memory Subsystem Control Register 0 */
#define SPRN_MSSSR0	0x3f7	/* Memory Subsystem Status Register 1 */
#define SPRN_LDSTCR	0x3f8	/* Load/Store control register */
#define SPRN_LDSTDB	0x3f4	/* */
#define	SPRN_LR		0x008	/* Link Register */
#define	SPRN_MMCR0	0x3B8	/* Monitor Mode Control Register 0 */
#define	SPRN_MMCR1	0x3BC	/* Monitor Mode Control Register 1 */
#define	SPRN_PBL1	0x3FC	/* Protection Bound Lower 1 */
#define	SPRN_PBL2	0x3FE	/* Protection Bound Lower 2 */
#define	SPRN_PBU1	0x3FD	/* Protection Bound Upper 1 */
#define	SPRN_PBU2	0x3FF	/* Protection Bound Upper 2 */
#ifdef CONFIG_BOOKE
#define	SPRN_PID	0x030	/* Book E Process ID */
#define	SPRN_PIR	0x11E	/* Book E Processor Identification Register */
#else
#define	SPRN_PID	0x3B1	/* Process ID */
#define	SPRN_PIR	0x3FF	/* Processor Identification Register */
#endif /* CONFIG_BOOKE */
#define	SPRN_PIT	0x3DB	/* Programmable Interval Timer */
#define	SPRN_PMC1	0x3B9	/* Performance Counter Register 1 */
#define	SPRN_PMC2	0x3BA	/* Performance Counter Register 2 */
#define	SPRN_PMC3	0x3BD	/* Performance Counter Register 3 */
#define	SPRN_PMC4	0x3BE	/* Performance Counter Register 4 */
#define	SPRN_PTEHI	0x3D5	/* 981 7450 PTE HI word (S/W TLB load) */
#define	SPRN_PTELO	0x3D6	/* 982 7450 PTE LO word (S/W TLB load)  */
#define	SPRN_PVR	0x11F	/* Processor Version Register */
#define	SPRN_RPA	0x3D6	/* Required Physical Address Register */
#define	SPRN_SDA	0x3BF	/* Sampled Data Address Register */
#define	SPRN_SDR1	0x019	/* MMU Hash Base Register */
#define	SPRN_SGR	0x3B9	/* Storage Guarded Register */
#define	  SGR_NORMAL		0
#define	  SGR_GUARDED		1
#define	SPRN_SIA	0x3BB	/* Sampled Instruction Address Register */
#define	SPRN_SLER	0x3BB	/* Little-endian real mode */
#define	SPRN_SPRG0	0x110	/* Special Purpose Register General 0 */
#define	SPRN_SPRG1	0x111	/* Special Purpose Register General 1 */
#define	SPRN_SPRG2	0x112	/* Special Purpose Register General 2 */
#define	SPRN_SPRG3	0x113	/* Special Purpose Register General 3 */
#define	SPRN_SPRG4	0x114	/* Special Purpose Register General 4 (4xx) */
#define	SPRN_SPRG5	0x115	/* Special Purpose Register General 5 (4xx) */
#define	SPRN_SPRG6	0x116	/* Special Purpose Register General 6 (4xx) */
#define	SPRN_SPRG7	0x117	/* Special Purpose Register General 7 (4xx) */
#define SPRG4R	SPRN_SPRG4R	/* Book E Supervisor Private Registers */
#define SPRG5R	SPRN_SPRG5R
#define SPRG6R	SPRN_SPRG6R
#define SPRG7R	SPRN_SPRG7R
#define SPRG4W	SPRN_SPRG4W
#define SPRG5W	SPRN_SPRG5W
#define SPRG6W	SPRN_SPRG6W
#define SPRG7W	SPRN_SPRG7W
#define	SPRN_SRR0	0x01A	/* Save/Restore Register 0 */
#define	SPRN_SRR1	0x01B	/* Save/Restore Register 1 */
#define	SPRN_SRR2	0x3DE	/* Save/Restore Register 2 */
#define	SPRN_SRR3 	0x3DF	/* Save/Restore Register 3 */
#define	SPRN_SU0R	0x3BC	/* "User 0" real mode */
#define	SPRN_TBHI	0x3DC	/* Time Base High (4xx) */
#define	SPRN_TBHU	0x3CC	/* Time Base High User-mode (4xx) */
#define	SPRN_TBLO	0x3DD	/* Time Base Low (4xx) */
#define	SPRN_TBLU	0x3CD	/* Time Base Low User-mode (4xx) */
#define	SPRN_TBRL	0x10C	/* Time Base Read Lower Register (user, R/O) */
#define	SPRN_TBRU	0x10D	/* Time Base Read Upper Register (user, R/O) */
#define	SPRN_TBWL	0x11C	/* Time Base Lower Register (super, R/W) */
#define	SPRN_TBWU	0x11D	/* Time Base Upper Register (super, R/W) */
#ifdef CONFIG_BOOKE
#define	SPRN_TCR	0x154	/* Book E Timer Control Register */
#else
#define	SPRN_TCR	0x3DA	/* Timer Control Register */
#endif /* CONFIG_BOOKE */
#define	  TCR_WP(x)		(((x)&0x3)<<30)	/* WDT Period */
#define	    WP_2_17		0		/* 2^17 clocks */
#define	    WP_2_21		1		/* 2^21 clocks */
#define	    WP_2_25		2		/* 2^25 clocks */
#define	    WP_2_29		3		/* 2^29 clocks */
#define	  TCR_WRC(x)		(((x)&0x3)<<28)	/* WDT Reset Control */
#define	    WRC_NONE		0		/* No reset will occur */
#define	    WRC_CORE		1		/* Core reset will occur */
#define	    WRC_CHIP		2		/* Chip reset will occur */
#define	    WRC_SYSTEM		3		/* System reset will occur */
#define	  TCR_WIE		0x08000000	/* WDT Interrupt Enable */
#define	  TCR_PIE		0x04000000	/* PIT Interrupt Enable */
#define	  TCR_DIE		TCR_PIE		/* DEC Interrupt Enable */
#define	  TCR_FP(x)		(((x)&0x3)<<24)	/* FIT Period */
#define	    FP_2_9		0		/* 2^9 clocks */
#define	    FP_2_13		1		/* 2^13 clocks */
#define	    FP_2_17		2		/* 2^17 clocks */
#define	    FP_2_21		3		/* 2^21 clocks */
#define	  TCR_FIE		0x00800000	/* FIT Interrupt Enable */
#define	  TCR_ARE		0x00400000	/* Auto Reload Enable */
#define	SPRN_THRM1	0x3FC	/* Thermal Management Register 1 */
/* these bits were defined in inverted endian sense originally, ugh, confusing */
#define	  THRM1_TIN		(1 << 31)
#define	  THRM1_TIV		(1 << 30)
#define	  THRM1_THRES(x)	((x&0x7f)<<23)
#define   THRM3_SITV(x)		((x&0x3fff)<<1)
#define	  THRM1_TID		(1<<2)
#define	  THRM1_TIE		(1<<1)
#define	  THRM1_V		(1<<0)
#define	SPRN_THRM2	0x3FD	/* Thermal Management Register 2 */
#define	SPRN_THRM3	0x3FE	/* Thermal Management Register 3 */
#define	  THRM3_E		(1<<0)
#define	SPRN_TLBMISS	0x3D4	/* 980 7450 TLB Miss Register */
#ifdef CONFIG_BOOKE
#define	SPRN_TSR	0x150	/* Book E Timer Status Register */
#else
#define	SPRN_TSR	0x3D8	/* Timer Status Register */
#endif /* CONFIG_BOOKE */
#define	  TSR_ENW		0x80000000	/* Enable Next Watchdog */
#define	  TSR_WIS		0x40000000	/* WDT Interrupt Status */
#define	  TSR_WRS(x)		(((x)&0x3)<<28)	/* WDT Reset Status */
#define	    WRS_NONE		0		/* No WDT reset occurred */
#define	    WRS_CORE		1		/* WDT forced core reset */
#define	    WRS_CHIP		2		/* WDT forced chip reset */
#define	    WRS_SYSTEM		3		/* WDT forced system reset */
#define	  TSR_PIS		0x08000000	/* PIT Interrupt Status */
#define	  TSR_DIS		TSR_PIS		/* DEC Interrupt Status */
#define	  TSR_FIS		0x04000000	/* FIT Interrupt Status */
#define	SPRN_UMMCR0	0x3A8	/* User Monitor Mode Control Register 0 */
#define	SPRN_UMMCR1	0x3AC	/* User Monitor Mode Control Register 0 */
#define	SPRN_UPMC1	0x3A9	/* User Performance Counter Register 1 */
#define	SPRN_UPMC2	0x3AA	/* User Performance Counter Register 2 */
#define	SPRN_UPMC3	0x3AD	/* User Performance Counter Register 3 */
#define	SPRN_UPMC4	0x3AE	/* User Performance Counter Register 4 */
#define	SPRN_USIA	0x3AB	/* User Sampled Instruction Address Register */
#define SPRN_VRSAVE	0x100	/* Vector Register Save Register */
#define	SPRN_XER	0x001	/* Fixed Point Exception Register */
#define	SPRN_ZPR	0x3B0	/* Zone Protection Register */

/* Book E definitions */
#define SPRN_DECAR	0x036	/* Decrementer Auto Reload Register */
#define SPRN_CSRR0	0x03A	/* Critical Save and Restore Register 0 */
#define SPRN_CSRR1	0x03B	/* Critical Save and Restore Register 1 */
#define	SPRN_IVPR	0x03F	/* Interrupt Vector Prefix Register */
#define SPRN_USPRG0	0x100	/* User Special Purpose Register General 0 */
#define	SPRN_SPRG4R	0x104	/* Special Purpose Register General 4 Read */
#define	SPRN_SPRG5R	0x105	/* Special Purpose Register General 5 Read */
#define	SPRN_SPRG6R	0x106	/* Special Purpose Register General 6 Read */
#define	SPRN_SPRG7R	0x107	/* Special Purpose Register General 7 Read */
#define	SPRN_SPRG4W	0x114	/* Special Purpose Register General 4 Write */
#define	SPRN_SPRG5W	0x115	/* Special Purpose Register General 5 Write */
#define	SPRN_SPRG6W	0x116	/* Special Purpose Register General 6 Write */
#define	SPRN_SPRG7W	0x117	/* Special Purpose Register General 7 Write */
#define SPRN_DBCR2	0x136	/* Debug Control Register 2 */
#define	SPRN_IAC3	0x13A	/* Instruction Address Compare 3 */
#define	SPRN_IAC4	0x13B	/* Instruction Address Compare 4 */
#define SPRN_DVC1	0x13E	/* */
#define SPRN_DVC2	0x13F	/* */
#define SPRN_IVOR0	0x190	/* Interrupt Vector Offset Register 0 */
#define SPRN_IVOR1	0x191	/* Interrupt Vector Offset Register 1 */
#define SPRN_IVOR2	0x192	/* Interrupt Vector Offset Register 2 */
#define SPRN_IVOR3	0x193	/* Interrupt Vector Offset Register 3 */
#define SPRN_IVOR4	0x194	/* Interrupt Vector Offset Register 4 */
#define SPRN_IVOR5	0x195	/* Interrupt Vector Offset Register 5 */
#define SPRN_IVOR6	0x196	/* Interrupt Vector Offset Register 6 */
#define SPRN_IVOR7	0x197	/* Interrupt Vector Offset Register 7 */
#define SPRN_IVOR8	0x198	/* Interrupt Vector Offset Register 8 */
#define SPRN_IVOR9	0x199	/* Interrupt Vector Offset Register 9 */
#define SPRN_IVOR10	0x19a	/* Interrupt Vector Offset Register 10 */
#define SPRN_IVOR11	0x19b	/* Interrupt Vector Offset Register 11 */
#define SPRN_IVOR12	0x19c	/* Interrupt Vector Offset Register 12 */
#define SPRN_IVOR13	0x19d	/* Interrupt Vector Offset Register 13 */
#define SPRN_IVOR14	0x19e	/* Interrupt Vector Offset Register 14 */
#define SPRN_IVOR15	0x19f	/* Interrupt Vector Offset Register 15 */
#define SPRN_MCSRR0	0x23a	/* Machine Check Save and Restore Register 0 */
#define SPRN_MCSRR1	0x23b	/* Machine Check Save and Restore Register 1 */
#define SPRN_MCSR	0x23c	/* Machine Check Syndrom Register */
#if defined(CONFIG_440A)
#define  MCSR_MCS	0x80000000 /* Machine Check Summary */
#define  MCSR_IB	0x40000000 /* Instruction PLB Error */
#define  MCSR_DRB	0x20000000 /* Data Read PLB Error */
#define  MCSR_DWB	0x10000000 /* Data Write PLB Error */
#define  MCSR_TLBP	0x08000000 /* TLB Parity Error */
#define  MCSR_ICP	0x04000000 /* I-Cache Parity Error */
#define  MCSR_DCSP	0x02000000 /* D-Cache Search Parity Error */
#define  MCSR_DCFP	0x01000000 /* D-Cache Flush Parity Error */
#define  MCSR_IMPE	0x00800000 /* Imprecise Machine Check Exception */
#endif
#define SPRN_MMUCR	0x3b2	/* MMU Control Register */

/* Short-hand versions for a number of the above SPRNs */

#define	CTR	SPRN_CTR	/* Counter Register */
#define	DAR	SPRN_DAR	/* Data Address Register */
#define	DABR	SPRN_DABR	/* Data Address Breakpoint Register */
#define	DBAT0L	SPRN_DBAT0L	/* Data BAT 0 Lower Register */
#define	DBAT0U	SPRN_DBAT0U	/* Data BAT 0 Upper Register */
#define	DBAT1L	SPRN_DBAT1L	/* Data BAT 1 Lower Register */
#define	DBAT1U	SPRN_DBAT1U	/* Data BAT 1 Upper Register */
#define	DBAT2L	SPRN_DBAT2L	/* Data BAT 2 Lower Register */
#define	DBAT2U	SPRN_DBAT2U	/* Data BAT 2 Upper Register */
#define	DBAT3L	SPRN_DBAT3L	/* Data BAT 3 Lower Register */
#define	DBAT3U	SPRN_DBAT3U	/* Data BAT 3 Upper Register */
#define	DBAT4L	SPRN_DBAT4L	/* Data BAT 4 Lower Register */
#define	DBAT4U	SPRN_DBAT4U	/* Data BAT 4 Upper Register */
#define	DBAT5L	SPRN_DBAT5L	/* Data BAT 5 Lower Register */
#define	DBAT5U	SPRN_DBAT5U	/* Data BAT 5 Upper Register */
#define	DBAT6L	SPRN_DBAT6L	/* Data BAT 6 Lower Register */
#define	DBAT6U	SPRN_DBAT6U	/* Data BAT 6 Upper Register */
#define	DBAT7L	SPRN_DBAT7L	/* Data BAT 7 Lower Register */
#define	DBAT7U	SPRN_DBAT7U	/* Data BAT 7 Upper Register */
#define	DCMP	SPRN_DCMP      	/* Data TLB Compare Register */
#define	DEC	SPRN_DEC       	/* Decrement Register */
#define DECAR	SPRN_DECAR	/* Decrementer Auto Reload Register */
#define	DMISS	SPRN_DMISS     	/* Data TLB Miss Register */
#define	DSISR	SPRN_DSISR	/* Data Storage Interrupt Status Register */
#define	EAR	SPRN_EAR       	/* External Address Register */
#define	HASH1	SPRN_HASH1	/* Primary Hash Address Register */
#define	HASH2	SPRN_HASH2	/* Secondary Hash Address Register */
#define	HID0	SPRN_HID0	/* Hardware Implementation Register 0 */
#define	HID1	SPRN_HID1	/* Hardware Implementation Register 1 */
#define	IABR	SPRN_IABR      	/* Instruction Address Breakpoint Register */
#define	IBAT0L	SPRN_IBAT0L	/* Instruction BAT 0 Lower Register */
#define	IBAT0U	SPRN_IBAT0U	/* Instruction BAT 0 Upper Register */
#define	IBAT1L	SPRN_IBAT1L	/* Instruction BAT 1 Lower Register */
#define	IBAT1U	SPRN_IBAT1U	/* Instruction BAT 1 Upper Register */
#define	IBAT2L	SPRN_IBAT2L	/* Instruction BAT 2 Lower Register */
#define	IBAT2U	SPRN_IBAT2U	/* Instruction BAT 2 Upper Register */
#define	IBAT3L	SPRN_IBAT3L	/* Instruction BAT 3 Lower Register */
#define	IBAT3U	SPRN_IBAT3U	/* Instruction BAT 3 Upper Register */
#define	IBAT4L	SPRN_IBAT4L	/* Instruction BAT 4 Lower Register */
#define	IBAT4U	SPRN_IBAT4U	/* Instruction BAT 4 Upper Register */
#define	IBAT5L	SPRN_IBAT5L	/* Instruction BAT 5 Lower Register */
#define	IBAT5U	SPRN_IBAT5U	/* Instruction BAT 5 Upper Register */
#define	IBAT6L	SPRN_IBAT6L	/* Instruction BAT 6 Lower Register */
#define	IBAT6U	SPRN_IBAT6U	/* Instruction BAT 6 Upper Register */
#define	IBAT7L	SPRN_IBAT7L	/* Instruction BAT 7 Lower Register */
#define	IBAT7U	SPRN_IBAT7U	/* Instruction BAT 7 Upper Register */
#define	ICMP	SPRN_ICMP	/* Instruction TLB Compare Register */
#define	IMISS	SPRN_IMISS	/* Instruction TLB Miss Register */
#define	IMMR	SPRN_IMMR      	/* PPC 860/821 Internal Memory Map Register */
#define	L2CR	SPRN_L2CR    	/* Classic PPC L2 cache control register */
#define	L3CR	SPRN_L3CR	/* PPC 745x L3 cache control register */
#define	LR	SPRN_LR
#define	PVR	SPRN_PVR	/* Processor Version */
#define	RPA	SPRN_RPA	/* Required Physical Address Register */
#define	SDR1	SPRN_SDR1      	/* MMU hash base register */
#define USPRG0	SPRN_USPRG0
#define	SPR0	SPRN_SPRG0	/* Supervisor Private Registers */
#define	SPR1	SPRN_SPRG1
#define	SPR2	SPRN_SPRG2
#define	SPR3	SPRN_SPRG3
#define	SPR4	SPRN_SPRG4	/* Supervisor Private Registers (4xx) */
#define	SPR5	SPRN_SPRG5
#define	SPR6	SPRN_SPRG6
#define	SPR7	SPRN_SPRG7
#define	SPRG0   SPRN_SPRG0
#define	SPRG1   SPRN_SPRG1
#define	SPRG2   SPRN_SPRG2
#define	SPRG3   SPRN_SPRG3
#define	SPRG4   SPRN_SPRG4
#define	SPRG5   SPRN_SPRG5
#define	SPRG6   SPRN_SPRG6
#define	SPRG7   SPRN_SPRG7
#define	SRR0	SPRN_SRR0	/* Save and Restore Register 0 */
#define	SRR1	SPRN_SRR1	/* Save and Restore Register 1 */
#define	TBRL	SPRN_TBRL	/* Time Base Read Lower Register */
#define	TBRU	SPRN_TBRU	/* Time Base Read Upper Register */
#define	TBWL	SPRN_TBWL	/* Time Base Write Lower Register */
#define	TBWU	SPRN_TBWU	/* Time Base Write Upper Register */
#define ICTC	1019
#define	THRM1	SPRN_THRM1	/* Thermal Management Register 1 */
#define	THRM2	SPRN_THRM2	/* Thermal Management Register 2 */
#define	THRM3	SPRN_THRM3	/* Thermal Management Register 3 */
#define	XER	SPRN_XER

/* Processor Version Register */

/* Processor Version Register (PVR) field extraction */

#define	PVR_VER(pvr)  (((pvr) >>  16) & 0xFFFF)	/* Version field */
#define	PVR_REV(pvr)  (((pvr) >>   0) & 0xFFFF)	/* Revison field */

/*
 * IBM has further subdivided the standard PowerPC 16-bit version and
 * revision subfields of the PVR for the PowerPC 403s into the following:
 */

#define	PVR_FAM(pvr)	(((pvr) >> 20) & 0xFFF)	/* Family field */
#define	PVR_MEM(pvr)	(((pvr) >> 16) & 0xF)	/* Member field */
#define	PVR_CORE(pvr)	(((pvr) >> 12) & 0xF)	/* Core field */
#define	PVR_CFG(pvr)	(((pvr) >>  8) & 0xF)	/* Configuration field */
#define	PVR_MAJ(pvr)	(((pvr) >>  4) & 0xF)	/* Major revision field */
#define	PVR_MIN(pvr)	(((pvr) >>  0) & 0xF)	/* Minor revision field */

/* Processor Version Numbers */

#define	PVR_403GA	0x00200000
#define	PVR_403GB	0x00200100
#define	PVR_403GC	0x00200200
#define	PVR_403GCX	0x00201400
#define	PVR_405GP	0x40110000
#define	PVR_STB03XXX	0x40310000
#define PVR_440GP_RB	0x40120440
#define PVR_440GP_RC1	0x40120481
#define PVR_440GP_RC2	0x40200481
#define PVR_440GX_RA	0x51b21850
#define PVR_440GX_RB	0x51b21851
#define PVR_440GX_RB1	0x51b21852
#define	PVR_601		0x00010000
#define	PVR_602		0x00050000
#define	PVR_603		0x00030000
#define	PVR_603e	0x00060000
#define	PVR_603ev	0x00070000
#define	PVR_603r	0x00071000
#define	PVR_604		0x00040000
#define	PVR_604e	0x00090000
#define	PVR_604r	0x000A0000
#define	PVR_620		0x00140000
#define	PVR_740		0x00080000
#define	PVR_750		PVR_740
#define	PVR_740P	0x10080000
#define	PVR_750P	PVR_740P
#define	PVR_7400	0x000C0000
#define	PVR_7410	0x800C0000
#define	PVR_7450	0x80000000
/*
 * For the 8xx processors, all of them report the same PVR family for
 * the PowerPC core. The various versions of these processors must be
 * differentiated by the version number in the Communication Processor
 * Module (CPM).
 */
#define	PVR_821		0x00500000
#define	PVR_823		PVR_821
#define	PVR_850		PVR_821
#define	PVR_860		PVR_821
#define	PVR_8240	0x00810100
#define	PVR_8260	PVR_8240

/* We only need to define a new _MACH_xxx for machines which are part of
 * a configuration which supports more than one type of different machine.
 * This is currently limited to CONFIG_ALL_PPC and CHRP/PReP/PMac. -- Tom
 */
#define _MACH_prep	0x00000001
#define _MACH_Pmac	0x00000002	/* pmac or pmac clone (non-chrp) */
#define _MACH_chrp	0x00000004	/* chrp machine */

/* see residual.h for these */
#define _PREP_Motorola 0x01  /* motorola prep */
#define _PREP_Firm     0x02  /* firmworks prep */
#define _PREP_IBM      0x00  /* ibm prep */
#define _PREP_Bull     0x03  /* bull prep */

/* these are arbitrary */
#define _CHRP_Motorola 0x04  /* motorola chrp, the cobra */
#define _CHRP_IBM      0x05  /* IBM chrp, the longtrail and longtrail 2 */

#define _GLOBAL(n)\
        .stabs __stringify(n:F-1),N_FUN,0,0,n;\
	.globl n;\
n:

/* Macros for setting and retrieving special purpose registers */

#define mfdcr(rn)	({unsigned int rval; \
			asm volatile("mfdcr %0," __stringify(rn) \
				     : "=r" (rval)); rval;})
#define mtdcr(rn, v)	asm volatile("mtdcr " __stringify(rn) ",%0" : : "r" (v))

#define mfmsr()		({unsigned int rval; \
			asm volatile("mfmsr %0" : "=r" (rval)); rval;})
#define mtmsr(v)	asm volatile("mtmsr %0" : : "r" (v))

#define mfspr(rn)	({unsigned int rval; \
			asm volatile("mfspr %0," __stringify(rn) \
				     : "=r" (rval)); rval;})
#define mtspr(rn, v)	asm volatile("mtspr " __stringify(rn) ",%0" : : "r" (v))

/* Segment Registers */

#define SR0	0
#define SR1	1
#define SR2	2
#define SR3	3
#define SR4	4
#define SR5	5
#define SR6	6
#define SR7	7
#define SR8	8
#define SR9	9
#define SR10	10
#define SR11	11
#define SR12	12
#define SR13	13
#define SR14	14
#define SR15	15

#ifndef __ASSEMBLY__
#if defined(CONFIG_ALL_PPC)
extern int _machine;

/* what kind of prep workstation we are */
extern int _prep_type;

/*
 * This is used to identify the board type from a given PReP board
 * vendor. Board revision is also made available.
 */
extern unsigned char ucSystemType;
extern unsigned char ucBoardRev;
extern unsigned char ucBoardRevMaj, ucBoardRevMin;
#else
#define _machine 0
#endif /* CONFIG_ALL_PPC */

struct task_struct;
void start_thread(struct pt_regs *regs, unsigned long nip, unsigned long sp);
void release_thread(struct task_struct *);

/*
 * Create a new kernel thread.
 */
extern long arch_kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);

/*
 * Bus types
 */
#define EISA_bus 0
#define EISA_bus__is_a_macro /* for versions in ksyms.c */
#define MCA_bus 0
#define MCA_bus__is_a_macro /* for versions in ksyms.c */

/* Lazy FPU handling on uni-processor */
extern struct task_struct *last_task_used_math;
extern struct task_struct *last_task_used_altivec;

/*
 * this is the minimum allowable io space due to the location
 * of the io areas on prep (first one at 0x80000000) but
 * as soon as I get around to remapping the io areas with the BATs
 * to match the mac we can raise this. -- Cort
 */
#define TASK_SIZE	CONFIG_TASK_SIZE

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	(TASK_SIZE / 8 * 3)

typedef struct {
	unsigned long seg;
} mm_segment_t;

struct thread_struct {
	unsigned long	ksp;		/* Kernel stack pointer */
	struct pt_regs	*regs;		/* Pointer to saved register state */
	mm_segment_t	fs;		/* for get_fs() validation */
	void		*pgdir;		/* root of page-table tree */
	int		fpexc_mode;	/* floating-point exception mode */
	signed long     last_syscall;
	double		fpr[32];	/* Complete floating point set */
	unsigned long	fpscr_pad;	/* fpr ... fpscr must be contiguous */
	unsigned long	fpscr;		/* Floating point status */
#ifdef CONFIG_ALTIVEC
	vector128	vr[32];		/* Complete AltiVec set */
	vector128	vscr;		/* AltiVec status */
	unsigned long	vrsave;
	int		used_vr;	/* set if process has used altivec */
#endif /* CONFIG_ALTIVEC */
#if defined(CONFIG_4xx)
	/* Saved 4xx debug registers */
	unsigned long dbcr0;
#endif
};

#define INIT_SP		(sizeof(init_stack) + (unsigned long) &init_stack)

#define INIT_THREAD  { \
	.ksp = INIT_SP, \
	.fs = KERNEL_DS, \
	.pgdir = swapper_pg_dir, \
	.fpexc_mode = MSR_FE0 | MSR_FE1, \
}

/*
 * Return saved PC of a blocked thread. For now, this is the "user" PC
 */
static inline unsigned long thread_saved_pc(struct thread_struct *t)
{
	return (t->regs) ? t->regs->nip : 0;
}

#define copy_segments(tsk, mm)		do { } while (0)
#define release_segments(mm)		do { } while (0)

unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)  ((tsk)->thread.regs? (tsk)->thread.regs->nip: 0)
#define KSTK_ESP(tsk)  ((tsk)->thread.regs? (tsk)->thread.regs->gpr[1]: 0)

/* Get/set floating-point exception mode */
#define GET_FPEXC_CTL(tsk, adr)	get_fpexc_mode((tsk), (adr))
#define SET_FPEXC_CTL(tsk, val)	set_fpexc_mode((tsk), (val))

extern int get_fpexc_mode(struct task_struct *tsk, unsigned long adr);
extern int set_fpexc_mode(struct task_struct *tsk, unsigned int val);

/*
 * NOTE! The task struct and the stack go together
 */
#define THREAD_SIZE (2*PAGE_SIZE)
#define alloc_task_struct() \
	((struct task_struct *) __get_free_pages(GFP_KERNEL,1))
#define free_task_struct(p)	free_pages((unsigned long)(p),1)
#define get_task_struct(tsk)      atomic_inc(&virt_to_page(tsk)->count)

/* in process.c - for early bootup debug -- Cort */
int ll_printk(const char *, ...);
void ll_puts(const char *);

#define init_task	(init_task_union.task)
#define init_stack	(init_task_union.stack)

/* In misc.c */
void _nmask_and_or_msr(unsigned long nmask, unsigned long or_val);

#define cpu_relax()	do { } while (0)

/*
 * Prefetch macros.
 */
#define ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCHW
#define ARCH_HAS_SPINLOCK_PREFETCH

extern inline void prefetch(const void *x)
{
	 __asm__ __volatile__ ("dcbt 0,%0" : : "r" (x));
}

extern inline void prefetchw(const void *x)
{
	 __asm__ __volatile__ ("dcbtst 0,%0" : : "r" (x));
}

#define spin_lock_prefetch(x)	prefetchw(x)

#endif /* !__ASSEMBLY__ */

#define have_of (_machine == _MACH_chrp || _machine == _MACH_Pmac)

#endif /* __ASM_PPC_PROCESSOR_H */
#endif /* __KERNEL__ */
