/*
 * Copyright (c) 1993 Atsushi Murai (amurai@spec.co.jp)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Atsushi Murai(amurai@spec.co.jp)``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)isofs_rrip.h
 *	$Id: isofs_rrip.h,v 1.2 1993/10/16 19:26:45 rgrimes Exp $
 */

typedef struct {
	char 	      type		[ISODCL (  0,    1)];
	unsigned char length		[ISODCL (  2,    2)]; /* 711 */
	unsigned char version		[ISODCL (  3,    3)];
} ISO_SUSP_HEADER;

typedef struct {
	ISO_SUSP_HEADER			h;
	char mode_l			[ISODCL (  4,    7)]; /* 731 */
	char mode_m			[ISODCL (  8,   11)]; /* 732 */
	char links_l			[ISODCL ( 12,   15)]; /* 731 */
	char links_m			[ISODCL ( 16,   19)]; /* 732 */
	char uid_l			[ISODCL ( 20,   23)]; /* 731 */
	char uid_m			[ISODCL ( 24,   27)]; /* 732 */
	char gid_l			[ISODCL ( 28,   31)]; /* 731 */
	char gid_m			[ISODCL ( 32,   35)]; /* 732 */
} ISO_RRIP_ATTR;

typedef struct {
	ISO_SUSP_HEADER			h;
	char dev_t_high_l		[ISODCL (  4,    7)]; /* 731 */
	char dev_t_high_m		[ISODCL (  8,   11)]; /* 732 */
	char dev_t_low_l		[ISODCL ( 12,   15)]; /* 731 */
	char dev_t_low_m		[ISODCL ( 16,   19)]; /* 732 */
} ISO_RRIP_DEVICE;

#define	ISO_SUSP_CFLAG_CONTINUE	0x01
#define	ISO_SUSP_CFLAG_CURRENT	0x02
#define	ISO_SUSP_CFLAG_PARENT	0x04
#define	ISO_SUSP_CFLAG_ROOT	0x08
#define	ISO_SUSP_CFLAG_VOLROOT	0x10
#define	ISO_SUSP_CFLAG_HOST	0x20

typedef struct {
	u_char cflag			[ISODCL (  1,    1)];
	u_char clen			[ISODCL (  2,    2)];
	u_char name			[ISODCL (  3,    3)];
} ISO_RRIP_SLINK_COMPONENT;

typedef struct {
	ISO_SUSP_HEADER			h;
	u_char flags			[ISODCL (  4,    4)];
	u_char component		[ISODCL (  5,    5)];
} ISO_RRIP_SLINK;

typedef struct {
	ISO_SUSP_HEADER			h;
	char flags			[ISODCL (  4,    4)];
} ISO_RRIP_ALTNAME;

typedef struct {
	ISO_SUSP_HEADER			h;
	char dir_loc			[ISODCL (  4,    11)]; /* 733 */
} ISO_RRIP_CLINK;

typedef struct {
	ISO_SUSP_HEADER			h;
	char dir_loc			[ISODCL (  4,    11)]; /* 733 */
} ISO_RRIP_PLINK;

typedef struct {
	ISO_SUSP_HEADER			h;
} ISO_RRIP_RELDIR;

#define	ISO_SUSP_TSTAMP_FORM17	0x80
#define	ISO_SUSP_TSTAMP_FORM7	0x00
#define	ISO_SUSP_TSTAMP_CREAT	0x01
#define	ISO_SUSP_TSTAMP_MODIFY	0x02
#define	ISO_SUSP_TSTAMP_ACCESS	0x04
#define	ISO_SUSP_TSTAMP_ATTR	0x08
#define	ISO_SUSP_TSTAMP_BACKUP	0x10
#define	ISO_SUSP_TSTAMP_EXPIRE	0x20
#define	ISO_SUSP_TSTAMP_EFFECT	0x40

typedef struct {
	ISO_SUSP_HEADER			h;
	unsigned char flags		[ISODCL (  4,    4)];
	unsigned char time		[ISODCL (  5,    5)];
} ISO_RRIP_TSTAMP;

typedef struct {
	ISO_SUSP_HEADER			h;
	unsigned char flags		[ISODCL (  4,    4)];
} ISO_RRIP_IDFLAG;

typedef struct {
	ISO_SUSP_HEADER			h;
	unsigned char flags		[ISODCL (  4,    4)];
} ISO_RRIP_EXFLAG;

