/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_ser_table.c#1 $
 */

/*
 * CAM Target Layer command serialization table.
 *
 * Author: Kim Le
 */

/****************************************************************************/
/* TABLE       ctlSerTbl                                                    */
/*                                                                          */
/*  The matrix which drives the serialization algorithm. The major index    */
/*  (the first, row) into this table is the new command.  The minor index   */
/*  (column) is the older, possibly already running, command.               */
/*                                                                          */
/*  Whenever a new value is added to the IDX_T type, this matrix must be    */
/*  expanded by one row AND one column -- Because of this, some effort      */
/*  should be made to re-use the indexes whenever possible.                 */
/*                                                                          */
/****************************************************************************/

#define	pS	CTL_SER_PASS		/* Pass */
#define	bK	CTL_SER_BLOCK		/* Blocked */
#define	bO	CTL_SER_BLOCKOPT	/* Optional block */
#define	xT	CTL_SER_EXTENT		/* Extent check */
#define	xO	CTL_SER_EXTENTOPT	/* Optional extent check */
#define	xS	CTL_SER_SEQ		/* Sequential check */

const static uint8_t
ctl_serialize_table[CTL_SERIDX_COUNT][CTL_SERIDX_COUNT] = {
/**>IDX_ :: 2nd:TUR RD  WRT UNM SYN MDSN MDSL RQSN INQ RDCP RES LSNS FMT STR*/
/*TUR     */{   pS, pS, pS, pS, pS, bK,  bK,  pS,  pS, pS,  bK, pS,  pS, bK},
/*READ    */{   pS, xS, xT, xO, pS, bK,  bK,  pS,  pS, pS,  bK, pS,  bK, bK},
/*WRITE   */{   pS, xT, xT, xO, pS, bK,  bK,  pS,  pS, pS,  bK, pS,  bK, bK},
/*UNMAP   */{   pS, bO, bO, pS, pS, bK,  bK,  pS,  pS, pS,  bK, pS,  bK, bK},
/*SYNC    */{   pS, pS, bO, pS, pS, bK,  bK,  pS,  pS, pS,  bK, pS,  bK, bK},
/*MD_SNS  */{   bK, bK, bK, bK, bK, pS,  bK,  pS,  pS, pS,  bK, pS,  bK, bK},
/*MD_SEL  */{   bK, bK, bK, bK, bK, bK,  bK,  pS,  pS, pS,  bK, bK,  bK, bK},
/*RQ_SNS  */{   bK, bK, bK, bK, bK, bK,  bK,  bK,  bK, bK,  bK, bK,  pS, bK},
/*INQ     */{   pS, pS, pS, pS, pS, pS,  pS,  pS,  pS, pS,  pS, pS,  pS, pS},
/*RD_CAP  */{   pS, pS, pS, pS, pS, pS,  pS,  pS,  pS, pS,  bK, pS,  bK, bK},
/*RES     */{   bK, bK, bK, bK, bK, bK,  bK,  bK,  pS, pS,  bK, bK,  bK, bK},
/*LOG_SNS */{   pS, pS, pS, pS, pS, pS,  pS,  pS,  pS, pS,  bK, pS,  bK, bK},
/*FORMAT  */{   bK, bK, bK, bK, bK, bK,  bK,  bK,  bK, bK,  bK, bK,  bK, bK},
/*START   */{   bK, bK, bK, bK, bK, bK,  bK,  bK,  bK, pS,  bK, bK,  bK, bK},
};

