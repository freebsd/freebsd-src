static char     rcsid[] = "@(#)$Id: dbg_5000.c,v 1.1 1995/01/25 14:06:18 jkr Exp jkr $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.1 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: dbg_5000.c,v $
 *
 ******************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>

static int
prtint(i)
{
	switch (i)
	{
		case 0:fprintf(stderr, "5");
		break;
	case 1:
		fprintf(stderr, "4");
		break;
	case 2:
		fprintf(stderr, "3");
		break;
	case 3:
		fprintf(stderr, "2/9");
		break;
	case 4:
		fprintf(stderr, "7");
		break;
	default:
		fprintf(stderr, "??????????\n");
	}
	fprintf(stderr, "\n");
}

static int
print_stat(status)
{
	if (status & 0x8A)
	{
		if (status & 0x80)
			fprintf(stderr, "Software-Fehler\n");
		if (status & 8)
			fprintf(stderr, "Hardware-Fehler\n");
		if (status & 2)
			fprintf(stderr, "Fehler beim Selbsttest\n");
	}
}

analyse_5000(unsigned char *data)
{
	print_stat(data[1]);

	fprintf(stderr, "%x %x %x %x\n", data[1]
		,data[5], data[6], data[7]);
	if (data[1] & 4)
		fprintf(stderr, "Layer 1 not active\n");
	if (data[1] & 0x10)
		fprintf(stderr, "Date/Time not set\n");
	if (data[1] & 0x20)
		fprintf(stderr, "ENTITY not loaded\n");
	if (data[1] & 0x40)
		fprintf(stderr, "out of sync.\n");
	fprintf(stderr, "Hardware Configuration:\n");
	if (data[5] & 0x80)
		fprintf(stderr, "No ");
	fprintf(stderr, "DRAM-Module\n");
	if (data[7] & 1)
		fprintf(stderr, "256 KB SRAM\n");
	switch (data[7])
	{
	case 4:
	case 5:
		fprintf(stderr, "1 MB DRAM\n");
		break;
	case 16:
	case 17:
		fprintf(stderr, "1 MB DRAM\n");
	}
	switch ((data[5] >> 5) & 3)
	{
	case 3:
		fprintf(stderr, "Modem Module\n");
		break;
	case 0:
		break;
	default:
		fprintf(stderr, "??????????\n");
	}
	switch ((data[5] >> 3) & 3)
	{
	case 3:
		fprintf(stderr, "Telefon Module\n");
		break;
	case 2:
		fprintf(stderr, "X Interface\n");
		break;
	case 0:
		break;
	default:
		fprintf(stderr, "??????????\n");
	}
	switch (data[5] & 7)
	{
	case 7:
		fprintf(stderr, "S0 Module\n");
		break;
	case 6:
		fprintf(stderr, "Uk0 Module\n");
		break;
	case 5:
		fprintf(stderr, "Up0 Module\n");
		break;
	case 0:
		break;
	default:
		fprintf(stderr, "??????????\n");
	}

	fprintf(stderr, "Jumper:\n");
	fprintf(stderr, "COM%d\n", ((data[6] >> 6) & 3) + 1);
	fprintf(stderr, "DPRAM-IRQ");
	prtint((data[6] >> 3) & 7);
	fprintf(stderr, "COM-IRQ");
	prtint(data[6] & 7);
}
