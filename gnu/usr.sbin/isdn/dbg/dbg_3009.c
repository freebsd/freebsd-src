static char     rcsid[] = "@(#)$Id: dbg_3009.c,v 1.1 1995/01/25 14:06:18 jkr Exp jkr $";
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
 * $Log: dbg_3009.c,v $
 *
 ******************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include "../../../../sys/gnu/i386/isa/nic3009.h"

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

static
mbx_analyse(mbx_type * mbx)
{
	printf("Type %x SubType %x no %x", mbx->type, mbx->subtype, mbx->number);
	if (mbx->more_data)
		printf(" more");
	printf(" len %d plci %x\n", mbx->data_len, mbx->plci);
}

analyse_3009(dpr_type * dpr)
{
	fprintf(stderr, "card state %x hw conf %x ram conf %x\n", dpr->card_state
		,dpr->hw_config, dpr->ram_config);
	print_stat(dpr->card_state);
	if (dpr->card_state & 4)
		fprintf(stderr, "Layer 1 not active\n");
	if (dpr->card_state & 0x10)
		fprintf(stderr, "Date/Time not set\n");
	if (dpr->card_state & 0x20)
		fprintf(stderr, "ENTITY not loaded\n");
	if (dpr->card_state & 0x40)
		fprintf(stderr, "out of sync.\n");
	fprintf(stderr, "Hardware Configuration:\n");
	if (dpr->hw_config & 0x80)
		fprintf(stderr, "No ");
	/*
	 * fprintf(stderr,"DRAM-Module\n"); if(dpr->ram_config&1)
	 * fprintf(stderr,"256 KB SRAM\n"); switch(dpr->ram_config) { case 4:
	 * case 5: fprintf(stderr,"1 MB DRAM\n"); break; case 16: case 17:
	 * fprintf(stderr,"1 MB DRAM\n"); }
	 */
	switch ((dpr->hw_config >> 5) & 3)
	{
	case 3:
		fprintf(stderr, "Modem Module\n");
		break;
	case 0:
		break;
	default:
		fprintf(stderr, "??????????\n");
	}
	switch ((dpr->hw_config >> 3) & 3)
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
	switch (dpr->hw_config & 7)
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
	if (dpr->up_mbx.msg_flag)
	{
		printf("Up Mailbox activ:\n");
		mbx_analyse(&dpr->up_mbx);
	}
	if (dpr->dn_mbx.msg_flag)
	{
		printf("Down Mailbox activ:\n");
		mbx_analyse(&dpr->dn_mbx);
	}
	if (dpr->up_mbx.msg_flag == 0)
	{
		printf("Up Mailbox not activ:\n");
		mbx_analyse(&dpr->up_mbx);
	}
	if (dpr->dn_mbx.msg_flag == 0)
	{
		printf("Down Mailbox not activ:\n");
		mbx_analyse(&dpr->dn_mbx);
	}
}
