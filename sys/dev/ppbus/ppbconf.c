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
 *	$Id: ppbconf.c,v 1.13 1999/01/14 06:22:02 jdp Exp $
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker_set.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/ppb_1284.h>

#include "opt_ppb_1284.h"

static LIST_HEAD(, ppb_data)	ppbdata;	/* list of existing ppbus */

/*
 * Add a null driver so that the linker set always exists.
 */

static struct ppb_driver nulldriver = {
    NULL, NULL, "null"
};
DATA_SET(ppbdriver_set, nulldriver);


/*
 * ppb_alloc_bus()
 *
 * Allocate area to store the ppbus description.
 */
struct ppb_data *
ppb_alloc_bus(void)
{
	struct ppb_data *ppb;
	static int ppbdata_initted = 0;		/* done-init flag */

	ppb = (struct ppb_data *) malloc(sizeof(struct ppb_data),
		M_TEMP, M_NOWAIT);

	/*
	 * Add the new parallel port bus to the list of existing ppbus.
	 */
	if (ppb) {
		bzero(ppb, sizeof(struct ppb_data));

		if (!ppbdata_initted) {		/* list not initialised */
		    LIST_INIT(&ppbdata);
		    ppbdata_initted = 1;
		}
		LIST_INSERT_HEAD(&ppbdata, ppb, ppb_chain);
	} else {
		printf("ppb_alloc_bus: cannot malloc!\n");
	}
	return(ppb);
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
ppb_pnp_detect(struct ppb_data *ppb, struct ppb_device *pnpdev)
{
	char *token, *class = 0;
	int i, len, error;
	int class_id = -1;
	char str[PPB_PnP_STRING_SIZE+1];

	printf("Probing for PnP devices on ppbus%d:\n",
			ppb->ppb_link->adapter_unit);
	
	if ((error = ppb_1284_read_id(pnpdev, PPB_NIBBLE, str,
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
		printf("ppbus%d: <%s", ppb->ppb_link->adapter_unit,
			search_token(token, UNKNOWN_LENGTH, ":") + 1);
	else
		printf("ppbus%d: <unknown", ppb->ppb_link->adapter_unit);

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
ppb_scan_bus(struct ppb_data *ppb)
{
	struct ppb_device pnpdev;	/* temporary device to perform I/O */
	int error = 0;

	/* initialize the pnpdev structure for future use */
	bzero(&pnpdev, sizeof(pnpdev));
	pnpdev.ppb = ppb;

	if ((error = ppb_request_bus(&pnpdev, PPB_DONTWAIT))) {
		if (bootverbose)
			printf("ppb: cannot allocate ppbus!\n");

		return (error);
	}

	/* try all IEEE1284 modes, for one device only
	 * 
	 * XXX We should implement the IEEE1284.3 standard to detect
	 * daisy chained devices
	 */

	error = ppb_1284_negociate(&pnpdev, PPB_NIBBLE, PPB_REQUEST_ID);

	if ((ppb->state == PPB_ERROR) && (ppb->error == PPB_NOT_IEEE1284))
		goto end_scan;

	ppb_1284_terminate(&pnpdev);

	printf("ppb%d: IEEE1284 device found ", ppb->ppb_link->adapter_unit);

	if (!(error = ppb_1284_negociate(&pnpdev, PPB_NIBBLE, 0))) {
		printf("/NIBBLE");
		ppb_1284_terminate(&pnpdev);
	}

	if (!(error = ppb_1284_negociate(&pnpdev, PPB_PS2, 0))) {
		printf("/PS2");
		ppb_1284_terminate(&pnpdev);
	}

	if (!(error = ppb_1284_negociate(&pnpdev, PPB_ECP, 0))) {
		printf("/ECP");
		ppb_1284_terminate(&pnpdev);
	}

	if (!(error = ppb_1284_negociate(&pnpdev, PPB_ECP, PPB_USE_RLE))) {
		printf("/ECP_RLE");
		ppb_1284_terminate(&pnpdev);
	}

	if (!(error = ppb_1284_negociate(&pnpdev, PPB_EPP, 0))) {
		printf("/EPP");
		ppb_1284_terminate(&pnpdev);
	}

	/* try more IEEE1284 modes */
	if (bootverbose) {
		if (!(error = ppb_1284_negociate(&pnpdev, PPB_NIBBLE,
				PPB_REQUEST_ID))) {
			printf("/NIBBLE_ID");
			ppb_1284_terminate(&pnpdev);
		}

		if (!(error = ppb_1284_negociate(&pnpdev, PPB_PS2,
				PPB_REQUEST_ID))) {
			printf("/PS2_ID");
			ppb_1284_terminate(&pnpdev);
		}

		if (!(error = ppb_1284_negociate(&pnpdev, PPB_ECP,
				PPB_REQUEST_ID))) {
			printf("/ECP_ID");
			ppb_1284_terminate(&pnpdev);
		}

		if (!(error = ppb_1284_negociate(&pnpdev, PPB_ECP,
				PPB_REQUEST_ID | PPB_USE_RLE))) {
			printf("/ECP_RLE_ID");
			ppb_1284_terminate(&pnpdev);
		}

		if (!(error = ppb_1284_negociate(&pnpdev, PPB_COMPATIBLE,
				PPB_EXTENSIBILITY_LINK))) {
			printf("/Extensibility Link");
			ppb_1284_terminate(&pnpdev);
		}
	}

	printf("\n");

	/* detect PnP devices */
	ppb->class_id = ppb_pnp_detect(ppb, &pnpdev);

	ppb_release_bus(&pnpdev);

	return (0);

end_scan:
	ppb_release_bus(&pnpdev);
	return (error);
}

#endif /* !DONTPROBE_1284 */

/*
 * ppb_attachdevs()
 *
 * Called by ppcattach(), this function probes the ppbus and
 * attaches found devices.
 */
int
ppb_attachdevs(struct ppb_data *ppb)
{
	struct ppb_device *dev;
	struct ppb_driver **p_drvpp, *p_drvp;

	LIST_INIT(&ppb->ppb_devs);	/* initialise device/driver list */
	p_drvpp = (struct ppb_driver **)ppbdriver_set.ls_items;

#ifndef DONTPROBE_1284
	/* detect IEEE1284 compliant devices */
	ppb_scan_bus(ppb);
#endif /* !DONTPROBE_1284 */
	
	/*
	 * Blindly try all probes here.  Later we should look at
	 * the parallel-port PnP standard, and intelligently seek
	 * drivers based on configuration first.
	 */
	while ((p_drvp = *p_drvpp++) != NULL) {
	    if (p_drvp->probe && (dev = (p_drvp->probe(ppb))) != NULL) {
		/*
		 * Add the device to the list of probed devices.
		 */
		LIST_INSERT_HEAD(&ppb->ppb_devs, dev, chain);
		
		/* Call the device's attach routine */
		(void)p_drvp->attach(dev);
	    }
	}
	return (0);
}

/*
 * ppb_next_bus()
 *
 * Return the next bus in ppbus queue
 */
struct ppb_data *
ppb_next_bus(struct ppb_data *ppb)
{

	if (ppb == NULL)
		return (ppbdata.lh_first);

	return (ppb->ppb_chain.le_next);
}

/*
 * ppb_lookup_bus()
 *
 * Get ppb_data structure pointer according to the base address of the ppbus
 */
struct ppb_data *
ppb_lookup_bus(int base_port)
{
	struct ppb_data *ppb;

	for (ppb = ppbdata.lh_first; ppb; ppb = ppb->ppb_chain.le_next)
		if (ppb->ppb_link->base == base_port)
			break;

	return (ppb);
}

/*
 * ppb_lookup_link()
 *
 * Get ppb_data structure pointer according to the unit value
 * of the corresponding link structure
 */
struct ppb_data *
ppb_lookup_link(int unit)
{
	struct ppb_data *ppb;

	for (ppb = ppbdata.lh_first; ppb; ppb = ppb->ppb_chain.le_next)
		if (ppb->ppb_link->adapter_unit == unit)
			break;

	return (ppb);
}

/*
 * ppb_attach_device()
 *
 * Called by loadable kernel modules to add a device
 */
int
ppb_attach_device(struct ppb_device *dev)
{
	struct ppb_data *ppb = dev->ppb;

	/* add the device to the list of probed devices */
	LIST_INSERT_HEAD(&ppb->ppb_devs, dev, chain);

	return (0);
}

/*
 * ppb_remove_device()
 *
 * Called by loadable kernel modules to remove a device
 */
void
ppb_remove_device(struct ppb_device *dev)
{

	/* remove the device from the list of probed devices */
	LIST_REMOVE(dev, chain);

	return;
}

/*
 * ppb_request_bus()
 *
 * Allocate the device to perform transfers.
 *
 * how	: PPB_WAIT or PPB_DONTWAIT
 */
int
ppb_request_bus(struct ppb_device *dev, int how)
{
	int s, error = 0;
	struct ppb_data *ppb = dev->ppb;

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
			if (dev->ctx.valid)
				ppb_set_mode(dev, dev->ctx.mode);

			splx(s);
			return (0);
		}
	}

	return (error);
}

/*
 * ppb_release_bus()
 *
 * Release the device allocated with ppb_request_dev()
 */
int
ppb_release_bus(struct ppb_device *dev)
{
	int s;
	struct ppb_data *ppb = dev->ppb;

	s = splhigh();
	if (ppb->ppb_owner != dev) {
		splx(s);
		return (EACCES);
	}

	ppb->ppb_owner = 0;
	splx(s);

	/* save the context of the device */
	dev->ctx.mode = ppb_get_mode(dev);

	/* ok, now the context of the device is valid */
	dev->ctx.valid = 1;

	/* wakeup waiting processes */
	wakeup(ppb);

	return (0);
}
