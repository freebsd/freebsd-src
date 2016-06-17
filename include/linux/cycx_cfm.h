/*
* cycx_cfm.h	Cyclom 2X WAN Link Driver.
*		Definitions for the Cyclom 2X Firmware Module (CFM).
*
* Author:	Arnaldo Carvalho de Melo <acme@conectiva.com.br>
*
* Copyright:	(c) 1998-2000 Arnaldo Carvalho de Melo
*
* Based on sdlasfm.h by Gene Kozin <74604.152@compuserve.com>
*
*		This program is free software; you can redistribute it and/or
*		modify it under the terms of the GNU General Public License
*		as published by the Free Software Foundation; either version
*		2 of the License, or (at your option) any later version.
* ============================================================================
* 1998/08/08	acme		Initial version.
*/
#ifndef	_CYCX_CFM_H
#define	_CYCX_CFM_H

/* Defines */

#define	CFM_VERSION	2
#define	CFM_SIGNATURE	"CFM - Cyclades CYCX Firmware Module"

/* min/max */
#define	CFM_IMAGE_SIZE	0x20000	/* max size of CYCX code image file */
#define	CFM_DESCR_LEN	256	/* max length of description string */
#define	CFM_MAX_CYCX	1	/* max number of compatible adapters */
#define	CFM_LOAD_BUFSZ	0x400	/* buffer size for reset code (buffer_load) */

/* Firmware Commands */
#define GEN_POWER_ON	0x1280

#define GEN_SET_SEG	0x1401	/* boot segment setting. */
#define GEN_BOOT_DAT	0x1402	/* boot data. */
#define GEN_START	0x1403	/* board start. */
#define GEN_DEFPAR	0x1404	/* buffer length for boot. */

/* Adapter Types */
#define CYCX_2X		2
/* for now only the 2X is supported, no plans to support 8X or 16X */
#define CYCX_8X		8
#define CYCX_16X	16

#define	CFID_X25_2X	5200

/* Data Types */

typedef struct	cfm_info		/* firmware module information */
{
	unsigned short	codeid;		/* firmware ID */
	unsigned short	version;	/* firmware version number */
	unsigned short	adapter[CFM_MAX_CYCX]; /* compatible adapter types */
	unsigned long	memsize;	/* minimum memory size */
	unsigned short	reserved[2];	/* reserved */
	unsigned short	startoffs;	/* entry point offset */
	unsigned short	winoffs;	/* dual-port memory window offset */
	unsigned short	codeoffs;	/* code load offset */
	unsigned long	codesize;	/* code size */
	unsigned short	dataoffs;	/* configuration data load offset */
	unsigned long	datasize;	/* configuration data size */
} cfm_info_t;

typedef struct cfm			/* CYCX firmware file structure */
{
	char		signature[80];	/* CFM file signature */
	unsigned short	version;	/* file format version */
	unsigned short	checksum;	/* info + image */
	unsigned short	reserved[6];	/* reserved */
	char		descr[CFM_DESCR_LEN]; /* description string */
	cfm_info_t	info;		/* firmware module info */
	unsigned char	image[1];	/* code image (variable size) */
} cfm_t;

typedef struct cycx_header_s {
	unsigned long  reset_size;
	unsigned long  data_size;
	unsigned long  code_size;
} cycx_header_t;

#endif	/* _CYCX_CFM_H */
