/*
 * acm_ops.h: Xen access control module hypervisor commands
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
 *
 * Reiner Sailer <sailer@watson.ibm.com>
 * Copyright (c) 2005,2006 International Business Machines Corporation.
 */

#ifndef __XEN_PUBLIC_ACM_OPS_H__
#define __XEN_PUBLIC_ACM_OPS_H__

#include "xen.h"
#include "acm.h"

/*
 * Make sure you increment the interface version whenever you modify this file!
 * This makes sure that old versions of acm tools will stop working in a
 * well-defined way (rather than crashing the machine, for instance).
 */
#define ACM_INTERFACE_VERSION   0xAAAA000A

/************************************************************************/

/*
 * Prototype for this hypercall is:
 *  int acm_op(int cmd, void *args)
 * @cmd  == ACMOP_??? (access control module operation).
 * @args == Operation-specific extra arguments (NULL if none).
 */


#define ACMOP_setpolicy         1
struct acm_setpolicy {
    /* IN */
    XEN_GUEST_HANDLE_64(void) pushcache;
    uint32_t pushcache_size;
};


#define ACMOP_getpolicy         2
struct acm_getpolicy {
    /* IN */
    XEN_GUEST_HANDLE_64(void) pullcache;
    uint32_t pullcache_size;
};


#define ACMOP_dumpstats         3
struct acm_dumpstats {
    /* IN */
    XEN_GUEST_HANDLE_64(void) pullcache;
    uint32_t pullcache_size;
};


#define ACMOP_getssid           4
#define ACM_GETBY_ssidref  1
#define ACM_GETBY_domainid 2
struct acm_getssid {
    /* IN */
    uint32_t get_ssid_by; /* ACM_GETBY_* */
    union {
        domaintype_t domainid;
        ssidref_t    ssidref;
    } id;
    XEN_GUEST_HANDLE_64(void) ssidbuf;
    uint32_t ssidbuf_size;
};

#define ACMOP_getdecision      5
struct acm_getdecision {
    /* IN */
    uint32_t get_decision_by1; /* ACM_GETBY_* */
    uint32_t get_decision_by2; /* ACM_GETBY_* */
    union {
        domaintype_t domainid;
        ssidref_t    ssidref;
    } id1;
    union {
        domaintype_t domainid;
        ssidref_t    ssidref;
    } id2;
    uint32_t hook;
    /* OUT */
    uint32_t acm_decision;
};


#define ACMOP_chgpolicy        6
struct acm_change_policy {
    /* IN */
    XEN_GUEST_HANDLE_64(void) policy_pushcache;
    uint32_t policy_pushcache_size;
    XEN_GUEST_HANDLE_64(void) del_array;
    uint32_t delarray_size;
    XEN_GUEST_HANDLE_64(void) chg_array;
    uint32_t chgarray_size;
    /* OUT */
    /* array with error code */
    XEN_GUEST_HANDLE_64(void) err_array;
    uint32_t errarray_size;
};

#define ACMOP_relabeldoms       7
struct acm_relabel_doms {
    /* IN */
    XEN_GUEST_HANDLE_64(void) relabel_map;
    uint32_t relabel_map_size;
    /* OUT */
    XEN_GUEST_HANDLE_64(void) err_array;
    uint32_t errarray_size;
};

/* future interface to Xen */
struct xen_acmctl {
    uint32_t cmd;
    uint32_t interface_version;
    union {
        struct acm_setpolicy     setpolicy;
        struct acm_getpolicy     getpolicy;
        struct acm_dumpstats     dumpstats;
        struct acm_getssid       getssid;
        struct acm_getdecision   getdecision;
        struct acm_change_policy change_policy;
        struct acm_relabel_doms  relabel_doms;
    } u;
};

typedef struct xen_acmctl xen_acmctl_t;
DEFINE_XEN_GUEST_HANDLE(xen_acmctl_t);

#endif /* __XEN_PUBLIC_ACM_OPS_H__ */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
