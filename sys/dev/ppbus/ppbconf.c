/*-
 * Copyright (c) 1997 Nicolas Souchu
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
 *	$Id: ppbconf.c,v 1.3 1997/08/28 10:15:14 msmith Exp $
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/ppb_1284.h>

LIST_HEAD(, ppb_data)	ppbdata;	/* list of existing ppbus */

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
 * This function is called by ppcattach().
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

static char *pnp_tokens[] = {
	"PRINTER", "MODEM", "NET", "HDC", "PCMCIA", "MEDIA",
	"FDC", "PORTS", "SCANNER", "DIGICAM", "", NULL };

static char *pnp_classes[] = {
	"printer", "modem", "network device",
	"hard disk", "PCMCIA", "multimedia device",
	"floppy disk", "ports", "scanner",
	"digital camera", "unknown device", NULL };

/*
 * search_token()
 *
 * Search the first occurence of a token within a string
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
ppb_pnp_detect(struct ppb_data *ppb)
{
	char *token, *q, *class = 0;
	int i, len, error;
	char str[PPB_PnP_STRING_SIZE+1];

	struct ppb_device pnpdev;	/* temporary device to perform I/O */

	/* initialize the pnpdev structure for future use */
	bzero(&pnpdev, sizeof(pnpdev));

	pnpdev.ppb = ppb;

#ifdef PnP_DEBUG
	printf("ppb: <PnP> probing PnP devices on ppbus%d...\n",
		ppb->ppb_link->adapter_unit);
#endif

	ppb_wctr(&pnpdev, nINIT | SELECTIN);

	/* select NIBBLE_1284_REQUEST_ID mode */
	if ((error = nibble_1284_mode(&pnpdev, NIBBLE_1284_REQUEST_ID))) {
#ifdef PnP_DEBUG
		printf("ppb: <PnP> nibble_1284_mode()=%d\n", error);
#endif
		return (-1);
	}

	len = 0;
	for (q = str; !(ppb_rstr(&pnpdev) & ERROR); q++) {
		if ((error = nibble_1284_inbyte(&pnpdev, q))) {
#ifdef PnP_DEBUG
			printf("ppb: <PnP> nibble_1284_inbyte()=%d\n", error);
#endif
			return (-1);
		}
		if (len++ >= PPB_PnP_STRING_SIZE) {
			printf("ppb: <PnP> not space left!\n");
			return (-1);
		}
	}
	*q = '\0';

	nibble_1284_sync(&pnpdev);

#ifdef PnP_DEBUG
	printf("ppb: <PnP> %d characters: ", len);
	for (i = 0; i < len; i++)
		printf("0x%x ", str[i]);
	printf("\n");
#endif

	/* replace ';' characters by '\0' */
	for (i = 0; i < len; i++)
		str[i] = (str[i] == ';') ? '\0' : str[i];

	if ((token = search_token(str, len, "MFG")) != NULL)
		printf("ppbus%d: <%s", ppb->ppb_link->adapter_unit,
			search_token(token, UNKNOWN_LENGTH, ":") + 1);
	else
		printf("ppbus%d: <unknown", ppb->ppb_link->adapter_unit);

	if ((token = search_token(str, len, "MDL")) != NULL)
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

	if ((token = search_token(str, len, "CMD")) != NULL)
		printf(" %s",
			search_token(token, UNKNOWN_LENGTH, ":") + 1);

	printf("\n");

	if (class)
		/* identify class ident */
		for (i = 0; pnp_tokens[i] != NULL; i++) {
			if (search_token(class, len, pnp_tokens[i]) != NULL) {
				return (i);
				break;
			}
		}

	return (PPB_PnP_UNKNOWN);
}

/*
 * ppb_attachdevs()
 *
 * Called by ppcattach(), this function probes the ppbus and
 * attaches found devices.
 */
int
ppb_attachdevs(struct ppb_data *ppb)
{
	int error;
	struct ppb_device *dev;
	struct ppb_driver **p_drvpp, *p_drvp;

	LIST_INIT(&ppb->ppb_devs);	/* initialise device/driver list */
	p_drvpp = (struct ppb_driver **)ppbdriver_set.ls_items;

	/* detect PnP devices */
	ppb->class_id = ppb_pnp_detect(ppb);
	
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

	/* wakeup waiting processes */
	wakeup(ppb);

	return (0);
}
