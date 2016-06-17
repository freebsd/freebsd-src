/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2001, 2002-2003 Silicon Graphics, Inc.  All rights reserved.
 */


#ifndef _SHUB_MD_H
#define _SHUB_MD_H

/* SN2 supports a mostly-flat address space with 4 CPU-visible, evenly spaced, 
   contiguous regions, or "software banks".  On SN2, software bank n begins at 
   addresses n * 16GB, 0 <= n < 4.  Each bank has a 16GB address space.  If 
   the 4 dimms do not use up this space there will be holes between the 
   banks.  Even with these holes the whole memory space within a bank is
   not addressable address space.  The top 1/32 of each bank is directory
   memory space and is accessible through bist only.

   Physically a SN2 node board contains 2 daughter cards with 8 dimm sockets
   each.  A total of 16 dimm sockets arranged as 4 "DIMM banks" of 4 dimms 
   each.  The data is stripped across the 4 memory busses so all dimms within 
   a dimm bank must have identical capacity dimms.  Memory is increased or 
   decreased in sets of 4.  Each dimm bank has 2 dimms on each side.

             Physical Dimm Bank layout.
                  DTR Card0
                 ------------
   Dimm Bank 3   |  MemYL3  |   CS 3
                 |  MemXL3  |
                 |----------|
   Dimm Bank 2   |  MemYL2  |   CS 2
                 |  MemXL2  |
                 |----------|
   Dimm Bank 1   |  MemYL1  |   CS 1
                 |  MemXL1  |
                 |----------|
   Dimm Bank 0   |  MemYL0  |   CS 0 
                 |  MemXL0  |
                 ------------
                  |       |
                  BUS     BUS
                  XL      YL
                  |       |
                 ------------
                 |   SHUB   |
                 |    MD    |
                 ------------
                  |       |
                  BUS     BUS
                  XR      YR
                  |       |
                 ------------
   Dimm Bank 0   |  MemXR0  |   CS 0
                 |  MemYR0  |
                 |----------|
   Dimm Bank 1   |  MemXR1  |   CS 1
                 |  MemYR1  |
                 |----------|
   Dimm Bank 2   |  MemXR2  |   CS 2
                 |  MemYR2  |
                 |----------|
   Dimm Bank 3   |  MemXR3  |   CS 3
                 |  MemYR3  |
                 ------------
                  DTR Card1

   The dimms can be 1 or 2 sided dimms.  The size and bankness is defined  
   separately for each dimm bank in the sh_[x,y,jnr]_dimm_cfg MMR register.

   Normally software bank 0 would map directly to physical dimm bank 0.  The 
   software banks can map to the different physical dimm banks via the 
   DIMM[0-3]_CS field in SH_[x,y,jnr]_DIMM_CFG for each dimm slot.   

   All the PROM's data structures (promlog variables, klconfig, etc.)
   track memory by the physical dimm bank number.  The kernel usually
   tracks memory by the software bank number.

 */


/* Preprocessor macros */
#define MD_MEM_BANKS		4
#define MD_PHYS_BANKS_PER_DIMM  2                  /* dimms may be 2 sided. */
#define MD_NUM_PHYS_BANKS       (MD_MEM_BANKS * MD_PHYS_BANKS_PER_DIMM)
#define MD_DIMMS_IN_SLOT	4  /* 4 dimms in each dimm bank.  aka slot */

/* Address bits 35,34 control dimm bank access. */
#define MD_BANK_SHFT       	34     
#define MD_BANK_MASK       	(UINT64_CAST 0x3 << MD_BANK_SHFT )
#define MD_BANK_GET(addr)  	(((addr) & MD_BANK_MASK) >> MD_BANK_SHFT)
#define MD_BANK_SIZE       	(UINT64_CAST 0x1 << MD_BANK_SHFT ) /* 16 gb */
#define MD_BANK_OFFSET(_b) 	(UINT64_CAST (_b) << MD_BANK_SHFT)

/*Address bit 12 selects side of dimm if 2bnk dimms present. */
#define MD_PHYS_BANK_SEL_SHFT   12
#define MD_PHYS_BANK_SEL_MASK   (UINT64_CAST 0x1 << MD_PHYS_BANK_SEL_SHFT)

/* Address bit 7 determines if data resides on X or Y memory system. 
 * If addr Bit 7 is set the data resides on Y memory system and
 * the corresponing directory entry reside on the X. 
 */
#define MD_X_OR_Y_SEL_SHFT	7	
#define MD_X_OR_Y_SEL_MASK	(1 << MD_X_OR_Y_SEL_SHFT)	

/* Address bit 8 determines which directory entry of the pair the address
 * corresponds to.  If addr Bit 8 is set DirB corresponds to the memory address.
 */
#define MD_DIRA_OR_DIRB_SEL_SHFT	8
#define MD_DIRA_OR_DIRB_SEL_MASK  	(1 << MD_DIRA_OR_DIRB_SEL_SHFT)	

/* Address bit 11 determines if corresponding directory entry resides 
 * on Left or Right memory bus.  If addr Bit 11 is set the corresponding 
 * directory entry resides on Right memory bus.
 */
#define MD_L_OR_R_SEL_SHFT	11
#define MD_L_OR_R_SEL_MASK	(1 << MD_L_OR_R_SEL_SHFT)	

/* DRAM sizes. */
#define MD_SZ_64_Mb		0x0
#define MD_SZ_128_Mb		0x1
#define MD_SZ_256_Mb		0x2
#define MD_SZ_512_Mb		0x3
#define MD_SZ_1024_Mb		0x4
#define MD_SZ_2048_Mb	 	0x5
#define MD_SZ_UNUSED		0x7

#define MD_DIMM_SIZE_BYTES(_size, _2bk) (				 \
		( (_size) == 7 ? 0 : ( 0x4000000L << (_size)) << (_2bk)))\

#define MD_DIMM_SIZE_MBYTES(_size, _2bk) (				 \
	 	( (_size) == 7 ? 0 : ( 0x40L << (_size) ) << (_2bk)))  	 \

/* The top 1/32 of each bank is directory memory, and not accessible
 * via normal reads and writes */
#define MD_DIMM_USER_SIZE(_size)	((_size) * 31 / 32)

/* Minimum size of a populated bank is 64M (62M usable) */
#define MIN_BANK_SIZE		MD_DIMM_USER_SIZE((64 * 0x100000))
#define MIN_BANK_STRING		"62"


/*Possible values for FREQ field in sh_[x,y,jnr]_dimm_cfg regs */
#define MD_DIMM_100_CL2_0 	0x0
#define MD_DIMM_133_CL2_0 	0x1
#define MD_DIMM_133_CL2_5 	0x2
#define MD_DIMM_160_CL2_0 	0x3
#define MD_DIMM_160_CL2_5 	0x4
#define MD_DIMM_160_CL3_0 	0x5
#define MD_DIMM_200_CL2_0 	0x6
#define MD_DIMM_200_CL2_5 	0x7
#define MD_DIMM_200_CL3_0 	0x8

/* DIMM_CFG fields */
#define MD_DIMM_SHFT(_dimm)	((_dimm) << 3)
#define MD_DIMM_SIZE_MASK(_dimm)					\
		(SH_JNR_DIMM_CFG_DIMM0_SIZE_MASK << 			\
		(MD_DIMM_SHFT(_dimm)))

#define MD_DIMM_2BK_MASK(_dimm)						\
		(SH_JNR_DIMM_CFG_DIMM0_2BK_MASK << 			\
		MD_DIMM_SHFT(_dimm))

#define MD_DIMM_REV_MASK(_dimm)						\
		(SH_JNR_DIMM_CFG_DIMM0_REV_MASK << 			\
		MD_DIMM_SHFT(_dimm))

#define MD_DIMM_CS_MASK(_dimm)						\
		(SH_JNR_DIMM_CFG_DIMM0_CS_MASK << 			\
		MD_DIMM_SHFT(_dimm))

#define MD_DIMM_SIZE(_dimm, _cfg)					\
		(((_cfg) & MD_DIMM_SIZE_MASK(_dimm))			\
		>> (MD_DIMM_SHFT(_dimm)+SH_JNR_DIMM_CFG_DIMM0_SIZE_SHFT))

#define MD_DIMM_TWO_SIDED(_dimm,_cfg)					\
		( ((_cfg) & MD_DIMM_2BK_MASK(_dimm))			\
		>> (MD_DIMM_SHFT(_dimm)+SH_JNR_DIMM_CFG_DIMM0_2BK_SHFT))

#define MD_DIMM_REVERSED(_dimm,_cfg) 					\
		(((_cfg) & MD_DIMM_REV_MASK(_dimm))			\
		>> (MD_DIMM_SHFT(_dimm)+SH_JNR_DIMM_CFG_DIMM0_REV_SHFT))

#define MD_DIMM_CS(_dimm,_cfg)						\
		(((_cfg) & MD_DIMM_CS_MASK(_dimm))			\
		>> (MD_DIMM_SHFT(_dimm)+SH_JNR_DIMM_CFG_DIMM0_CS_SHFT))



/* Macros to set MMRs that must be set identically to others. */
#define MD_SET_DIMM_CFG(_n, _value) {					\
		REMOTE_HUB_S(_n, SH_X_DIMM_CFG,_value);			\
                REMOTE_HUB_S(_n, SH_Y_DIMM_CFG, _value);		\
                REMOTE_HUB_S(_n, SH_JNR_DIMM_CFG, _value);}

#define MD_SET_DQCT_CFG(_n, _value) {					\
		REMOTE_HUB_S(_n, SH_X_DQCT_CFG,_value);			\
		REMOTE_HUB_S(_n, SH_Y_DQCT_CFG,_value); }

#define MD_SET_CFG(_n, _value) {					\
		REMOTE_HUB_S(_n, SH_X_CFG,_value);			\
		REMOTE_HUB_S(_n, SH_Y_CFG,_value);} 

#define MD_SET_REFRESH_CONTROL(_n, _value) {				\
		REMOTE_HUB_S(_n, SH_X_REFRESH_CONTROL, _value);		\
		REMOTE_HUB_S(_n, SH_Y_REFRESH_CONTROL, _value);}

#define MD_SET_DQ_MMR_DIR_COFIG(_n, _value) {				\
		REMOTE_HUB_S(_n, SH_MD_DQLP_MMR_DIR_CONFIG, _value);	\
                REMOTE_HUB_S(_n, SH_MD_DQRP_MMR_DIR_CONFIG, _value);}

#define MD_SET_PIOWD_DIR_ENTRYS(_n, _value) {				\
		REMOTE_HUB_S(_n, SH_MD_DQLP_MMR_PIOWD_DIR_ENTRY, _value);\
		REMOTE_HUB_S(_n, SH_MD_DQRP_MMR_PIOWD_DIR_ENTRY, _value);}

/* 
 * There are 12 Node Presence MMRs, 4 in each primary DQ and 4 in the
 * LB.  The data in the left and right DQ MMRs and the LB must match.
 */
#define MD_SET_PRESENT_VEC(_n, _vec, _value) {				   \
		REMOTE_HUB_S(_n, SH_MD_DQLP_MMR_DIR_PRESVEC0+((_vec)*0x10),\
			 _value);					   \
		REMOTE_HUB_S(_n, SH_MD_DQRP_MMR_DIR_PRESVEC0+((_vec)*0x10),\
			 _value);					   \
		REMOTE_HUB_S(_n, SH_SHUBS_PRESENT0+((_vec)*0x80), _value);}
/*
 * There are 16 Privilege Vector MMRs, 8 in each primary DQ.  The data
 * in the corresponding left and right DQ MMRs must match.  Each MMR
 * pair is used for a single partition.
 */
#define MD_SET_PRI_VEC(_n, _vec, _value) {				  \
		REMOTE_HUB_S(_n, SH_MD_DQLP_MMR_DIR_PRIVEC0+((_vec)*0x10),\
			 _value);					  \
		REMOTE_HUB_S(_n, SH_MD_DQRP_MMR_DIR_PRIVEC0+((_vec)*0x10),\
			 _value);}
/*
 * There are 16 Local/Remote MMRs, 8 in each primary DQ.  The data in
 * the corresponding left and right DQ MMRs must match.  Each MMR pair
 * is used for a single partition.
 */
#define MD_SET_LOC_VEC(_n, _vec, _value) {				\
		REMOTE_HUB_S(_n, SH_MD_DQLP_MMR_DIR_LOCVEC0+((_vec)*0x10),\
			 _value);					\
		REMOTE_HUB_S(_n, SH_MD_DQRP_MMR_DIR_LOCVEC0+((_vec)*0x10),\
			 _value);}

/* Memory BIST CMDS */
#define MD_DIMM_INIT_MODE_SET	0x0
#define MD_DIMM_INIT_REFRESH	0x1
#define MD_DIMM_INIT_PRECHARGE	0x2
#define MD_DIMM_INIT_BURST_TERM	0x6
#define MD_DIMM_INIT_NOP	0x7
#define MD_DIMM_BIST_READ	0x10
#define MD_FILL_DIR		0x20
#define MD_FILL_DATA		0x30
#define MD_FILL_DIR_ACCESS	0X40
#define MD_READ_DIR_PAIR	0x50
#define MD_READ_DIR_TAG		0x60

/* SH_MMRBIST_CTL macros */
#define MD_BIST_FAIL(_n) (REMOTE_HUB_L(_n, SH_MMRBIST_CTL) &		\
                SH_MMRBIST_CTL_FAIL_MASK)

#define MD_BIST_IN_PROGRESS(_n) (REMOTE_HUB_L(_n, SH_MMRBIST_CTL) & 	\
                SH_MMRBIST_CTL_IN_PROGRESS_MASK)

#define MD_BIST_MEM_IDLE(_n); (REMOTE_HUB_L(_n, SH_MMRBIST_CTL) & 	\
                SH_MMRBIST_CTL_MEM_IDLE_MASK)

/* SH_MMRBIST_ERR macros */
#define MD_BIST_MISCOMPARE(_n) (REMOTE_HUB_L(_n, SH_MMRBIST_ERR) &	\
		SH_MMRBIST_ERR_DETECTED_MASK)

#endif	/* _SHUB_MD_H */
