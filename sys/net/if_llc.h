/*
 * Copyright (c) 1988 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
 *      from: @(#)if_llc.h	7.2 (Berkeley) 6/28/90
 *	$Id: if_llc.h,v 1.2 1993/10/16 17:43:17 rgrimes Exp $
 */

/*
 * IEEE 802.2 Link Level Control headers, for use in conjunction with
 * 802.{3,4,5} media access control methods.
 *
 * Headers here do not use bit fields due to shortcommings in many
 * compilers.
 */

struct llc {
	u_char	llc_dsap;
	u_char	llc_ssap;
	union {
	    struct {
		u_char control;
		u_char format_id;
		u_char class;
		u_char window_x2;
	    } type_u;
	    struct {
		u_char num_snd_x2;
		u_char num_rcv_x2;
	    } type_i;
	    struct {
		u_char control;
		u_char num_rcv_x2;
	    } type_s;
	    struct {
		u_char control;
		u_char org_code[3];
		u_short ether_type;
	    } type_snap;
	} llc_un;
};
#define llc_control llc_un.type_u.control
#define llc_fid llc_un.type_u.format_id
#define llc_class llc_un.type_u.class
#define llc_window llc_un.type_u.window_x2

#define LLC_UI		0x3
#define LLC_UI_P	0x13
#define LLC_XID		0xaf
#define LLC_XID_P	0xbf
#define LLC_TEST	0xe3
#define LLC_TEST_P	0xf3

#define LLC_ISO_LSAP	0xfe
#define LLC_SNAP_LSAP	0xaa
