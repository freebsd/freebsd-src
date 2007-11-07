/*
    ipv6cp.h - PPP IPV6 Control Protocol.
    Copyright (C) 1999  Tommi Komulainen <Tommi.Komulainen@iki.fi>

    Redistribution and use in source and binary forms are permitted
    provided that the above copyright notice and this paragraph are
    duplicated in all such forms.  The name of the author may not be
    used to endorse or promote products derived from this software
    without specific prior written permission.
    THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
    WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*/

/*  Original version, based on RFC2023 :

    Copyright (c) 1995, 1996, 1997 Francis.Dupont@inria.fr, INRIA Rocquencourt,
    Alain.Durand@imag.fr, IMAG,
    Jean-Luc.Richier@imag.fr, IMAG-LSR.

    Copyright (c) 1998, 1999 Francis.Dupont@inria.fr, GIE DYADE,
    Alain.Durand@imag.fr, IMAG,
    Jean-Luc.Richier@imag.fr, IMAG-LSR.

    Ce travail a été fait au sein du GIE DYADE (Groupement d'Intérêt
    Économique ayant pour membres BULL S.A. et l'INRIA).

    Ce logiciel informatique est disponible aux conditions
    usuelles dans la recherche, c'est-à-dire qu'il peut
    être utilisé, copié, modifié, distribué à l'unique
    condition que ce texte soit conservé afin que
    l'origine de ce logiciel soit reconnue.

    Le nom de l'Institut National de Recherche en Informatique
    et en Automatique (INRIA), de l'IMAG, ou d'une personne morale
    ou physique ayant participé à l'élaboration de ce logiciel ne peut
    être utilisé sans son accord préalable explicite.

    Ce logiciel est fourni tel quel sans aucune garantie,
    support ou responsabilité d'aucune sorte.
    Ce logiciel est dérivé de sources d'origine
    "University of California at Berkeley" et
    "Digital Equipment Corporation" couvertes par des copyrights.

    L'Institut d'Informatique et de Mathématiques Appliquées de Grenoble (IMAG)
    est une fédération d'unités mixtes de recherche du CNRS, de l'Institut National
    Polytechnique de Grenoble et de l'Université Joseph Fourier regroupant
    sept laboratoires dont le laboratoire Logiciels, Systèmes, Réseaux (LSR).

    This work has been done in the context of GIE DYADE (joint R & D venture
    between BULL S.A. and INRIA).

    This software is available with usual "research" terms
    with the aim of retain credits of the software. 
    Permission to use, copy, modify and distribute this software for any
    purpose and without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies,
    and the name of INRIA, IMAG, or any contributor not be used in advertising
    or publicity pertaining to this material without the prior explicit
    permission. The software is provided "as is" without any
    warranties, support or liabilities of any kind.
    This software is derived from source code from
    "University of California at Berkeley" and
    "Digital Equipment Corporation" protected by copyrights.

    Grenoble's Institute of Computer Science and Applied Mathematics (IMAG)
    is a federation of seven research units funded by the CNRS, National
    Polytechnic Institute of Grenoble and University Joseph Fourier.
    The research unit in Software, Systems, Networks (LSR) is member of IMAG.
*/

/*
 * Derived from :
 *
 *
 * ipcp.h - IP Control Protocol definitions.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: ipv6cp.h,v 1.3 1999/09/30 19:57:45 masputra Exp $
 * $FreeBSD$
 */

/*
 * Options.
 */
#define CI_IFACEID	1	/* Interface Identifier */
#define CI_COMPRESSTYPE	2	/* Compression Type     */

/* No compression types yet defined.
 *#define IPV6CP_COMP	0x004f
 */
typedef struct ipv6cp_options {
    int neg_ifaceid;		/* Negotiate interface identifier? */
    int req_ifaceid;		/* Ask peer to send interface identifier? */
    int accept_local;		/* accept peer's value for iface id? */
    int opt_local;		/* ourtoken set by option */
    int opt_remote;		/* histoken set by option */
    int use_ip;			/* use IP as interface identifier */
#if defined(SOL2)
    int use_persistent;		/* use uniquely persistent value for address */
#endif /* defined(SOL2) */
    int neg_vj;			/* Van Jacobson Compression? */
    u_short vj_protocol;	/* protocol value to use in VJ option */
    eui64_t ourid, hisid;	/* Interface identifiers */
} ipv6cp_options;

extern fsm ipv6cp_fsm[];
extern ipv6cp_options ipv6cp_wantoptions[];
extern ipv6cp_options ipv6cp_gotoptions[];
extern ipv6cp_options ipv6cp_allowoptions[];
extern ipv6cp_options ipv6cp_hisoptions[];

extern struct protent ipv6cp_protent;

extern int setifaceid(char **arg);
