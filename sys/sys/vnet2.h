
#ifndef _SYS_VNET2_H_
#define _SYS_VNET2_H_

/*
 * These two virtual network stack allocator definitions are also required
 * for libkvm so that it can evaluate virtualized global variables.
 */
#define VNET_SETNAME            "set_vnet"
#define VNET_SYMPREFIX          "vnet_entry_"

#if defined(_KERNEL) || defined(_WANT_VNET)

#include <sys/queue.h>

struct vnet {
        LIST_ENTRY(vnet)         vnet_le;       /* all vnets list */
        u_int                    vnet_magic_n;
        u_int                    vnet_ifcnt;
        u_int                    vnet_sockcnt;
        u_int                    vnet_vps_flags; /* flags used by VPS */
        void                    *vnet_data_mem;
        uintptr_t                vnet_data_base;
};
#define VNET_MAGIC_N    0x3e0d8f29

#ifdef VIMAGE

/*
 * Location of the kernel's 'set_vnet' linker set.
 */

extern uintptr_t        *__start_set_vnet;
__GLOBL(__start_set_vnet);
extern uintptr_t        *__stop_set_vnet;
__GLOBL(__stop_set_vnet);

#define VNET_START      (uintptr_t)&__start_set_vnet
#define VNET_STOP       (uintptr_t)&__stop_set_vnet

/*
 * Virtual network stack memory allocator, which allows global variables to
 * be automatically instantiated for each network stack instance.
 */
#define VNET_NAME(n)            vnet_entry_##n
#define VNET_DECLARE(t, n)      extern t VNET_NAME(n)
#define VNET_DEFINE(t, n)       t VNET_NAME(n) __section(VNET_SETNAME) __used
#define _VNET_PTR(b, n)         (__typeof(VNET_NAME(n))*)               \
                                    ((b) + (uintptr_t)&VNET_NAME(n))

#define _VNET(b, n)             (*_VNET_PTR(b, n))

/*
 * Virtualized global variable accessor macros.
 */
#define VNET_VNET_PTR(vnet, n)          _VNET_PTR((vnet)->vnet_data_base, n)
#define VNET_VNET(vnet, n)              (*VNET_VNET_PTR((vnet), n))

#define VNET_PTR(n)             VNET_VNET_PTR(curvnet, n)
#define VNET(n)                 VNET_VNET(curvnet, n)

#else /* !VIMAGE */


/*
 * Versions of the VNET macros that compile to normal global variables and
 * standard sysctl definitions.
 */
#define VNET_NAME(n)            n
#define VNET_DECLARE(t, n)      extern t n
#define VNET_DEFINE(t, n)       t n
#define _VNET_PTR(b, n)         &VNET_NAME(n)

/*
 * Virtualized global variable accessor macros.
 */
#define VNET_VNET_PTR(vnet, n)          (&(n))
#define VNET_VNET(vnet, n)              (n)

#define VNET_PTR(n)             (&(n))
#define VNET(n)                 (n)


#endif /* VIMAGE */
#endif /* _KERNEL || _WANT_VNET */

#endif /* !_SYS_VNET2_H_ */

