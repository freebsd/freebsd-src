/*-
 * Copyright (c) 1999 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
 * Copyright (c) 1999 Takanori Watanabe <takawata@jp.FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#if defined(__NetBSD__)
# include <sys/device.h>
#elif defined(__FreeBSD__)
# if __FreeBSD_version >= 400001
#  include <sys/module.h>
#  include <sys/bus.h>
# else
#  include <sys/device.h>
# endif
#endif

/*
 * Macro's to cope with the differences between operating systems and versions. 
 */

#if defined(__NetBSD__)
# define DEVPORT_DEVICE			struct device
# define DEVPORT_DEVNAME(dev)		(dev).dv_xname
# define DEVPORT_DEVUNIT(dev)		(dev).dv_unit

#elif defined(__FreeBSD__)
/*
 * FreeBSD (compatibility for struct device)
 */
#if __FreeBSD_version >= 400001
# define DEVPORT_DEVICE			device_t
# define DEVPORT_DEVNAME(dev)		device_get_name(dev)
# define DEVPORT_DEVUNIT(dev)		device_get_unit(dev)
# define DEVPORT_ALLOC_SOFTC(dev)       device_get_softc(dev)
# define DEVPORT_GET_SOFTC(dev)         device_get_softc(dev)

# define UNCONF	1		/* print " not configured\n" */

#else

# define DEVPORT_DEVICE			struct device
# define DEVPORT_DEVNAME(dev)		(dev).dv_xname
# define DEVPORT_DEVUNIT(dev)		(dev).dv_unit
# ifdef DEVPORT_ALLOCSOFTCFUNC
#  define DEVPORT_ALLOC_SOFTC(dev)	(DEVPORT_ALLOCSOFTCFUNC)((dev).dv_unit)
# else
#  define DEVPORT_ALLOC_SOFTC(dev)	DEVPORT_ALLOCSOFTCFUNC_is_not_defined_prior_than_device_port_h
# endif
# ifdef DEVPORT_SOFTCARRAY
#  define DEVPORT_GET_SOFTC(dev)	(DEVPORT_SOFTCARRAY)[(dev).dv_unit]
# else
#  define DEVPORT_GET_SOFTC(dev)	DEVPORT_SOFTCARRAY_is_not_defined_prior_than_device_port_h
# endif

#endif

/*
 * PC-Card device driver (compatibility for struct pccard_devinfo *)
 */
#if __FreeBSD_version >= 400001
# define DEVPORT_PDEVICE		device_t
# define DEVPORT_PDEVUNIT(pdev)		device_get_unit(pdev)
# define DEVPORT_PDEVFLAGS(pdev)	device_get_flags(pdev)
# define DEVPORT_PDEVIOBASE(pdev)	bus_get_resource_start(pdev, SYS_RES_IOPORT, 0)
# define DEVPORT_PDEVIRQ(pdev)		bus_get_resource_start(pdev, SYS_RES_IRQ, 0)
# define DEVPORT_PDEVMADDR(pdev)	bus_get_resource_start(pdev, SYS_RES_MEMORY, 0)
# define DEVPORT_PDEVALLOC_SOFTC(pdev)	device_get_softc(pdev)
# define DEVPORT_PDEVGET_SOFTC(pdev)	device_get_softc(pdev)

#else

# define DEVPORT_PDEVICE		struct pccard_devinfo *
# define DEVPORT_PDEVUNIT(pdev)		(pdev)->pd_unit
# define DEVPORT_PDEVFLAGS(pdev)	(pdev)->pd_flags
# define DEVPORT_PDEVIOBASE(pdev)	(pdev)->pd_iobase
# define DEVPORT_PDEVIRQ(pdev)		(pdev)->pd_irq
# define DEVPORT_PDEVMADDR(pdev)	(pdev)->pd_maddr
# ifdef DEVPORT_ALLOCSOFTCFUNC
#  define DEVPORT_PDEVALLOC_SOFTC(pdev)	(DEVPORT_ALLOCSOFTCFUNC)((pdev)->pd_unit)
# else
#  define DEVPORT_PDEVALLOC_SOFTC(pdev)	DEVPORT_ALLOCSOFTCFUNC_is_not_defined_prior_than_device_port_h
# endif
# ifdef DEVPORT_SOFTCARRAY
#  define DEVPORT_PDEVGET_SOFTC(pdev)	(DEVPORT_SOFTCARRAY)[(pdev)->pd_unit]
# else
#  define DEVPORT_PDEVGET_SOFTC(pdev)	DEVPORT_SOFTCARRAY_is_not_defined_prior_than_device_port_h
# endif
#endif

#endif /* __FreeBSD__ */

