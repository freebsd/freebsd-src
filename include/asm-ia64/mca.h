/*
 * File:	mca.h
 * Purpose:	Machine check handling specific defines
 *
 * Copyright (C) 1999, 2004 Silicon Graphics, Inc.
 * Copyright (C) Vijay Chander (vijay@engr.sgi.com)
 * Copyright (C) Srinivasa Thirumalachar (sprasad@engr.sgi.com)
 */

#ifndef _ASM_IA64_MCA_H
#define _ASM_IA64_MCA_H

#if !defined(__ASSEMBLY__)
#include <linux/types.h>
#include <asm/param.h>
#include <asm/sal.h>
#include <asm/processor.h>
#include <asm/mca_asm.h>

#define IA64_MCA_RENDEZ_TIMEOUT		(20 * 1000)	/* value in milliseconds - 20 seconds */

typedef union cmcv_reg_u {
	u64	cmcv_regval;
	struct	{
		u64	cmcr_vector		: 8;
		u64	cmcr_reserved1		: 4;
		u64	cmcr_ignored1		: 1;
		u64	cmcr_reserved2		: 3;
		u64	cmcr_mask		: 1;
		u64	cmcr_ignored2		: 47;
	} cmcv_reg_s;

} cmcv_reg_t;

#define cmcv_mask		cmcv_reg_s.cmcr_mask
#define cmcv_vector		cmcv_reg_s.cmcr_vector

enum {
	IA64_MCA_RENDEZ_CHECKIN_NOTDONE	=	0x0,
	IA64_MCA_RENDEZ_CHECKIN_DONE	=	0x1
};

/* the following data structure is used for TLB error recovery purposes */
extern struct ia64_mca_tlb_info {
	u64	cr_lid;
	u64	percpu_paddr;
	u64	ptce_base;
	u32	ptce_count[2];
	u32	ptce_stride[2];
	u64	pal_paddr;
	u64	pal_base;
} ia64_mca_tlb_list[NR_CPUS];

/* Information maintained by the MC infrastructure */
typedef struct ia64_mc_info_s {
	u64		imi_mca_handler;
	size_t		imi_mca_handler_size;
	u64		imi_monarch_init_handler;
	size_t		imi_monarch_init_handler_size;
	u64		imi_slave_init_handler;
	size_t		imi_slave_init_handler_size;
	u8		imi_rendez_checkin[NR_CPUS];

} ia64_mc_info_t;

typedef struct ia64_mca_sal_to_os_state_s {
	u64		imsto_os_gp;		/* GP of the os registered with the SAL */
	u64		imsto_pal_proc;		/* PAL_PROC entry point - physical addr */
	u64		imsto_sal_proc;		/* SAL_PROC entry point - physical addr */
	u64		imsto_sal_gp;		/* GP of the SAL - physical */
	u64		imsto_rendez_state;	/* Rendez state information */
	u64		imsto_sal_check_ra;	/* Return address in SAL_CHECK while going
						 * back to SAL from OS after MCA handling.
						 */
	u64		pal_min_state;		/* from PAL in r17 */
	u64		proc_state_param;	/* from PAL in r18. See SDV 2:268 11.3.2.1 */
} ia64_mca_sal_to_os_state_t;

enum {
	IA64_MCA_CORRECTED	=	0x0,	/* Error has been corrected by OS_MCA */
	IA64_MCA_WARM_BOOT	=	-1,	/* Warm boot of the system need from SAL */
	IA64_MCA_COLD_BOOT	=	-2,	/* Cold boot of the system need from SAL */
	IA64_MCA_HALT		=	-3	/* System to be halted by SAL */
};

enum {
	IA64_MCA_SAME_CONTEXT	=	0x0,	/* SAL to return to same context */
	IA64_MCA_NEW_CONTEXT	=	-1	/* SAL to return to new context */
};

typedef struct ia64_mca_os_to_sal_state_s {
	u64		imots_os_status;	/*   OS status to SAL as to what happened
						 *   with the MCA handling.
						 */
	u64		imots_sal_gp;		/* GP of the SAL - physical */
	u64		imots_context;		/* 0 if return to same context
						   1 if return to new context */
	u64		*imots_new_min_state;	/* Pointer to structure containing
						 * new values of registers in the min state
						 * save area.
						 */
	u64		imots_sal_check_ra;	/* Return address in SAL_CHECK while going
						 * back to SAL from OS after MCA handling.
						 */
} ia64_mca_os_to_sal_state_t;

extern void ia64_mca_init(void);
extern void ia64_os_mca_dispatch(void);
extern void ia64_os_mca_dispatch_end(void);
extern void ia64_mca_ucmc_handler(void);
extern void ia64_monarch_init_handler(void);
extern void ia64_slave_init_handler(void);
extern void ia64_mca_cmc_vector_setup(void);

#endif /* !__ASSEMBLY__ */
#endif /* _ASM_IA64_MCA_H */
