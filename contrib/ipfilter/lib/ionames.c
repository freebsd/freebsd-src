/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: ionames.c,v 1.7 2002/01/28 06:50:46 darrenr Exp $
 */
#include "ipf.h"


struct	ipopt_names	ionames[] ={
	{ IPOPT_NOP,	0x000001,	1,	"nop" },	/* RFC791 */
	{ IPOPT_RR,	0x000002,	7,	"rr" },		/* 1 route */
	{ IPOPT_ZSU,	0x000004,	3,	"zsu" },	/* size ?? */
	{ IPOPT_MTUP,	0x000008,	3,	"mtup" },	/* RFC1191 */
	{ IPOPT_MTUR,	0x000010,	3,	"mtur" },	/* RFC1191 */
	{ IPOPT_ENCODE,	0x000020,	3,	"encode" },	/* size ?? */
	{ IPOPT_TS,	0x000040,	8,	"ts" },		/* 1 TS */
	{ IPOPT_TR,	0x000080,	3,	"tr" },		/* RFC1393 */
	{ IPOPT_SECURITY,0x000100,	11,	"sec" },	/* RFC1108 */
	{ IPOPT_SECURITY,0x000100,	11,	"sec-class" },	/* RFC1108 */
	{ IPOPT_LSRR,	0x000200,	7,	"lsrr" },	/* 1 route */
	{ IPOPT_E_SEC,	0x000400,	3,	"e-sec" },	/* RFC1108 */
	{ IPOPT_CIPSO,	0x000800,	3,	"cipso" },	/* size ?? */
	{ IPOPT_SATID,	0x001000,	4,	"satid" },	/* RFC791 */
	{ IPOPT_SSRR,	0x002000,	7,	"ssrr" },	/* 1 route */
	{ IPOPT_ADDEXT,	0x004000,	3,	"addext" },	/* IPv7 ?? */
	{ IPOPT_VISA,	0x008000,	3,	"visa" },	/* size ?? */
	{ IPOPT_IMITD,	0x010000,	3,	"imitd" },	/* size ?? */
	{ IPOPT_EIP,	0x020000,	3,	"eip" },	/* RFC1385 */
	{ IPOPT_FINN,	0x040000,	3,	"finn" },	/* size ?? */
	{ IPOPT_DPS,	0x080000,	3,	"dps" },	/* size ?? */
	{ IPOPT_SDB,	0x100000,	3,	"sdb" },	/* size ?? */
	{ IPOPT_NSAPA,	0x200000,	3,	"nsapa" },	/* size ?? */
	{ IPOPT_RTRALRT,0x400000,	3,	"rtralrt" },	/* RFC2113 */
	{ IPOPT_UMP,	0x800000,	3,	"ump" },	/* size ?? */
	{ 0, 		0,	0,	(char *)NULL }     /* must be last */
};
