/*
 * USB device controllers have lots of quirks.  Use these macros in
 * gadget drivers or other code that needs to deal with them, and which
 * autoconfigures instead of using early binding to the hardware.
 *
 * This could eventually work like the ARM mach_is_*() stuff, driven by
 * some config file that gets updated as new hardware is supported.
 *
 * NOTE:  some of these controller drivers may not be available yet.
 */
#ifdef CONFIG_USB_GADGET_NET2280
#define	gadget_is_net2280(g)	!strcmp("net2280", (g)->name)
#else
#define	gadget_is_net2280(g)	0
#endif

#ifdef CONFIG_USB_GADGET_PXA
#define	gadget_is_pxa(g)	!strcmp("pxa2xx_udc", (g)->name)
#else
#define	gadget_is_pxa(g)	0
#endif

#ifdef CONFIG_USB_GADGET_GOKU
#define	gadget_is_goku(g)	!strcmp("goku_udc", (g)->name)
#else
#define	gadget_is_goku(g)	0
#endif

#ifdef CONFIG_USB_GADGET_SUPERH
#define	gadget_is_sh(g)		!strcmp("sh_udc", (g)->name)
#else
#define	gadget_is_sh(g)		0
#endif

#ifdef CONFIG_USB_GADGET_SA1100
#define	gadget_is_sa1100(g)	!strcmp("sa1100_udc", (g)->name)
#else
#define	gadget_is_sa1100(g)	0
#endif

#ifdef CONFIG_USB_GADGET_MQ11XX
#define	gadget_is_mq11xx(g)	!strcmp("mq11xx_udc", (g)->name)
#else
#define	gadget_is_mq11xx(g)	0
#endif

#ifdef CONFIG_USB_GADGET_OMAP
#define	gadget_is_omap(g)	!strcmp("omap_udc", (g)->name)
#else
#define	gadget_is_omap(g)	0
#endif

// CONFIG_USB_GADGET_AT91RM9200
// CONFIG_USB_GADGET_SX2
// CONFIG_USB_GADGET_AU1X00
// ...

