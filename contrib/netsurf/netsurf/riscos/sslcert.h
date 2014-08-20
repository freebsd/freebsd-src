/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * SSL certificate viewer (interface).
 */

#ifndef _NETSURF_RISCOS_SSLCERT_H_
#define _NETSURF_RISCOS_SSLCERT_H_

void ro_gui_cert_preinitialise(void);
void ro_gui_cert_postinitialise(void);
void ro_gui_cert_open(struct tree *tree, struct node *node);

#endif

