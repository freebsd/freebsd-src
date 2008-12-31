/*-
 * Copyright (c) 2000, 2001 Hellmuth Michaelis. All rights reserved.
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
 */

/*---------------------------------------------------------------------------
 *
 *      i4b_iwic - isdn4bsd Winbond W6692 driver
 *      ----------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/iwic/i4b_iwic_ext.h,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $
 *
 *      last edit-date: [Sun Jan 21 11:09:14 2001]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_IWIC_EXT_H_
#define _I4B_IWIC_EXT_H_

#include <i4b/include/i4b_l3l4.h>

void iwic_set_linktab(int unit, int channel, drvr_link_t * dlt);
isdn_link_t *iwic_ret_linktab(int unit, int channel);

int iwic_ph_data_req(int unit, struct mbuf *m, int freeflag);
int iwic_ph_activate_req(int unit);
int iwic_mph_command_req(int unit, int command, void *parm);

#endif /* _I4B_IWIC_EXT_H_ */
