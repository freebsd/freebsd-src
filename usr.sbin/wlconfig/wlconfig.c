/*
 * Copyright (C) 1996
 *      Michael Smith.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY Michael Smith AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Michael Smith OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/wlconfig/wlconfig.c,v 1.8.2.1 2000/07/19 16:33:02 archie Exp $";
#endif /* not lint */

/*
 * wlconfig.c
 * 
 * utility to read out and change various WAVELAN parameters.
 * Currently supports NWID and IRQ values.
 *
 * The NWID is used by 2 or more wavelan controllers to determine
 * if packets should be received or not.  It is a filter that
 * is roughly analogous to the "channel" setting with a garage
 * door controller.  Two companies side by side with wavelan devices
 * that could actually hear each other can use different NWIDs
 * and ignore packets.  In truth however, the air space is shared, 
 * and the NWID is a virtual filter.
 *
 * In the current set of wavelan drivers, ioctls changed only
 * the runtime radio modem registers which act in a manner analogous
 * to an ethernet transceiver.  The ioctls do not change the 
 * stored nvram PSA (or parameter storage area).  At boot, the PSA
 * values are stored in the radio modem.   Thus when the
 * system reboots it will restore the wavelan NWID to the value
 * stored in the PSA.  The NCR/ATT dos utilities must be used to
 * change the initial NWID values in the PSA.  The wlconfig utility
 * may be used to set a different NWID at runtime; this is only
 * permitted while the interface is up and running.
 *
 * By contrast, the IRQ value can only be changed while the 
 * Wavelan card is down and unconfigured, and it will remain
 * disabled after an IRQ change until reboot.
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <machine/if_wl_wavelan.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* translate IRQ bit to number */
/* array for maping irq numbers to values for the irq parameter register */
static int irqvals[16] = { 
    0, 0, 0, 0x01, 0x02, 0x04, 0, 0x08, 0, 0, 0x10, 0x20, 0x40, 0, 0, 0x80 
};

/* cache */
static int w_sigitems;	/* count of valid items */
static struct w_sigcache wsc[MAXCACHEITEMS];

int
wlirq(int irqval)
{
    int irq;
    
    for(irq = 0; irq < 16; irq++)
	if(irqvals[irq] == irqval)
	    return(irq);
    return 0;
}

char *compat_type[] = {
    "PC-AT 915MHz",
    "PC-MC 915MHz",
    "PC-AT 2.4GHz",
    "PC-MC 2.4GHz",
    "PCCARD or 1/2 size AT, 915MHz or 2.4GHz"
};

char *subband[] = {
    "915MHz/see WaveModem",
    "2425MHz",
    "2460MHz",
    "2484MHz",
    "2430.5MHz"
};


/*
** print_psa
**
** Given a pointer to a PSA structure, print it out
*/
void
print_psa(u_char *psa, int currnwid)
{
    int		nwid;
    
    /*
    ** Work out what sort of board we have
    */
    if (psa[0] == 0x14) {
	printf("Board type            : Microchannel\n");
    } else {
	if (psa[1] == 0) {
	    printf("Board type            : PCCARD\n");
	} else {
	    printf("Board type            : ISA");
	    if ((psa[4] == 0) &&
		(psa[5] == 0) &&
		(psa[6] == 0))
		printf(" (DEC OEM)");
	    printf("\n");
	    printf("Base address options  : 0x300, 0x%02x0, 0x%02x0, 0x%02x0\n",
		   (int)psa[1], (int)psa[2], (int)psa[3]);
	    printf("Waitstates            : %d\n",psa[7] & 0xf);
	    printf("Bus mode              : %s\n",(psa[7] & 0x10) ? "EISA" : "ISA");
	    printf("IRQ                   : %d\n",wlirq(psa[8]));
	}
    }
    printf("Default MAC address   : %02x:%02x:%02x:%02x:%02x:%02x\n",
	   psa[0x10],psa[0x11],psa[0x12],psa[0x13],psa[0x14],psa[0x15]);
    printf("Soft MAC address      : %02x:%02x:%02x:%02x:%02x:%02x\n",
	   psa[0x16],psa[0x17],psa[0x18],psa[0x19],psa[0x1a],psa[0x1b]);
    printf("Current MAC address   : %s\n",(psa[0x1c] & 0x1) ? "Soft" : "Default");
    printf("Adapter compatability : ");
    if (psa[0x1d] < 5) {
	printf("%s\n",compat_type[psa[0x1d]]);
    } else {
	printf("unknown\n");
    }
    printf("Threshold preset      : %d\n",psa[0x1e]);
    printf("Call code required    : %s\n",(psa[0x1f] & 0x1) ? "YES" : "NO");
    if (psa[0x1f] & 0x1)
	printf("Call code             : 0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
	       psa[0x30],psa[0x31],psa[0x32],psa[0x33],psa[0x34],psa[0x35],psa[0x36],psa[0x37]);
    printf("Subband               : %s\n",subband[psa[0x20] & 0xf]);
    printf("Quality threshold     : %d\n",psa[0x21]);
    printf("Hardware version      : %d (%s)\n",psa[0x22],psa[0x22] ? "Rel3" : "Rel1/Rel2");
    printf("Network ID enable     : %s\n",(psa[0x25] & 0x1) ? "YES" : "NO");
    if (psa[0x25] & 0x1) {
	nwid = (psa[0x23] << 8) + psa[0x24];
	printf("NWID                  : 0x%04x\n",nwid);
	if (nwid != currnwid) {
	    printf("Current NWID          : 0x%04x\n",currnwid);
	}
    }
    printf("Datalink security     : %s\n",(psa[0x26] & 0x1) ? "YES" : "NO");
    if (psa[0x26] & 0x1) {
	printf("Encryption key        : ");
	if (psa[0x27] == 0) {
	    printf("DENIED\n");
	} else {
	    printf("0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		   psa[0x27],psa[0x28],psa[0x29],psa[0x2a],psa[0x2b],psa[0x2c],psa[0x2d],psa[0x2e]);
	}
    }
    printf("Databus width         : %d (%s)\n",
	   (psa[0x2f] & 0x1) ? 16 : 8, (psa[0x2f] & 0x80) ? "fixed" : "variable");
    printf("Configuration state   : %sconfigured\n",(psa[0x38] & 0x1) ? "" : "un");
    printf("CRC-16                : 0x%02x%02x\n",psa[0x3e],psa[0x3d]);
    printf("CRC status            : ");
    switch(psa[0x3f]) {
    case 0xaa:
	printf("OK\n");
	break;
    case 0x55:
	printf("BAD\n");
	break;
    default:
	printf("Error\n");
	break;
    }
}


static void
usage()
{
    fprintf(stderr,"usage: wlconfig ifname [param value ...]\n");
    exit(1);
}


void
get_cache(int sd, struct ifreq *ifr) 
{
    /* get the cache count */
    if (ioctl(sd, SIOCGWLCITEM, (caddr_t)ifr))
	err(1, "SIOCGWLCITEM - get cache count");
    w_sigitems = (int) ifr->ifr_data;

    ifr->ifr_data = (caddr_t) &wsc;
    /* get the cache */
    if (ioctl(sd, SIOCGWLCACHE, (caddr_t)ifr))
	err(1, "SIOCGWLCACHE - get cache count");
}

static int
scale_value(int value, int max)
{
	double dmax = (double) max;
	if (value > max)
		return(100);
	return((value/dmax) * 100);
}

static void
dump_cache(int rawFlag)
{
	int i;
	int signal, silence, quality; 

	if (rawFlag)
		printf("signal range 0..63: silence 0..63: quality 0..15\n");
	else
		printf("signal range 0..100: silence 0..100: quality 0..100\n");

	/* after you read it, loop through structure,i.e. wsc
         * print each item:
	 */
	for(i = 0; i < w_sigitems; i++) {
		printf("[%d:%d]>\n", i+1, w_sigitems);
        	printf("\tip: %d.%d.%d.%d,",((wsc[i].ipsrc >> 0) & 0xff),
				        ((wsc[i].ipsrc >> 8) & 0xff),
				        ((wsc[i].ipsrc >> 16) & 0xff),
				        ((wsc[i].ipsrc >> 24) & 0xff));
		printf(" mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
		  		    	wsc[i].macsrc[0]&0xff,
		  		    	wsc[i].macsrc[1]&0xff,
		   		    	wsc[i].macsrc[2]&0xff,
		   			wsc[i].macsrc[3]&0xff,
		   			wsc[i].macsrc[4]&0xff,
		   			wsc[i].macsrc[5]&0xff);
		if (rawFlag) {
			signal = wsc[i].signal;
			silence = wsc[i].silence;
			quality = wsc[i].quality;
		}
		else {
			signal = scale_value(wsc[i].signal, 63);
			silence = scale_value(wsc[i].silence, 63);
			quality = scale_value(wsc[i].quality, 15);
		}
		printf("\tsignal: %d, silence: %d, quality: %d, ",
		   			signal,
		   			silence,
		   			quality);
		printf("snr: %d\n", signal - silence);
	}
}

#define raw_cache()	dump_cache(1)
#define scale_cache()	dump_cache(0)

int
main(int argc, char *argv[])
{
    int 		sd;
    struct ifreq	ifr; 
    u_char		psabuf[0x40];
    int			val, argind, i;
    char		*cp, *param, *value;
    struct ether_addr	*ea;
    int			work = 0;
    int			currnwid;

    if ((argc < 2) || (argc % 2))
	usage();

    /* get a socket */
    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
	err(1,"socket");
    strncpy(ifr.ifr_name, argv[1], sizeof(ifr.ifr_name));
    ifr.ifr_addr.sa_family = AF_INET;

    /* get the PSA */
    ifr.ifr_data = (caddr_t)psabuf;
    if (ioctl(sd, SIOCGWLPSA, (caddr_t)&ifr))
	err(1,"get PSA");

    /* get the current NWID */
    if (ioctl(sd, SIOCGWLCNWID, (caddr_t)&ifr))
	err(1,"get NWID");
    currnwid = (int)ifr.ifr_data;

    /* just dump and exit? */
    if (argc == 2) {
	print_psa(psabuf, currnwid);
	exit(0);
    }

    /* loop reading arg pairs */
    for (argind = 2; argind < argc; argind += 2) {

	param = argv[argind];
	value = argv[argind+1];

	/* What to do? */
	
	if (!strcasecmp(param,"currnwid")) {		/* set current NWID */
	    val = strtol(value,&cp,0);
	    if ((val < 0) || (val > 0xffff) || (cp == value))
		errx(1,"bad NWID '%s'",value);
	    
	    ifr.ifr_data = (caddr_t)val;
	    if (ioctl(sd, SIOCSWLCNWID, (caddr_t)&ifr))
		err(1,"set NWID (interface not up?)");
	    continue ;
	}

	if (!strcasecmp(param,"irq")) {
	    val = strtol(value,&cp,0);
	    val = irqvals[val];
	    if ((val == 0) || (cp == value))
		errx(1,"bad IRQ '%s'",value);
	    psabuf[WLPSA_IRQNO] = (u_char)val;
	    work = 1;
	    continue;
	}
	
	if (!strcasecmp(param,"mac")) {
	    if ((ea = ether_aton(value)) == NULL)
		errx(1,"bad ethernet address '%s'",value);
	    for (i = 0; i < 6; i++)
		psabuf[WLPSA_LOCALMAC + i] = ea->octet[i];
	    work = 1;
	    continue;
	}

	if (!strcasecmp(param,"macsel")) {
	    if (!strcasecmp(value,"local")) {
		psabuf[WLPSA_MACSEL] |= 0x1;
		work = 1;
		continue;
	    }
	    if (!strcasecmp(value,"universal")) {
		psabuf[WLPSA_MACSEL] &= ~0x1;
		work = 1;
		continue;
	    }
	    errx(1,"bad macsel value '%s'",value);
	}
	
	if (!strcasecmp(param,"nwid")) {
	    val = strtol(value,&cp,0);
	    if ((val < 0) || (val > 0xffff) || (cp == value))
		errx(1,"bad NWID '%s'",value);
	    psabuf[WLPSA_NWID] = (val >> 8) & 0xff;
	    psabuf[WLPSA_NWID+1] = val & 0xff;
	    work = 1;	
	    continue;
	}
	if (!strcasecmp(param,"cache")) {

            /* raw cache dump
	    */
	    if (!strcasecmp(value,"raw")) {
	    	get_cache(sd, &ifr);
		raw_cache();
		continue;
	    }
            /* scaled cache dump
	    */
	    else if (!strcasecmp(value,"scale")) {
	    	get_cache(sd, &ifr);
		scale_cache();
		continue;
	    }
	    /* zero out cache
	    */
	    else if (!strcasecmp(value,"zero")) {
		if (ioctl(sd, SIOCDWLCACHE, (caddr_t)&ifr))
		    err(1,"zero cache");
		continue;
	    }
	    errx(1,"unknown value '%s'", value);
 	}
	errx(1,"unknown parameter '%s'",param);
    }
    if (work) {
	ifr.ifr_data = (caddr_t)psabuf;
	if (ioctl(sd, SIOCSWLPSA, (caddr_t)&ifr))
	    err(1,"set PSA");
    }
    return(0);
}
