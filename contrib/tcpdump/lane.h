/*
 * Marko Kiiskila carnil@cs.tut.fi 
 * 
 * Tampere University of Technology - Telecommunications Laboratory
 *
 * Permission to use, copy, modify and distribute this
 * software and its documentation is hereby granted,
 * provided that both the copyright notice and this
 * permission notice appear in all copies of the software,
 * derivative works or modified versions, and any portions
 * thereof, that both notices appear in supporting
 * documentation, and that the use of this software is
 * acknowledged in any publications resulting from using
 * the software.
 * 
 * TUT ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION AND DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS
 * SOFTWARE.
 * 
 */

/* $Id: lane.h,v 1.2 1999/11/21 16:35:11 assar Exp $ */

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN 6
#endif

struct lecdatahdr_8023 {
  u_short le_header;
  u_char h_dest[ETHER_ADDR_LEN];
  u_char h_source[ETHER_ADDR_LEN];
  u_short h_type;
};
