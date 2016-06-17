/* $Id: pstate.h,v 1.6 1997/06/25 07:39:45 jj Exp $ */
#ifndef _SPARC64_PSTATE_H
#define _SPARC64_PSTATE_H

/* The V9 PSTATE Register (with SpitFire extensions).
 *
 * -----------------------------------------------------------------------
 * | Resv | IG | MG | CLE | TLE |  MM  | RED | PEF | AM | PRIV | IE | AG |
 * -----------------------------------------------------------------------
 *  63  12  11   10    9     8    7   6   5     4     3     2     1    0
 */
#define PSTATE_IG	0x0000000000000800	/* Interrupt Globals.		*/
#define PSTATE_MG	0x0000000000000400	/* MMU Globals.			*/
#define PSTATE_CLE	0x0000000000000200	/* Current Little Endian.	*/
#define PSTATE_TLE	0x0000000000000100	/* Trap Little Endian.		*/
#define PSTATE_MM	0x00000000000000c0	/* Memory Model.		*/
#define PSTATE_TSO	0x0000000000000000	/* MM: Total Store Order	*/
#define PSTATE_PSO	0x0000000000000040	/* MM: Partial Store Order	*/
#define PSTATE_RMO	0x0000000000000080	/* MM: Relaxed Memory Order	*/
#define PSTATE_RED	0x0000000000000020	/* Reset Error Debug State.	*/
#define PSTATE_PEF	0x0000000000000010	/* Floating Point Enable.	*/
#define PSTATE_AM	0x0000000000000008	/* Address Mask.		*/
#define PSTATE_PRIV	0x0000000000000004	/* Privilege.			*/
#define PSTATE_IE	0x0000000000000002	/* Interrupt Enable.		*/
#define PSTATE_AG	0x0000000000000001	/* Alternate Globals.		*/

/* The V9 TSTATE Register (with SpitFire and Linux extensions).
 *
 * ---------------------------------------------------------------
 * |  Resv  |  CCR  |  ASI  |  %pil  |  PSTATE  |  Resv  |  CWP  |
 * ---------------------------------------------------------------
 *  63    40 39   32 31   24 23    20 19       8 7      5 4     0
 */
#define TSTATE_CCR	0x000000ff00000000	/* Condition Codes.		*/
#define TSTATE_XCC	0x000000f000000000	/* Condition Codes.		*/
#define TSTATE_XNEG	0x0000008000000000	/* %xcc Negative.		*/
#define TSTATE_XZERO	0x0000004000000000	/* %xcc Zero.			*/
#define TSTATE_XOVFL	0x0000002000000000	/* %xcc Overflow.		*/
#define TSTATE_XCARRY	0x0000001000000000	/* %xcc Carry.			*/
#define TSTATE_ICC	0x0000000f00000000	/* Condition Codes.		*/
#define TSTATE_INEG	0x0000000800000000	/* %icc Negative.		*/
#define TSTATE_IZERO	0x0000000400000000	/* %icc Zero.			*/
#define TSTATE_IOVFL	0x0000000200000000	/* %icc Overflow.		*/
#define TSTATE_ICARRY	0x0000000100000000	/* %icc Carry.			*/
#define TSTATE_ASI	0x00000000ff000000	/* Address Space Identifier.	*/
#define TSTATE_PIL	0x0000000000f00000	/* %pil (Linux traps set this)  */
#define TSTATE_PSTATE	0x00000000000fff00	/* PSTATE.			*/
#define TSTATE_IG	0x0000000000080000	/* Interrupt Globals.		*/
#define TSTATE_MG	0x0000000000040000	/* MMU Globals.			*/
#define TSTATE_CLE	0x0000000000020000	/* Current Little Endian.	*/
#define TSTATE_TLE	0x0000000000010000	/* Trap Little Endian.		*/
#define TSTATE_MM	0x000000000000c000	/* Memory Model.		*/
#define TSTATE_TSO	0x0000000000000000	/* MM: Total Store Order	*/
#define TSTATE_PSO	0x0000000000004000	/* MM: Partial Store Order	*/
#define TSTATE_RMO	0x0000000000008000	/* MM: Relaxed Memory Order	*/
#define TSTATE_RED	0x0000000000002000	/* Reset Error Debug State.	*/
#define TSTATE_PEF	0x0000000000001000	/* Floating Point Enable.	*/
#define TSTATE_AM	0x0000000000000800	/* Address Mask.		*/
#define TSTATE_PRIV	0x0000000000000400	/* Privilege.			*/
#define TSTATE_IE	0x0000000000000200	/* Interrupt Enable.		*/
#define TSTATE_AG	0x0000000000000100	/* Alternate Globals.		*/
#define TSTATE_CWP	0x000000000000001f	/* Current Window Pointer.	*/

/* Floating-Point Registers State Register.
 *
 * --------------------------------
 * |  Resv  |  FEF  |  DU  |  DL  |
 * --------------------------------
 *  63     3    2       1      0
 */
#define FPRS_FEF	0x0000000000000004	/* Enable Floating Point.	*/
#define FPRS_DU		0x0000000000000002	/* Dirty Upper.			*/
#define FPRS_DL		0x0000000000000001	/* Dirty Lower.			*/

/* Version Register.
 *
 * ------------------------------------------------------
 * | MANUF | IMPL | MASK | Resv | MAXTL | Resv | MAXWIN |
 * ------------------------------------------------------
 *  63   48 47  32 31  24 23  16 15    8 7    5 4      0
 */
#define VERS_MANUF	0xffff000000000000	/* Manufacturer.		*/
#define VERS_IMPL	0x0000ffff00000000	/* Implementation.		*/
#define VERS_MASK	0x00000000ff000000	/* Mask Set Revision.		*/
#define VERS_MAXTL	0x000000000000ff00	/* Maximum Trap Level.		*/
#define VERS_MAXWIN	0x000000000000001f	/* Maximum Reg Window Index.	*/

#if defined(__KERNEL__) && !defined(__ASSEMBLY__)
#define set_pstate(bits)					\
	__asm__ __volatile__(					\
		"rdpr      %%pstate, %%g1\n\t"			\
		"or        %%g1, %0, %%g1\n\t"			\
		"wrpr      %%g1, 0x0, %%pstate\n\t"		\
		: /* no outputs */				\
		: "i" (bits)					\
		: "g1")

#define clear_pstate(bits)					\
	__asm__ __volatile__(					\
		"rdpr      %%pstate, %%g1\n\t"			\
		"andn        %%g1, %0, %%g1\n\t"		\
		"wrpr      %%g1, 0x0, %%pstate\n\t"		\
		: /* no outputs */				\
		: "i" (bits)					\
		: "g1")

#define change_pstate(bits)					\
	__asm__ __volatile__(					\
		"rdpr      %%pstate, %%g1\n\t"			\
		"wrpr      %%g1, %0, %%pstate\n\t"		\
		: /* no outputs */				\
		: "i" (bits)					\
		: "g1")
#endif

#endif /* !(_SPARC64_PSTATE_H) */
