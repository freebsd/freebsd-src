/******************************************************************************
 * arch-x86/mca.h
 * 
 * Contributed by Advanced Micro Devices, Inc.
 * Author: Christoph Egger <Christoph.Egger@amd.com>
 *
 * Guest OS machine check interface to x86 Xen.
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

/* Full MCA functionality has the following Usecases from the guest side:
 *
 * Must have's:
 * 1. Dom0 and DomU register machine check trap callback handlers
 *    (already done via "set_trap_table" hypercall)
 * 2. Dom0 registers machine check event callback handler
 *    (doable via EVTCHNOP_bind_virq)
 * 3. Dom0 and DomU fetches machine check data
 * 4. Dom0 wants Xen to notify a DomU
 * 5. Dom0 gets DomU ID from physical address
 * 6. Dom0 wants Xen to kill DomU (already done for "xm destroy")
 *
 * Nice to have's:
 * 7. Dom0 wants Xen to deactivate a physical CPU
 *    This is better done as separate task, physical CPU hotplugging,
 *    and hypercall(s) should be sysctl's
 * 8. Page migration proposed from Xen NUMA work, where Dom0 can tell Xen to
 *    move a DomU (or Dom0 itself) away from a malicious page
 *    producing correctable errors.
 * 9. offlining physical page:
 *    Xen free's and never re-uses a certain physical page.
 * 10. Testfacility: Allow Dom0 to write values into machine check MSR's
 *     and tell Xen to trigger a machine check
 */

#ifndef __XEN_PUBLIC_ARCH_X86_MCA_H__
#define __XEN_PUBLIC_ARCH_X86_MCA_H__

/* Hypercall */
#define __HYPERVISOR_mca __HYPERVISOR_arch_0

#define XEN_MCA_INTERFACE_VERSION 0x03000001

/* IN: Dom0 calls hypercall from MC event handler. */
#define XEN_MC_CORRECTABLE  0x0
/* IN: Dom0/DomU calls hypercall from MC trap handler. */
#define XEN_MC_TRAP         0x1
/* XEN_MC_CORRECTABLE and XEN_MC_TRAP are mutually exclusive. */

/* OUT: All is ok */
#define XEN_MC_OK           0x0
/* OUT: Domain could not fetch data. */
#define XEN_MC_FETCHFAILED  0x1
/* OUT: There was no machine check data to fetch. */
#define XEN_MC_NODATA       0x2
/* OUT: Between notification time and this hypercall an other
 *  (most likely) correctable error happened. The fetched data,
 *  does not match the original machine check data. */
#define XEN_MC_NOMATCH      0x4

/* OUT: DomU did not register MC NMI handler. Try something else. */
#define XEN_MC_CANNOTHANDLE 0x8
/* OUT: Notifying DomU failed. Retry later or try something else. */
#define XEN_MC_NOTDELIVERED 0x10
/* Note, XEN_MC_CANNOTHANDLE and XEN_MC_NOTDELIVERED are mutually exclusive. */


#ifndef __ASSEMBLY__

#define VIRQ_MCA VIRQ_ARCH_0 /* G. (DOM0) Machine Check Architecture */

/*
 * Machine Check Architecure:
 * structs are read-only and used to report all kinds of
 * correctable and uncorrectable errors detected by the HW.
 * Dom0 and DomU: register a handler to get notified.
 * Dom0 only: Correctable errors are reported via VIRQ_MCA
 * Dom0 and DomU: Uncorrectable errors are reported via nmi handlers
 */
#define MC_TYPE_GLOBAL          0
#define MC_TYPE_BANK            1
#define MC_TYPE_EXTENDED        2

struct mcinfo_common {
    uint16_t type;      /* structure type */
    uint16_t size;      /* size of this struct in bytes */
};


#define MC_FLAG_CORRECTABLE     (1 << 0)
#define MC_FLAG_UNCORRECTABLE   (1 << 1)

/* contains global x86 mc information */
struct mcinfo_global {
    struct mcinfo_common common;

    /* running domain at the time in error (most likely the impacted one) */
    uint16_t mc_domid;
    uint32_t mc_socketid; /* physical socket of the physical core */
    uint16_t mc_coreid; /* physical impacted core */
    uint16_t mc_core_threadid; /* core thread of physical core */
    uint16_t mc_vcpuid; /* virtual cpu scheduled for mc_domid */
    uint64_t mc_gstatus; /* global status */
    uint32_t mc_flags;
};

/* contains bank local x86 mc information */
struct mcinfo_bank {
    struct mcinfo_common common;

    uint16_t mc_bank; /* bank nr */
    uint16_t mc_domid; /* Usecase 5: domain referenced by mc_addr on dom0
                        * and if mc_addr is valid. Never valid on DomU. */
    uint64_t mc_status; /* bank status */
    uint64_t mc_addr;   /* bank address, only valid
                         * if addr bit is set in mc_status */
    uint64_t mc_misc;
};


struct mcinfo_msr {
    uint64_t reg;   /* MSR */
    uint64_t value; /* MSR value */
};

/* contains mc information from other
 * or additional mc MSRs */ 
struct mcinfo_extended {
    struct mcinfo_common common;

    /* You can fill up to five registers.
     * If you need more, then use this structure
     * multiple times. */

    uint32_t mc_msrs; /* Number of msr with valid values. */
    struct mcinfo_msr mc_msr[5];
};

#define MCINFO_HYPERCALLSIZE	1024
#define MCINFO_MAXSIZE		768

struct mc_info {
    /* Number of mcinfo_* entries in mi_data */
    uint32_t mi_nentries;

    uint8_t mi_data[MCINFO_MAXSIZE - sizeof(uint32_t)];
};
typedef struct mc_info mc_info_t;



/* 
 * OS's should use these instead of writing their own lookup function
 * each with its own bugs and drawbacks.
 * We use macros instead of static inline functions to allow guests
 * to include this header in assembly files (*.S).
 */
/* Prototype:
 *    uint32_t x86_mcinfo_nentries(struct mc_info *mi);
 */
#define x86_mcinfo_nentries(_mi)    \
    (_mi)->mi_nentries
/* Prototype:
 *    struct mcinfo_common *x86_mcinfo_first(struct mc_info *mi);
 */
#define x86_mcinfo_first(_mi)       \
    (struct mcinfo_common *)((_mi)->mi_data)
/* Prototype:
 *    struct mcinfo_common *x86_mcinfo_next(struct mcinfo_common *mic);
 */
#define x86_mcinfo_next(_mic)       \
    (struct mcinfo_common *)((uint8_t *)(_mic) + (_mic)->size)

/* Prototype:
 *    void x86_mcinfo_lookup(void *ret, struct mc_info *mi, uint16_t type);
 */
#define x86_mcinfo_lookup(_ret, _mi, _type)    \
    do {                                                        \
        uint32_t found, i;                                      \
        struct mcinfo_common *_mic;                             \
                                                                \
        found = 0;                                              \
	(_ret) = NULL;						\
	if (_mi == NULL) break;					\
        _mic = x86_mcinfo_first(_mi);                           \
        for (i = 0; i < x86_mcinfo_nentries(_mi); i++) {        \
            if (_mic->type == (_type)) {                        \
                found = 1;                                      \
                break;                                          \
            }                                                   \
            _mic = x86_mcinfo_next(_mic);                       \
        }                                                       \
        (_ret) = found ? _mic : NULL;                           \
    } while (0)


/* Usecase 1
 * Register machine check trap callback handler
 *    (already done via "set_trap_table" hypercall)
 */

/* Usecase 2
 * Dom0 registers machine check event callback handler
 * done by EVTCHNOP_bind_virq
 */

/* Usecase 3
 * Fetch machine check data from hypervisor.
 * Note, this hypercall is special, because both Dom0 and DomU must use this.
 */
#define XEN_MC_fetch            1
struct xen_mc_fetch {
    /* IN/OUT variables. */
    uint32_t flags;

/* IN: XEN_MC_CORRECTABLE, XEN_MC_TRAP */
/* OUT: XEN_MC_OK, XEN_MC_FETCHFAILED, XEN_MC_NODATA, XEN_MC_NOMATCH */

    /* OUT variables. */
    uint32_t fetch_idx;  /* only useful for Dom0 for the notify hypercall */
    struct mc_info mc_info;
};
typedef struct xen_mc_fetch xen_mc_fetch_t;
DEFINE_XEN_GUEST_HANDLE(xen_mc_fetch_t);


/* Usecase 4
 * This tells the hypervisor to notify a DomU about the machine check error
 */
#define XEN_MC_notifydomain     2
struct xen_mc_notifydomain {
    /* IN variables. */
    uint16_t mc_domid;    /* The unprivileged domain to notify. */
    uint16_t mc_vcpuid;   /* The vcpu in mc_domid to notify.
                           * Usually echo'd value from the fetch hypercall. */
    uint32_t fetch_idx;   /* echo'd value from the fetch hypercall. */

    /* IN/OUT variables. */
    uint32_t flags;

/* IN: XEN_MC_CORRECTABLE, XEN_MC_TRAP */
/* OUT: XEN_MC_OK, XEN_MC_CANNOTHANDLE, XEN_MC_NOTDELIVERED, XEN_MC_NOMATCH */
};
typedef struct xen_mc_notifydomain xen_mc_notifydomain_t;
DEFINE_XEN_GUEST_HANDLE(xen_mc_notifydomain_t);


struct xen_mc {
    uint32_t cmd;
    uint32_t interface_version; /* XEN_MCA_INTERFACE_VERSION */
    union {
        struct xen_mc_fetch        mc_fetch;
        struct xen_mc_notifydomain mc_notifydomain;
        uint8_t pad[MCINFO_HYPERCALLSIZE];
    } u;
};
typedef struct xen_mc xen_mc_t;
DEFINE_XEN_GUEST_HANDLE(xen_mc_t);

#endif /* __ASSEMBLY__ */

#endif /* __XEN_PUBLIC_ARCH_X86_MCA_H__ */
