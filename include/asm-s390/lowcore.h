/*
 *  include/asm-s390/lowcore.h
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Hartmut Penner (hp@de.ibm.com),
 *               Martin Schwidefsky (schwidefsky@de.ibm.com),
 *               Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 */

#ifndef _ASM_S390_LOWCORE_H
#define _ASM_S390_LOWCORE_H

#define __LC_EXT_OLD_PSW                0x018
#define __LC_SVC_OLD_PSW                0x020
#define __LC_PGM_OLD_PSW                0x028
#define __LC_MCK_OLD_PSW                0x030
#define __LC_IO_OLD_PSW                 0x038
#define __LC_EXT_NEW_PSW                0x058
#define __LC_SVC_NEW_PSW                0x060
#define __LC_PGM_NEW_PSW                0x068
#define __LC_MCK_NEW_PSW                0x070
#define __LC_IO_NEW_PSW                 0x078
#define __LC_EXT_PARAMS                 0x080
#define __LC_CPU_ADDRESS                0x084
#define __LC_EXT_INT_CODE               0x086
#define __LC_SVC_INT_CODE               0x08B
#define __LC_PGM_ILC                    0x08C
#define __LC_PGM_INT_CODE               0x08E
#define __LC_TRANS_EXC_ADDR             0x090
#define __LC_SUBCHANNEL_ID              0x0B8
#define __LC_SUBCHANNEL_NR              0x0BA
#define __LC_IO_INT_PARM                0x0BC
#define __LC_IO_INT_WORD                0x0C0
#define __LC_MCCK_CODE                  0x0E8
#define __LC_AREGS_SAVE_AREA            0x120
#define __LC_CREGS_SAVE_AREA            0x1C0
#define __LC_RETURN_PSW                 0x200

#define __LC_SYNC_IO_WORD               0x400

#define __LC_SAVE_AREA                  0xC00
#define __LC_KERNEL_STACK               0xC40
#define __LC_ASYNC_STACK                0xC44
#define __LC_CPUID                      0xC60
#define __LC_CPUADDR                    0xC68
#define __LC_IPLDEV                     0xC7C

#define __LC_JIFFY_TIMER		0xC80

#define __LC_PANIC_MAGIC                0xE00

#define __LC_PFAULT_INTPARM             0x080

/* interrupt handler start with all io, external and mcck interrupt disabled */

#define _RESTART_PSW_MASK    0x00080000
#define _EXT_PSW_MASK        0x04080000
#define _PGM_PSW_MASK        0x04080000
#define _SVC_PSW_MASK        0x04080000
#define _MCCK_PSW_MASK       0x04080000
#define _IO_PSW_MASK         0x04080000
#define _USER_PSW_MASK       0x070DC000/* DAT, IO, EXT, Home-space         */
#define _WAIT_PSW_MASK       0x070E0000/* DAT, IO, EXT, Wait, Home-space   */
#define _DW_PSW_MASK         0x000A0000/* disabled wait PSW mask           */

#define _PRIMARY_MASK        0x0000    /* MASK for SACF                    */
#define _SECONDARY_MASK      0x0100    /* MASK for SACF                    */
#define _ACCESS_MASK         0x0200    /* MASK for SACF                    */
#define _HOME_MASK           0x0300    /* MASK for SACF                    */

#define _PSW_PRIM_SPACE_MODE 0x00000000
#define _PSW_SEC_SPACE_MODE  0x00008000
#define _PSW_ACC_REG_MODE    0x00004000
#define _PSW_HOME_SPACE_MODE 0x0000C000

#define _PSW_WAIT_MASK_BIT   0x00020000 /* Wait bit */
#define _PSW_IO_MASK_BIT     0x02000000 /* IO bit */
#define _PSW_IO_WAIT         0x02020000 /* IO & Wait bit */

/* we run in 31 Bit mode */
#define _ADDR_31             0x80000000

#ifndef __ASSEMBLY__

#include <linux/config.h>
#include <asm/processor.h>
#include <linux/types.h>
#include <asm/atomic.h>
#include <asm/sigp.h>

void restart_int_handler(void);
void ext_int_handler(void);
void system_call(void);
void pgm_check_handler(void);
void mcck_int_handler(void);
void io_int_handler(void);

struct _lowcore
{
        /* prefix area: defined by architecture */
	psw_t        restart_psw;              /* 0x000 */
	__u32        ccw2[4];                  /* 0x008 */
	psw_t        external_old_psw;         /* 0x018 */
	psw_t        svc_old_psw;              /* 0x020 */
	psw_t        program_old_psw;          /* 0x028 */
	psw_t        mcck_old_psw;             /* 0x030 */
	psw_t        io_old_psw;               /* 0x038 */
	__u8         pad1[0x58-0x40];          /* 0x040 */
	psw_t        external_new_psw;         /* 0x058 */
	psw_t        svc_new_psw;              /* 0x060 */
	psw_t        program_new_psw;          /* 0x068 */
	psw_t        mcck_new_psw;             /* 0x070 */
	psw_t        io_new_psw;               /* 0x078 */
	__u32        ext_params;               /* 0x080 */
	__u16        cpu_addr;                 /* 0x084 */
	__u16        ext_int_code;             /* 0x086 */
        __u16        svc_ilc;                  /* 0x088 */
        __u16        svc_code;                 /* 0x08a */
        __u16        pgm_ilc;                  /* 0x08c */
        __u16        pgm_code;                 /* 0x08e */
	__u32        trans_exc_code;           /* 0x090 */
	__u16        mon_class_num;            /* 0x094 */
	__u16        per_perc_atmid;           /* 0x096 */
	__u32        per_address;              /* 0x098 */
	__u32        monitor_code;             /* 0x09c */
	__u8         exc_access_id;            /* 0x0a0 */
	__u8         per_access_id;            /* 0x0a1 */
	__u8         pad2[0xB8-0xA2];          /* 0x0a2 */
	__u16        subchannel_id;            /* 0x0b8 */
	__u16        subchannel_nr;            /* 0x0ba */
	__u32        io_int_parm;              /* 0x0bc */
	__u32        io_int_word;              /* 0x0c0 */
        __u8         pad3[0xD8-0xC4];          /* 0x0c4 */
	__u32        cpu_timer_save_area[2];   /* 0x0d8 */
	__u32        clock_comp_save_area[2];  /* 0x0e0 */
	__u32        mcck_interruption_code[2]; /* 0x0e8 */
	__u8         pad4[0xf4-0xf0];          /* 0x0f0 */
	__u32        external_damage_code;     /* 0x0f4 */
	__u32        failing_storage_address;  /* 0x0f8 */
	__u8         pad5[0x100-0xfc];         /* 0x0fc */
	__u32        st_status_fixed_logout[4];/* 0x100 */
	__u8         pad6[0x120-0x110];        /* 0x110 */
	__u32        access_regs_save_area[16];/* 0x120 */
	__u32        floating_pt_save_area[8]; /* 0x160 */
	__u32        gpregs_save_area[16];     /* 0x180 */
	__u32        cregs_save_area[16];      /* 0x240 */	

        psw_t        return_psw;               /* 0x200 */
	__u8         pad8[0x400-0x208];        /* 0x208 */

	__u32        sync_io_word;	       /* 0x400 */

        __u8         pad9[0xc00-0x404];        /* 0x404 */

        /* System info area */
	__u32        save_area[16];            /* 0xc00 */
	__u32        kernel_stack;             /* 0xc40 */
	__u32        async_stack;              /* 0xc44 */
	/* entry.S sensitive area start */
	__u8         pad10[0xc60-0xc48];       /* 0xc5c */
	struct       cpuinfo_S390 cpu_data;    /* 0xc60 */
	__u32        ipl_device;               /* 0xc7c */
	/* entry.S sensitive area end */

        /* SMP info area: defined by DJB */
        __u64        jiffy_timer;              /* 0xc80 */
	atomic_t     ext_call_fast;            /* 0xc88 */
        __u8         pad11[0xe00-0xc8c];       /* 0xc8c */

        /* 0xe00 is used as indicator for dump tools */
        /* whether the kernel died with panic() or not */
        __u32        panic_magic;              /* 0xe00 */

        /* Align to the top 1k of prefix area */
	__u8         pad12[0x1000-0xe04];      /* 0xe04 */
} __attribute__((packed)); /* End structure*/

extern __inline__ void set_prefix(__u32 address)
{
        __asm__ __volatile__ ("spx %0" : : "m" (address) : "memory" );
}

#define S390_lowcore (*((struct _lowcore *) 0))
extern struct _lowcore *lowcore_ptr[];

#ifndef CONFIG_SMP
#define get_cpu_lowcore(cpu)      (&S390_lowcore)
#define safe_get_cpu_lowcore(cpu) (&S390_lowcore)
#else
#define get_cpu_lowcore(cpu)      (lowcore_ptr[(cpu)])
#define safe_get_cpu_lowcore(cpu) \
        ((cpu) == smp_processor_id() ? &S390_lowcore : lowcore_ptr[(cpu)])
#endif
#endif /* __ASSEMBLY__ */

#define __PANIC_MAGIC           0xDEADC0DE

#endif

