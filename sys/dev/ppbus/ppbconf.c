/*-
 * Copyright (c) 1997, 1998, 1999 Nicolas Souchu
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
 *
 */
#include "opt_ppb_1284.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/ppb_1284.h>

#include "ppbus_if.h"
  
#define DEVTOSOFTC(dev) ((struct ppb_data *)device_get_softc(dev))
  
MALLOC_DEFINE(M_PPBUSDEV, "ppbusdev", "Parallel Port bus device");


/*
 * Device methods
 */

static void
ppbus_print_child(device_t bus, device_t dev)
{
	struct ppb_device *ppbdev;

	bus_print_child_header(bus, dev);

	ppbdev = (struct ppb_device *)device_get_ivars(dev);

	if (ppbdev->flags != 0)
		printf(" flags 0x%x", ppbdev->flags);

	printf(" on %s%d\n", device_get_name(bus), device_get_unit(bus));

	return;
}

static int
ppbus_probe(device_t dev)
{
	device_set_desc(dev, "Parallel port bus");

	return (0);
}

/*
 * ppbus_add_child()
 *
 * Add a ppbus device, allocate/initialize the ivars
 */
static device_t
ppbus_add_child(device_t dev, int order, const char *name, int unit)
{
	struct ppb_device *ppbdev;
	device_t child;
        
	/* allocate ivars for the new ppbus child */
	ppbdev = malloc(sizeof(struct ppb_device), M_PPBUSDEV, M_NOWAIT);
	if (!ppbdev)
		return NULL;
	bzero(ppbdev, sizeof *ppbdev);

	/* initialize the ivars */
	ppbdev->name = name;

	/* add the device as a child to the ppbus bus with the allocated
	 * ivars */
	child = device_add_child_ordered(dev, order, name, unit);
	device_set_ivars(child, ppbdev);

	return child;
}

static int
ppbus_read_ivar(device_t bus, device_t dev, int index, uintptr_t* val)
{
	struct ppb_device *ppbdev = (struct ppb_device *)device_get_ivars(dev);
  
	switch (index) {
	case PPBUS_IVAR_MODE:
		/* XXX yet device mode = ppbus mode = chipset mode */
		*val = (u_long)ppb_get_mode(bus);
		ppbdev->mode = (u_short)*val;
		break;
	case PPBUS_IVAR_AVM:
		*val = (u_long)ppbdev->avm;
		break;
	case PPBUS_IVAR_IRQ:
		BUS_READ_IVAR(device_get_parent(bus), bus, PPC_IVAR_IRQ, val);
		break;
	default:
		return (ENOENT);
	}
  
	return (0);
}
  
static int
ppbus_write_ivar(device_t bus, device_t dev, int index, u_long val)
{
	struct ppb_device *ppbdev = (struct ppb_device *)device_get_ivars(dev);

	switch (index) {
	case PPBUS_IVAR_MODE:
		/* XXX yet device mode = ppbus mode = chipset mode */
		ppb_set_mode(bus,val);
		ppbdev->mode = ppb_get_mode(bus);
		break;
	default:
		return (ENOENT);
  	}

	return (0);
  }

#define PPB_PNP_PRINTER		0
#define PPB_PNP_MODEM		1
#define PPB_PNP_NET		2
#define PPB_PNP_HDC		3
#define PPB_PNP_PCMCIA		4
#define PPB_PNP_MEDIA		5
#define PPB_PNP_FDC		6
#define PPB_PNP_PORTS		7
#define PPB_PNP_SCANNER		8
#define PPB_PNP_DIGICAM		9

#ifndef DONTPROBE_1284

static char *pnp_tokens[] = {
	"PRINTER", "MODEM", "NET", "HDC", "PCMCIA", "MEDIA",
	"FDC", "PORTS", "SCANNER", "DIGICAM", "", NULL };

#if 0
static char *pnp_classes[] = {
	"printer", "modem", "network device",
	"hard disk", "PCMCIA", "multimedia device",
	"floppy disk", "ports", "scanner",
	"digital camera", "unknown device", NULL };
#endif

/*
 * search_token()
 *
 * Search the first occurence of a token within a string
 *
 * XXX should use strxxx() calls
 */
static char *
search_token(char *str, int slen, char *token)
{
	char *p;
	int tlen, i, j;

#define UNKNOWN_LENGTH	-1

	if (slen == UNKNOWN_LENGTH)
		/* get string's length */
		for (slen = 0, p = str; *p != '\0'; p++)
			slen ++;

	/* get token's length */
	for (tlen = 0, p = token; *p != '\0'; p++)
		tlen ++;

	if (tlen == 0)
		return (str);

	for (i = 0; i <= slen-tlen; i++) {
		for (j = 0; j < tlen; j++)
			if (str[i+j] != token[j])
				break;
		if (j == tlen)
			return (&str[i]);
	}

	return (NULL);
}

/*
 * ppb_pnp_detect()
 *
 * Returns the class id. of the peripherial, -1 otherwise
 */
static int
ppb_pnp_detect(device_t bus)
{
	char *token, *class = 0;
	int i, len, error;
	int class_id = -1;
	char str[PPB_PnP_STRING_SIZE+1];
	int unit = device_get_unit(bus);

	printf("Probing for PnP devices on ppbus%d:\n", unit);
	
	if ((error = ppb_1284_read_id(bus, PPB_NIBBLE, str,
					PPB_PnP_STRING_SIZE, &len)))
		goto end_detect;

#ifdef DEBUG_1284
	printf("ppb: <PnP> %d characters: ", len);
	for (i = 0; i < len; i++)
		printf("%c(0x%x) ", str[i], str[i]);
	printf("\n");
#endif

	/* replace ';' characters by '\0' */
	for (i = 0; i < len; i++)
		str[i] = (str[i] == ';') ? '\0' : str[i];

	if ((token = search_token(str, len, "MFG")) != NULL ||
		(token = search_token(str, len, "MANUFACTURER")) != NULL)
		printf("ppbus%d: <%s", unit,
			search_token(token, UNKNOWN_LENGTH, ":") + 1);
	else
		printf("ppbus%d: <unknown", unit);

	if ((token = search_token(str, len, "MDL")) != NULL ||
		(token = search_token(str, len, "MODEL")) != NULL)
		printf(" %s",
			search_token(token, UNKNOWN_LENGTH, ":") + 1);
	else
		printf(" unknown");

	if ((token = search_token(str, len, "VER")) != NULL)
		printf("/%s",
			search_token(token, UNKNOWN_LENGTH, ":") + 1);

	if ((token = search_token(str, len, "REV")) != NULL)
		printf(".%s",
			search_token(token, UNKNOWN_LENGTH, ":") + 1);

	printf(">");

	if ((token = search_token(str, len, "CLS")) != NULL) {
		class = search_token(token, UNKNOWN_LENGTH, ":") + 1;
		printf(" %s", class);
	}

	if ((token = search_token(str, len, "CMD")) != NULL ||
		(token = search_token(str, len, "COMMAND")) != NULL)
		printf(" %s",
			search_token(token, UNKNOWN_LENGTH, ":") + 1);

	printf("\n");

	if (class)
		/* identify class ident */
		for (i = 0; pnp_tokens[i] != NULL; i++) {
			if (search_token(class, len, pnp_tokens[i]) != NULL) {
				class_id = i;
				goto end_detect;
			}
		}

	class_id = PPB_PnP_UNKNOWN;

end_detect:
	return (class_id);
}

/*
 * ppb_scan_bus()
 *
 * Scan the ppbus for IEEE1284 compliant devices
 */
static int
ppb_scan_bus(device_t bus)
{
	struct ppb_data * ppb = (struct ppb_data *)device_get_softc(bus);
	int error = 0;
	int unit = device_get_unit(bus);

	/* try all IEEE1284 modes, for one device only
	 * 
	 * XXX We should implement the IEEE1284.3 standard to detect
	 * daisy chained devices
	 */

	error = ppb_1284_negociate(bus, PPB_NIBBLE, PPB_REQUEST_ID);

	if ((ppb->state == PPB_ERROR) && (ppb->error == PPB_NOT_IEEE1284))
		goto end_scan;

	ppb_1284_terminate(bus);

	printf("ppbus%d: IEEE1284 device found ", unit);

	if (!(error = ppb_1284_negociate(bus, PPB_NIBBLE, 0))) {
		printf("/NIBBLE");
		ppb_1284_terminate(bus);
	}

	if (!(error = ppb_1284_negociate(bus, PPB_PS2, 0))) {
		printf("/PS2");
		ppb_1284_terminate(bus);
	}

	if (!(error = ppb_1284_negociate(bus, PPB_ECP, 0))) {
		printf("/ECP");
		ppb_1284_terminate(bus);
	}

	if (!(error = ppb_1284_negociate(bus, PPB_ECP, PPB_USE_RLE))) {
		printf("/ECP_RLE");
		ppb_1284_terminate(bus);
	}

	if (!(error = ppb_1284_negociate(bus, PPB_EPP, 0))) {
		printf("/EPP");
		ppb_1284_terminate(bus);
	}

	/* try more IEEE1284 modes */
	if (bootverbose) {
		if (!(error = ppb_1284_negociate(bus, PPB_NIBBLE,
				PPB_REQUEST_ID))) {
			printf("/NIBBLE_ID");
			ppb_1284_terminate(bus);
		}

		if (!(error = ppb_1284_negociate(bus, PPB_PS2,
				PPB_REQUEST_ID))) {
			printf("/PS2_ID");
			ppb_1284_terminate(bus);
		}

		if (!(error = ppb_1284_negociate(bus, PPB_ECP,
				PPB_REQUEST_ID))) {
			printf("/ECP_ID");
			ppb_1284_terminate(bus);
		}

		if (!(error = ppb_1284_negociate(bus, PPB_ECP,
				PPB_REQUEST_ID | PPB_USE_RLE))) {
			printf("/ECP_RLE_ID");
			ppb_1284_terminate(bus);
		}

		if (!(error = ppb_1284_negociate(bus, PPB_COMPATIBLE,
				PPB_EXTENSIBILITY_LINK))) {
			printf("/Extensibility Link");
			ppb_1284_terminate(bus);
		}
	}

	printf("\n");

	/* detect PnP devices */
	ppb->class_id = ppb_pnp_detect(bus);

	return (0);

end_scan:
	return (error);
}

#endif /* !DONTPROBE_1284 */

static int
ppbus_attach(device_t dev)
{

	/* Locate our children */
	bus_generic_probe(dev);

#ifndef DONTPROBE_1284
	/* detect IEEE1284 compliant devices */
	ppb_scan_bus(dev);
#endif /* !DONTPROBE_1284 */

	/* launch attachement of the added children */
	bus_generic_attach(dev);

	return 0;
}

static int
ppbus_setup_intr(device_t bus, device_t child, struct resource *r, int flags,
			void (*ihand)(void *), void *arg, void **cookiep)
{
	int error;
	struct ppb_data *ppb = DEVTOSOFTC(bus);
	struct ppb_device *ppbdev = (struct ppb_device *)device_get_ivars(child);

	/* a device driver must own the bus to register an interrupt */
	if (ppb->ppb_owner != child)
		return (EINVAL);

	if ((error = BUS_SETUP_INTR(device_get_parent(bus), child, r, flags,
					ihand, arg, cookiep)))
		return (error);

	/* store the resource and the cookie for eventually forcing
	 * handler unregistration
	 */
	ppbdev->intr_cookie = *cookiep;
	ppbdev->intr_resource = r;

	return (0);
}

static int
ppbus_teardown_intr(device_t bus, device_t child, struct resource *r, void *ih)
{
	struct ppb_data *ppb = DEVTOSOFTC(bus);
	struct ppb_device *ppbdev = (struct ppb_device *)device_get_ivars(child);
	
	/* a device driver must own the bus to unregister an interrupt */
	if ((ppb->ppb_owner != child) || (ppbdev->intr_cookie != ih) ||
			(ppbdev->intr_resource != r))
		return (EINVAL);

	ppbdev->intr_cookie = 0;
	ppbdev->intr_resource = 0;

	/* pass unregistration to the upper layer */
	return (BUS_TEARDOWN_INTR(device_get_parent(bus), child, r, ih));
}

/*
 * ppb_request_bus()
 *
 * Allocate the device to perform transfers.
 *
 * how	: PPB_WAIT or PPB_DONTWAIT
 */
int
ppb_request_bus(device_t bus, device_t dev, int how)
{
	int s, error = 0;
	struct ppb_data *ppb = DEVTOSOFTC(bus);
	struct ppb_device *ppbdev = (struct ppb_device *)device_get_ivars(dev);

	while (!error) {
		s = splhigh();	
		if (ppb->ppb_owner) {
			splx(s);

			switch (how) {
			case (PPB_WAIT | PPB_INTR):
				error = tsleep(ppb, PPBPRI|PCATCH, "ppbreq", 0);
				break;

			case (PPB_WAIT | PPB_NOINTR):
				error = tsleep(ppb, PPBPRI, "ppbreq", 0);
				break;

			default:
				return (EWOULDBLOCK);
				break;
			}

		} else {
			ppb->ppb_owner = dev;

			/* restore the context of the device
			 * The first time, ctx.valid is certainly false
			 * then do not change anything. This is usefull for
			 * drivers that do not set there operating mode 
			 * during attachement
			 */
			if (ppbdev->ctx.valid)
				ppb_set_mode(bus, ppbdev->ctx.mode);

			splx(s);
			return (0);
		}
	}

	return (error);
}

/*
 * ppb_release_bus()
 *
 * Release the device allocated with ppb_request_bus()
 */
int
ppb_release_bus(device_t bus, device_t dev)
{
	int s, error;
	struct ppb_data *ppb = DEVTOSOFTC(bus);
	struct ppb_device *ppbdev = (struct ppb_device *)device_get_ivars(dev);

	if (ppbdev->intr_resource != 0)
		/* force interrupt handler unregistration when the ppbus is released */
		if ((error = BUS_TEARDOWN_INTR(bus, dev, ppbdev->intr_resource,
					       ppbdev->intr_cookie)))
			return (error);

	s = splhigh();
	if (ppb->ppb_owner != dev) {
		splx(s);
		return (EACCES);
	}

	ppb->ppb_owner = 0;
	splx(s);

	/* save the context of the device */
	ppbdev->ctx.mode = ppb_get_mode(bus);

	/* ok, now the context of the device is valid */
	ppbdev->ctx.valid = 1;

	/* wakeup waiting processes */
	wakeup(ppb);

	return (0);
}

static devclass_t ppbus_devclass;

static device_method_t ppbus_methods[] = {
        /* device interface */
	DEVMETHOD(device_probe,         ppbus_probe),
	DEVMETHOD(device_attach,        ppbus_attach),
  
        /* bus interface */
	DEVMETHOD(bus_add_child,	ppbus_add_child),
	DEVMETHOD(bus_print_child,	ppbus_print_child),
	DEVMETHOD(bus_read_ivar,        ppbus_read_ivar),
	DEVMETHOD(bus_write_ivar,       ppbus_write_ivar),
	DEVMETHOD(bus_setup_intr,	ppbus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	ppbus_teardown_intr),
	DEVMETHOD(bus_alloc_resource,   bus_generic_alloc_resource),

        { 0, 0 }
};

static driver_t ppbus_driver = {
        "ppbus",
        ppbus_methods,
        sizeof(struct ppb_data),
};
DRIVER_MODULE(ppbus, ppc, ppbus_driver, ppbus_devclass, 0, 0);
