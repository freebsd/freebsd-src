static char     rcsid[] = "@(#)$Id: scsi_nic.h,v 1.1 1995/01/25 14:06:18 jkr Exp jkr $";
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
 * $Log: scsi_nic.h,v $
 *
 ******************************************************************************/

/*
 * This file defines the NICCY 5000 Interface.
 * Copyright Dr. Neuhaus GmbH, Hamburg and Dietmar Friede
 *
*/
#define	GET_MSG_COMMAND		0x08
#define PUT_MSG_COMMAND		0x0a

#pragma pack (1)
struct scsi_msg
{
	u_char	op_code;
	u_char	dummy;
	u_char	len[3];
	u_char	control;
};

typedef struct
{
	unsigned char	Type;
	unsigned char	SubType;
	unsigned short	Number ;
	unsigned char	MoreData ;
	unsigned char	Reserved[1] ;
	unsigned short	DataLen ;
	unsigned short	PLCI;
} Header;

#define SNIC_BUF_SIZE 2048+15

typedef struct
{
	Header h;
	unsigned char	Data[SNIC_BUF_SIZE];
} Buffer;

#pragma pack ()

