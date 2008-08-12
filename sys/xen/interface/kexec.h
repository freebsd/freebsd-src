/******************************************************************************
 * kexec.h - Public portion
 * 
 * Xen port written by:
 * - Simon 'Horms' Horman <horms@verge.net.au>
 * - Magnus Damm <magnus@valinux.co.jp>
 */

#ifndef _XEN_PUBLIC_KEXEC_H
#define _XEN_PUBLIC_KEXEC_H


/* This file describes the Kexec / Kdump hypercall interface for Xen.
 *
 * Kexec under vanilla Linux allows a user to reboot the physical machine 
 * into a new user-specified kernel. The Xen port extends this idea
 * to allow rebooting of the machine from dom0. When kexec for dom0
 * is used to reboot,  both the hypervisor and the domains get replaced
 * with some other kernel. It is possible to kexec between vanilla
 * Linux and Xen and back again. Xen to Xen works well too.
 *
 * The hypercall interface for kexec can be divided into three main
 * types of hypercall operations:
 *
 * 1) Range information:
 *    This is used by the dom0 kernel to ask the hypervisor about various 
 *    address information. This information is needed to allow kexec-tools 
 *    to fill in the ELF headers for /proc/vmcore properly.
 *
 * 2) Load and unload of images:
 *    There are no big surprises here, the kexec binary from kexec-tools
 *    runs in userspace in dom0. The tool loads/unloads data into the
 *    dom0 kernel such as new kernel, initramfs and hypervisor. When
 *    loaded the dom0 kernel performs a load hypercall operation, and
 *    before releasing all page references the dom0 kernel calls unload.
 *
 * 3) Kexec operation:
 *    This is used to start a previously loaded kernel.
 */

#include "xen.h"

#if defined(__i386__) || defined(__x86_64__)
#define KEXEC_XEN_NO_PAGES 17
#endif

/*
 * Prototype for this hypercall is:
 *  int kexec_op(int cmd, void *args)
 * @cmd  == KEXEC_CMD_... 
 *          KEXEC operation to perform
 * @args == Operation-specific extra arguments (NULL if none).
 */

/*
 * Kexec supports two types of operation:
 * - kexec into a regular kernel, very similar to a standard reboot
 *   - KEXEC_TYPE_DEFAULT is used to specify this type
 * - kexec into a special "crash kernel", aka kexec-on-panic
 *   - KEXEC_TYPE_CRASH is used to specify this type
 *   - parts of our system may be broken at kexec-on-panic time
 *     - the code should be kept as simple and self-contained as possible
 */

#define KEXEC_TYPE_DEFAULT 0
#define KEXEC_TYPE_CRASH   1


/* The kexec implementation for Xen allows the user to load two
 * types of kernels, KEXEC_TYPE_DEFAULT and KEXEC_TYPE_CRASH.
 * All data needed for a kexec reboot is kept in one xen_kexec_image_t
 * per "instance". The data mainly consists of machine address lists to pages
 * together with destination addresses. The data in xen_kexec_image_t
 * is passed to the "code page" which is one page of code that performs
 * the final relocations before jumping to the new kernel.
 */
 
typedef struct xen_kexec_image {
#if defined(__i386__) || defined(__x86_64__)
    unsigned long page_list[KEXEC_XEN_NO_PAGES];
#endif
    unsigned long indirection_page;
    unsigned long start_address;
} xen_kexec_image_t;

/*
 * Perform kexec having previously loaded a kexec or kdump kernel
 * as appropriate.
 * type == KEXEC_TYPE_DEFAULT or KEXEC_TYPE_CRASH [in]
 */
#define KEXEC_CMD_kexec                 0
typedef struct xen_kexec_exec {
    int type;
} xen_kexec_exec_t;

/*
 * Load/Unload kernel image for kexec or kdump.
 * type  == KEXEC_TYPE_DEFAULT or KEXEC_TYPE_CRASH [in]
 * image == relocation information for kexec (ignored for unload) [in]
 */
#define KEXEC_CMD_kexec_load            1
#define KEXEC_CMD_kexec_unload          2
typedef struct xen_kexec_load {
    int type;
    xen_kexec_image_t image;
} xen_kexec_load_t;

#define KEXEC_RANGE_MA_CRASH 0   /* machine address and size of crash area */
#define KEXEC_RANGE_MA_XEN   1   /* machine address and size of Xen itself */
#define KEXEC_RANGE_MA_CPU   2   /* machine address and size of a CPU note */

/*
 * Find the address and size of certain memory areas
 * range == KEXEC_RANGE_... [in]
 * nr    == physical CPU number (starting from 0) if KEXEC_RANGE_MA_CPU [in]
 * size  == number of bytes reserved in window [out]
 * start == address of the first byte in the window [out]
 */
#define KEXEC_CMD_kexec_get_range       3
typedef struct xen_kexec_range {
    int range;
    int nr;
    unsigned long size;
    unsigned long start;
} xen_kexec_range_t;

#endif /* _XEN_PUBLIC_KEXEC_H */

/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
