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
 *	i4b_q932fac.h - Q932 facility handling header file
 *	--------------------------------------------------
 *
 *	$Id: i4b_q932fac.h,v 1.8 2000/04/27 09:25:22 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Thu Apr 27 11:07:52 2000]
 *
 *---------------------------------------------------------------------------
 *
 *	- Q.932 (03/93) Generic Procedures for the Control of
 *		ISDN Supplementaty Services
 *	- Q.950 (03/93) Supplementary Services Protocols, Structure and
 *		General Principles
 *	- ETS 300 179 (10/92) Advice Of Charge: charging information during
 *		the call (AOC-D) supplementary service Service description
 *	- ETS 300 180 (10/92) Advice Of Charge: charging information at the
 *		end of call (AOC-E) supplementary service Service description
 *	- ETS 300 181 (04/93) Advice Of Charge (AOC) supplementary service
 *		Functional capabilities and information flows
 *	- ETS 300 182 (04/93) Advice Of Charge (AOC) supplementary service
 *		Digital Subscriber Signalling System No. one (DSS1) protocol
 *	- X.208 Specification of Abstract Syntax Notation One (ASN.1)
 *	- X.209 Specification of Basic Encoding Rules for
 *		Abstract Syntax Notation One (ASN.1) 
 *	- "ASN.1 Abstract Syntax Notation One", Walter Gora, DATACOM-Verlag
 *		1992, 3rd Edition (ISBN 3-89238-062-7) (german !)
 *
 *---------------------------------------------------------------------------*/

/* protocols */
#define FAC_PROTO_ROP		0x11
#define FAC_PROTO_CMIP		0x12
#define FAC_PROTO_ACSE		0x13

/* tag classes */
#define FAC_TAGCLASS_UNI	0x00
#define FAC_TAGCLASS_APW	0x01
#define FAC_TAGCLASS_COS	0x02
#define FAC_TAGCLASS_PRU	0x03

/* tag forms */
#define FAC_TAGFORM_PRI		0x00
#define FAC_TAGFORM_CON		0x01

/* class UNIVERSAL values */
#define FAC_CODEUNI_BOOL	1
#define FAC_CODEUNI_INT		2
#define FAC_CODEUNI_BITS	3
#define FAC_CODEUNI_OCTS	4
#define FAC_CODEUNI_NULL	5
#define FAC_CODEUNI_OBJI	6
#define FAC_CODEUNI_OBJD	7
#define FAC_CODEUNI_EXT		8
#define FAC_CODEUNI_REAL	9
#define FAC_CODEUNI_ENUM	10
#define FAC_CODEUNI_R11		11
#define FAC_CODEUNI_R12		12
#define FAC_CODEUNI_R13		13
#define FAC_CODEUNI_R14		14
#define FAC_CODEUNI_R15		15
#define FAC_CODEUNI_SEQ		16
#define FAC_CODEUNI_SET		17
#define FAC_CODEUNI_NSTR	18
#define FAC_CODEUNI_PSTR	19
#define FAC_CODEUNI_TSTR	20
#define FAC_CODEUNI_VSTR	21
#define FAC_CODEUNI_ISTR	22
#define FAC_CODEUNI_UTIME	23
#define FAC_CODEUNI_GTIME	24
#define FAC_CODEUNI_GSTR	25
#define FAC_CODEUNI_VISTR	26
#define FAC_CODEUNI_GNSTR	27

/* operation values */
#define FAC_OPVAL_UUS		1
#define FAC_OPVAL_CUG		2
#define FAC_OPVAL_MCID		3
#define FAC_OPVAL_BTPY		4
#define FAC_OPVAL_ETPY		5
#define FAC_OPVAL_ECT		6

#define FAC_OPVAL_DIV_ACT	7
#define FAC_OPVAL_DIV_DEACT	8
#define FAC_OPVAL_DIV_ACTSN	9
#define FAC_OPVAL_DIV_DEACTSN	10
#define FAC_OPVAL_DIV_INTER	11
#define FAC_OPVAL_DIV_INFO	12
#define FAC_OPVAL_DIV_CALLDEF	13
#define FAC_OPVAL_DIV_CALLRER	14
#define FAC_OPVAL_DIV_LINF2	15
#define FAC_OPVAL_DIV_INVS	16
#define FAC_OPVAL_DIV_INTER1	17
#define FAC_OPVAL_DIV_LINF1	18
#define FAC_OPVAL_DIV_LINF3	19

#define FAC_OPVAL_ER_CRCO	20
#define FAC_OPVAL_ER_MGMT	21
#define FAC_OPVAL_ER_CANC	22

#define FAC_OPVAL_MLPP_QUERY	24
#define FAC_OPVAL_MLPP_CALLR	25
#define FAC_OPVAL_MLPP_CALLP	26

#define FAC_OPVAL_AOC_REQ	30
#define FAC_OPVAL_AOC_S_CUR	31
#define FAC_OPVAL_AOC_S_SPC	32
#define FAC_OPVAL_AOC_D_CUR	33
#define FAC_OPVAL_AOC_D_UNIT	34
#define FAC_OPVAL_AOC_E_CUR	35
#define FAC_OPVAL_AOC_E_UNIT	36
#define FAC_OPVAL_AOC_IDOFCRG	37

#define FAC_OPVAL_CONF_BEG	40
#define FAC_OPVAL_CONF_ADD	41
#define FAC_OPVAL_CONF_SPLIT	42
#define FAC_OPVAL_CONF_DROP	43
#define FAC_OPVAL_CONF_ISOLATE	44
#define FAC_OPVAL_CONF_REATT	45
#define FAC_OPVAL_CONF_PDISC	46
#define FAC_OPVAL_CONF_FCONF	47
#define FAC_OPVAL_CONF_END	48
#define FAC_OPVAL_CONF_IDCFE	49

#define FAC_OPVAL_REVC_REQ	60

enum states {
	ST_EXP_COMP_TYP,
	ST_EXP_INV_ID,
	ST_EXP_OP_VAL,
	ST_EXP_INFO,
	ST_EXP_RUL,
	ST_EXP_RU,
	ST_EXP_RNOU,
	ST_EXP_TOCI,
	ST_EXP_DBID,	
	
	ST_EXP_NIX	
};

/* EOF */

