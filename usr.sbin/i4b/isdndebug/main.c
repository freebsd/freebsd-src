/*
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	main.c - i4b set debug options
 *	------------------------------
 *
 *	$Id: main.c,v 1.27 2000/07/24 12:22:08 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Thu Oct 26 08:50:30 2000]
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>

#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>

char *bin_str(unsigned long val, int length);

static void usage ( void );
void printl1(unsigned long val);
void printl2(unsigned long val);
void printl3(unsigned long val);
void printl4(unsigned long val);

static int isdnfd;

#define I4BCTLDEVICE	"/dev/i4bctl"

int opt_get = 0;
int opt_layer = -1;
int opt_set = 0;
int opt_setval;
int opt_reset = 0;
int opt_max = 0;
int opt_err = 0;
int opt_zero = 0;
int opt_unit = 0;
int opt_lapd = 0;
int opt_rlapd = 0;
int opt_chipstat = 0;

/*---------------------------------------------------------------------------*
 *	usage display and exit
 *---------------------------------------------------------------------------*/
static void
usage(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "isdndebug - i4b set debug level, version %d.%d.%d, compiled %s %s\n", VERSION, REL, STEP, __DATE__, __TIME__);
	fprintf(stderr, "usage: isdndebug -c -e -g -l <layer> -m -q -r -s <value> -u <unit> -z -C -Q\n");
	fprintf(stderr, "       -c            get chipset statistics\n");
	fprintf(stderr, "       -e            set error only debugging output\n");
	fprintf(stderr, "       -g            get current debugging values\n");
	fprintf(stderr, "       -l layer      specify layer (1...4)\n");
	fprintf(stderr, "       -m            set maximum debugging output\n");
	fprintf(stderr, "       -q            get Q.921 statistics\n");
	fprintf(stderr, "       -r            reset values(s) to compiled in default\n");
	fprintf(stderr, "       -s value      set new debugging value for layer\n");
	fprintf(stderr, "       -u unit       unit number for -c, -q, -C and -Q commands\n");	
	fprintf(stderr, "       -z            set zero (=no) debugging output\n");
	fprintf(stderr, "       -C            reset chipset statistics\n");
	fprintf(stderr, "       -Q            reset Q.921 statistics\n");
	fprintf(stderr, "\n");
	exit(1);
}

/*---------------------------------------------------------------------------*
 *	program entry
 *---------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
	int c;
	ctl_debug_t cdbg;
	int ret;
	
	while ((c = getopt(argc, argv, "ceghl:mqrs:u:zCHQ")) != -1)
	{
		switch(c)
		{
			case 'c':
				opt_chipstat = 1;
				break;

			case 'e':
				opt_err = 1;
				break;

			case 'g':
				opt_get = 1;
				break;

			case 'q':
				opt_lapd = 1;
				break;

			case 'r':
				opt_reset = 1;
				break;

			case 'm':
				opt_max = 1;
				break;

			case 'l':
				opt_layer = atoi(optarg);
				if(opt_layer < 1 || opt_layer > 4)
					usage();
				break;

			case 's':
				if((sscanf(optarg, "%i", &opt_setval)) != 1)
					usage();
				opt_set = 1;
				break;

			case 'u':
				opt_unit = atoi(optarg);
				if(opt_unit < 0 || opt_unit > 9)
					usage();
				break;

			case 'z':
				opt_zero = 1;
				break;

			case 'Q':
				opt_rlapd = 1;
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	if(opt_get == 0 && opt_set == 0 && opt_reset == 0 && opt_max == 0 &&
	   opt_err == 0 && opt_zero == 0 && opt_lapd == 0 && opt_rlapd == 0 &&
	   opt_chipstat == 0)
	{
		usage();
	}

	if((opt_get + opt_set + opt_reset + opt_max + opt_err + opt_zero +
	    opt_lapd + opt_rlapd + opt_chipstat) > 1)
	{
		usage();
	}

	if((isdnfd = open(I4BCTLDEVICE, O_RDWR)) < 0)
	{
		fprintf(stderr, "i4bctl: cannot open %s: %s\n", I4BCTLDEVICE, strerror(errno));
		exit(1);
	}

	if(opt_chipstat)
	{
		struct chipstat cst;
		u_char *name;
		
		cst.driver_unit = opt_unit;
		cst.driver_bchannel = 0; 
		
		if((ret = ioctl(isdnfd, I4B_CTL_GET_CHIPSTAT, &cst)) < 0)
		{
			fprintf(stderr, "ioctl I4B_CTL_GET_CHIPSTAT failed: %s", strerror(errno));
			exit(1);
		}

		switch(cst.driver_type)
		{
			case L1DRVR_ISIC:
				name = "isic";
				printf("\nisic-driver\nHSCX events:      VFR    RDO    CRC    RAB    XDU    RFO\n");

				printf("unit %d chan %d: %6d %6d %6d %6d %6d %6d\n",
					cst.stats.hscxstat.unit,
					cst.stats.hscxstat.chan,
					cst.stats.hscxstat.vfr,
					cst.stats.hscxstat.rdo,
					cst.stats.hscxstat.crc,
					cst.stats.hscxstat.rab,
					cst.stats.hscxstat.xdu,
					cst.stats.hscxstat.rfo);

				cst.driver_unit = opt_unit;
				cst.driver_bchannel = 1; 
		
				if((ret = ioctl(isdnfd, I4B_CTL_GET_CHIPSTAT, &cst)) < 0)
				{
					fprintf(stderr, "ioctl I4B_CTL_GET_CHIPSTAT failed: %s", strerror(errno));
					exit(1);
				}

				printf("HSCX events:      VFR    RDO    CRC    RAB    XDU    RFO\n");

				printf("unit %d chan %d: %6d %6d %6d %6d %6d %6d\n",
					cst.stats.hscxstat.unit,
					cst.stats.hscxstat.chan,
					cst.stats.hscxstat.vfr,
					cst.stats.hscxstat.rdo,
					cst.stats.hscxstat.crc,
					cst.stats.hscxstat.rab,
					cst.stats.hscxstat.xdu,
					cst.stats.hscxstat.rfo);

				break;
	
			case L1DRVR_IWIC:
				name = "iwic";
				break;
	
			case L1DRVR_IFPI:
				name = "ifpi";
				break;
	
			case L1DRVR_IHFC:
				name = "ihfc";
				break;
	
			case L1DRVR_IFPNP:
				name = "ifpnp";
				break;

			default:
				fprintf(stderr, "ioctl I4B_CTL_GET_CHIPSTAT, unknown driver %d\n",cst.driver_type);
				exit(1);
				break;
		}
		exit(0);
	}

	if(opt_lapd)
	{
		l2stat_t l2s;

		l2s.unit = opt_unit;
		
		if((ret = ioctl(isdnfd, I4B_CTL_GET_LAPDSTAT, &l2s)) < 0)
		{
			fprintf(stderr, "ioctl I4B_CTL_GET_LAPDSTAT failed: %s", strerror(errno));
			exit(1);
		}

		printf("unit %d Q.921 statistics: receive     transmit\n", opt_unit);
		printf("---------------------------------------------\n");
		printf("# of I-frames       %12lu %12lu\n", l2s.lapdstat.rx_i, l2s.lapdstat.tx_i);
		printf("# of RR-frames      %12lu %12lu\n", l2s.lapdstat.rx_rr, l2s.lapdstat.tx_rr);
		printf("# of RNR-frames     %12lu %12lu\n", l2s.lapdstat.rx_rnr, l2s.lapdstat.tx_rnr);
		printf("# of REJ-frames     %12lu %12lu\n", l2s.lapdstat.rx_rej, l2s.lapdstat.tx_rej);
		printf("# of SABME-frames   %12lu %12lu\n", l2s.lapdstat.rx_sabme, l2s.lapdstat.tx_sabme);
		printf("# of DM-frames      %12lu %12lu\n", l2s.lapdstat.rx_dm, l2s.lapdstat.tx_dm);
		printf("# of DISC-frames    %12lu %12lu\n", l2s.lapdstat.rx_disc, l2s.lapdstat.tx_disc);
		printf("# of UA-frames      %12lu %12lu\n", l2s.lapdstat.rx_ua, l2s.lapdstat.tx_ua);
		printf("# of FRMR-frames    %12lu %12lu\n", l2s.lapdstat.rx_frmr, l2s.lapdstat.tx_frmr);
		printf("# of TEI-frames     %12lu %12lu\n", l2s.lapdstat.rx_tei, l2s.lapdstat.tx_tei);
		printf("# of UI-frames      %12lu      \n", l2s.lapdstat.rx_ui);
		printf("# of XID-frames     %12lu      \n", l2s.lapdstat.rx_xid);
		printf("                                       errors\n");
		printf("---------------------------------------------\n");
		printf("# of frames with incorrect length%12lu\n", l2s.lapdstat.err_rx_len);
		printf("# of frames with bad frame type  %12lu\n", l2s.lapdstat.err_rx_badf);
		printf("# of bad S frames                %12lu\n", l2s.lapdstat.err_rx_bads);
		printf("# of bad U frames                %12lu\n", l2s.lapdstat.err_rx_badu);
		printf("# of bad UI frames               %12lu\n", l2s.lapdstat.err_rx_badui);

		exit(0);
	}

	if(opt_rlapd)
	{
		int unit;

		unit = opt_unit;
		
		if((ret = ioctl(isdnfd, I4B_CTL_CLR_LAPDSTAT, &unit)) < 0)
		{
			fprintf(stderr, "ioctl I4B_CTL_CLR_LAPDSTAT failed: %s", strerror(errno));
			exit(1);
		}

		printf("Q.921 statistics counters unit %d reset to zero!\n", unit);
		exit(0);
	}
		
	if((ret = ioctl(isdnfd, I4B_CTL_GET_DEBUG, &cdbg)) < 0)
	{
		fprintf(stderr, "ioctl I4B_CTL_GET_DEBUG failed: %s", strerror(errno));
		exit(1);
	}

	if(opt_get)
	{
		switch(opt_layer)
		{
			case -1:
				printl1(cdbg.l1);
				printl2(cdbg.l2);
				printl3(cdbg.l3);
				printl4(cdbg.l4);
				break;
	
			case 1:
				printl1(cdbg.l1);
				break;
	
			case 2:
				printl2(cdbg.l2);
				break;
	
			case 3:
				printl3(cdbg.l3);
				break;
	
			case 4:
				printl4(cdbg.l4);
				break;
		}
		printf("\n");
		return(0);
	}
	else if(opt_set)
	{
		switch(opt_layer)
		{
			case -1:
				usage();
				break;
	
			case 1:
				cdbg.l1 = opt_setval;
				break;
	
			case 2:
				cdbg.l2 = opt_setval;
				break;
	
			case 3:
				cdbg.l3 = opt_setval;
				break;
	
			case 4:
				cdbg.l4 = opt_setval;
				break;
		}
	}
	else if(opt_reset)
	{
		switch(opt_layer)
		{
			case -1:
				cdbg.l1 = L1_DEBUG_DEFAULT;
				cdbg.l2 = L2_DEBUG_DEFAULT;
				cdbg.l3 = L3_DEBUG_DEFAULT;
				cdbg.l4 = L4_DEBUG_DEFAULT;
				break;
	
			case 1:
				cdbg.l1 = L1_DEBUG_DEFAULT;
				break;
	
			case 2:
				cdbg.l2 = L2_DEBUG_DEFAULT;
				break;
	
			case 3:
				cdbg.l3 = L3_DEBUG_DEFAULT;
				break;
	
			case 4:
				cdbg.l4 = L4_DEBUG_DEFAULT;
				break;
		}
	}
	else if(opt_max)
	{
		switch(opt_layer)
		{
			case -1:
				cdbg.l1 = L1_DEBUG_MAX;
				cdbg.l2 = L2_DEBUG_MAX;
				cdbg.l3 = L3_DEBUG_MAX;
				cdbg.l4 = L4_DEBUG_MAX;
				break;
	
			case 1:
				cdbg.l1 = L1_DEBUG_MAX;
				break;
	
			case 2:
				cdbg.l2 = L2_DEBUG_MAX;
				break;
	
			case 3:
				cdbg.l3 = L3_DEBUG_MAX;
				break;
	
			case 4:
				cdbg.l4 = L4_DEBUG_MAX;
				break;
		}
	}
	else if(opt_err)
	{
		switch(opt_layer)
		{
			case -1:
				cdbg.l1 = L1_DEBUG_ERR;
				cdbg.l2 = L2_DEBUG_ERR;
				cdbg.l3 = L3_DEBUG_ERR;
				cdbg.l4 = L4_DEBUG_ERR;
				break;
	
			case 1:
				cdbg.l1 = L1_DEBUG_ERR;
				break;
	
			case 2:
				cdbg.l2 = L2_DEBUG_ERR;
				break;
	
			case 3:
				cdbg.l3 = L3_DEBUG_ERR;
				break;
	
			case 4:
				cdbg.l4 = L4_DEBUG_ERR;
				break;
		}
	}
	else if(opt_zero)
	{
		switch(opt_layer)
		{
			case -1:
				cdbg.l1 = 0;
				cdbg.l2 = 0;
				cdbg.l3 = 0;
				cdbg.l4 = 0;
				break;
	
			case 1:
				cdbg.l1 = 0;
				break;
	
			case 2:
				cdbg.l2 = 0;
				break;
	
			case 3:
				cdbg.l3 = 0;
				break;
	
			case 4:
				cdbg.l4 = 0;
				break;
		}
	}
	else
	{
		exit(1);
	}	

	if((ret = ioctl(isdnfd, I4B_CTL_SET_DEBUG, &cdbg)) < 0)
	{
		fprintf(stderr, "ioctl I4B_CTL_SET_DEBUG failed: %s", strerror(errno));
		exit(1);
	}
	return(0);
}

/*---------------------------------------------------------------------------*
 *	return ptr to string of 1's and 0's for value
 *---------------------------------------------------------------------------*/
char *
bin_str(unsigned long val, int length)
{
	static char buffer[80];
	int i = 0;

	if (length > 32)
		length = 32;

	val = val << (32 - length);

	while (length--)
	{
		if (val & 0x80000000)
			buffer[i++] = '1';
		else
			buffer[i++] = '0';
		if ((length % 4) == 0 && length)
			buffer[i++] = '.';
		val = val << 1;
	}
	return (buffer);
}

/*---------------------------------------------------------------------------*
 *	print l1 info
 *---------------------------------------------------------------------------*/
void
printl1(unsigned long val)
{
	printf("\nLayer 1: %s  =  0x%lX\n", bin_str(val, 32), val);
	printf("                           | |||| |||| |||| ||||\n"),
	printf("                           | |||| |||| |||| |||+- general error messages\n");
	printf("                           | |||| |||| |||| ||+-- PH primitives exchanged\n");
	printf("                           | |||| |||| |||| |+--- B channel actions\n");
	printf("                           | |||| |||| |||| +---- HSCX error messages\n");
	printf("                           | |||| |||| |||+------ HSCX IRQ messages\n");
	printf("                           | |||| |||| ||+------- ISAC error messages\n");
	printf("                           | |||| |||| |+-------- ISAC messages\n");
	printf("                           | |||| |||| +--------- ISAC setup messages\n");
	printf("                           | |||| |||+----------- FSM general messages\n");
	printf("                           | |||| ||+------------ FSM error messages\n");
	printf("                           | |||| |+------------- timer general messages\n");
	printf("                           | |||| +-------------- timer error messages\n");
	printf("                           | |||+---------------- HSCX data xfer errors msgs\n");
	printf("                           | ||+----------------- ISAC CICO messages\n");
	printf("                           | |+------------------ silent messages (soft-HDLC)\n");
	printf("                           | +------------------- error messages (soft-HDLC)\n");
	printf("                           +--------------------- HFC-S PCI debug messages\n");
	printf("         ++++-++++-++++-+++---------------------- unassigned\n");
}

/*---------------------------------------------------------------------------*
 *	print l2 info
 *---------------------------------------------------------------------------*/
void
printl2(unsigned long val)
{
	printf("\nLayer 2: %s  =  0x%lX\n", bin_str(val, 32), val);
	printf("                               || |||| |||| ||||\n"),
	printf("                               || |||| |||| |||+- general error messages\n");
	printf("                               || |||| |||| ||+-- DL primitives exchanged\n");
	printf("                               || |||| |||| |+--- U frame messages\n");
	printf("                               || |||| |||| +---- U frame error messages\n");
	printf("                               || |||| |||+------ S frame messages\n");
	printf("                               || |||| ||+------- S frame error messages\n");
	printf("                               || |||| |+-------- I frame messages\n");
	printf("                               || |||| +--------- I frame error messages\n");
	printf("                               || |||+----------- FSM general messages\n");
	printf("                               || ||+------------ FSM error messages\n");
	printf("                               || |+------------- timer general messages\n");
	printf("                               || +-------------- timer error messages\n");
	printf("                               |+---------------- TEI general messages\n");
	printf("                               +----------------- TEI error messages\n");
	printf("         ++++-++++-++++-++++-++------------------ unassigned\n");
}

/*---------------------------------------------------------------------------*
 *	print l3 info
 *---------------------------------------------------------------------------*/
void
printl3(unsigned long val)
{
	printf("\nLayer 3: %s  =  0x%lX\n", bin_str(val, 32), val);
	printf("                                   ||| |||| ||||\n"),
	printf("                                   ||| |||| |||+- general error messages\n");
	printf("                                   ||| |||| ||+-- general messages\n");
	printf("                                   ||| |||| |+--- FSM messages\n");
	printf("                                   ||| |||| +---- FSM error messages\n");
	printf("                                   ||| |||+------ timer messages\n");
	printf("                                   ||| ||+------- timer error messages\n");
	printf("                                   ||| |+-------- protocol messages\n");
	printf("                                   ||| +--------- protocol error messages\n");
	printf("                                   ||+----------- facility messages\n");
	printf("                                   |+------------ facility error messages\n");
	printf("                                   +------------- Q.931 messages exchanged\n");	
	printf("         ++++-++++-++++-++++-++++-+-------------- unassigned\n");
}

/*---------------------------------------------------------------------------*
 *	print l4 info
 *---------------------------------------------------------------------------*/
void
printl4(unsigned long val)
{
	printf("\nLayer 4: %s  =  0x%lX\n", bin_str(val, 32), val);
	printf("                                  |||| |||| ||||\n"),
	printf("                                  |||| |||| |||+- general error messages\n");
	printf("                                  |||| |||| ||+-- general messages\n");
	printf("                                  |||| |||| |+--- B-ch timeout messages\n");
	printf("                                  |||| |||| +---- network driver dial state\n");
	printf("                                  |||| |||+------ ipr driver debug messages\n");
	printf("                                  |||| ||+------- rbch driver debug messages\n");
	printf("                                  |||| |+-------- isp driver debug messages\n");
	printf("                                  |||| +--------- tel driver debug messages\n");
	printf("                                  |||+----------- tina driver debug messages\n");
	printf("                                  ||+------------ tina driver messages\n");
	printf("                                  |+------------- tina driver error messages\n");
	printf("                                  +-------------- ing driver debug messages\n");
	printf("         ++++-++++-++++-++++-++++---------------- unassigned\n");
}

/* EOF */
