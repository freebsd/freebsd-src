/******************************************************************************
 * save_types.h
 *
 * Copyright (c) 2007 Isaku Yamahata <yamahata at valinux co jp>
 *                    VA Linux Systems Japan K.K.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __XEN_PUBLIC_HVM_SAVE_IA64_H__
#define __XEN_PUBLIC_HVM_SAVE_IA64_H__

#include "../../hvm/save.h"
#include "../../arch-ia64.h"

/* 
 * Save/restore header: general info about the save file. 
 */

/* x86 uses 0x54381286 */
#define HVM_FILE_MAGIC   0x343641492f6e6558UL   /* "Xen/IA64" */
#define HVM_FILE_VERSION 0x0000000000000001UL

struct hvm_save_header {
    uint64_t magic;             /* Must be HVM_FILE_MAGIC */
    uint64_t version;           /* File format version */
    uint64_t changeset;         /* Version of Xen that saved this file */
    uint64_t cpuid[5];          /* CPUID[0x01][%eax] on the saving machine */
};

DECLARE_HVM_SAVE_TYPE(HEADER, 1, struct hvm_save_header);

/*
 * CPU
 */
struct hvm_hw_ia64_cpu {
    uint64_t    ipsr;
};
DECLARE_HVM_SAVE_TYPE(CPU, 2, struct hvm_hw_ia64_cpu);

/*
 * CPU
 */
struct hvm_hw_ia64_vpd {
    struct vpd      vpd;
};
DECLARE_HVM_SAVE_TYPE(VPD, 3, struct hvm_hw_ia64_vpd);

/*
 * device dependency
 * vacpi => viosapic => vlsapic
 */
/*
 * vlsapic
 */
struct hvm_hw_ia64_vlsapic {
    uint64_t insvc[4];
    uint64_t vhpi; // ??? should this be saved in vpd
    uint8_t xtp;
    uint8_t pal_init_pending;
    uint8_t pad[2];
};
DECLARE_HVM_SAVE_TYPE(VLSAPIC, 4, struct hvm_hw_ia64_vlsapic);
/* set
 * unconditionaly set v->arch.irq_new_peding = 1 
 * unconditionaly set v->arch.irq_new_condition = 0
 */

/*
 * vtime
 */
/* itc, itm, itv are saved by arch vcpu context */
struct hvm_hw_ia64_vtime {
    uint64_t itc;
    uint64_t itm;

    uint64_t last_itc;
    uint64_t pending;
};
DECLARE_HVM_SAVE_TYPE(VTIME, 5, struct hvm_hw_ia64_vtime);
/*
 * calculate v->vtm.vtm_offset
 * ??? Or should vtm_offset be set by leave_hypervisor_tail()?
 * start vtm_timer if necessary by vtm_set_itm().
 * ??? Or should vtm_timer be set by leave_hypervisor_tail()?
 *
 * ??? or should be done by schedule_tail()
 *        => schedule_tail() should do.
 */

/*
 * viosapic
 */
#define VIOSAPIC_NUM_PINS     48

/* To share VT-d code which uses vioapic_redir_entry.
 * Although on ia64 this is for vsapic, but we have to vioapic_redir_entry
 * instead of viosapic_redir_entry.
 */
union vioapic_redir_entry
{
    uint64_t bits;
    struct {
        uint8_t vector;

        uint8_t delivery_mode  : 3;
        uint8_t reserve1       : 1;
        uint8_t delivery_status: 1;
        uint8_t polarity       : 1;
        uint8_t reserve2       : 1;
        uint8_t trig_mode      : 1;

        uint8_t mask           : 1;
        uint8_t reserve3       : 7;

        uint8_t reserved[3];
        uint16_t dest_id;
    } fields;
};

struct hvm_hw_ia64_viosapic {
    uint64_t    irr;
    uint64_t    isr;
    uint32_t    ioregsel;
    uint32_t    pad;
    uint64_t    lowest_vcpu_id;
    uint64_t    base_address;
    union vioapic_redir_entry  redirtbl[VIOSAPIC_NUM_PINS];
};
DECLARE_HVM_SAVE_TYPE(VIOSAPIC, 6, struct hvm_hw_ia64_viosapic);
  
/*
 * vacpi
 * PM timer
 */
struct vacpi_regs {
    union {
        struct {
            uint32_t pm1a_sts:16;/* PM1a_EVT_BLK.PM1a_STS: status register */
            uint32_t pm1a_en:16; /* PM1a_EVT_BLK.PM1a_EN: enable register */
        };
        uint32_t evt_blk;
    };
    uint32_t tmr_val;   /* PM_TMR_BLK.TMR_VAL: 32bit free-running counter */
};

struct hvm_hw_ia64_vacpi {
    struct vacpi_regs   regs;
};
DECLARE_HVM_SAVE_TYPE(VACPI, 7, struct hvm_hw_ia64_vacpi);
/* update last_gtime and setup timer of struct vacpi */

/*
 * opt_feature: identity mapping of region 4, 5 and 7.
 * With the c/s 16396:d2935f9c217f of xen-ia64-devel.hg,
 * opt_feature hypercall supports only region 4,5,7 identity mappings.
 * structure hvm_hw_ia64_identity_mappings only supports them.
 * The new structure, struct hvm_hw_ia64_identity_mappings, is created to
 * avoid to keep up with change of the xen/ia64 internal structure, struct
 * opt_feature.
 *
 * If it is enhanced in the future, new structure will be created.
 */
struct hvm_hw_ia64_identity_mapping {
    uint64_t on;        /* on/off */
    uint64_t pgprot;    /* The page protection bit mask of the pte. */
    uint64_t key;       /* A protection key. */
};

struct hvm_hw_ia64_identity_mappings {
    struct hvm_hw_ia64_identity_mapping im_reg4;/* Region 4 identity mapping */
    struct hvm_hw_ia64_identity_mapping im_reg5;/* Region 5 identity mapping */
    struct hvm_hw_ia64_identity_mapping im_reg7;/* Region 7 identity mapping */
};
DECLARE_HVM_SAVE_TYPE(OPT_FEATURE_IDENTITY_MAPPINGS, 8, struct hvm_hw_ia64_identity_mappings);

/* 
 * Largest type-code in use
 */
#define HVM_SAVE_CODE_MAX       8

#endif /* __XEN_PUBLIC_HVM_SAVE_IA64_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
