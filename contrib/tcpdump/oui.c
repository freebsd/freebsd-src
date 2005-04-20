/* 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Original code by Hannes Gredler (hannes@juniper.net)
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/oui.c,v 1.2.2.1 2004/02/06 14:38:51 hannes Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>
#include "interface.h"
#include "oui.h"

/* FIXME complete OUI list using a script */

struct tok oui_values[] = {
    { 0x009069, "Juniper"},
    { 0x00000c, "Cisco"},
};

/* list taken from ethereal/packet-radius.c */

struct tok smi_values[] = {
    { SMI_ACC,                  "ACC"},
    { SMI_CISCO,                "Cisco"},
    { SMI_SHIVA,                "Shiva"},
    { SMI_MICROSOFT,            "Microsoft"},
    { SMI_LIVINGSTON,           "Livingston"},
    { SMI_3COM,                 "3Com"},
    { SMI_ASCEND,               "Ascend"},
    { SMI_BAY,                  "Bay Networks"},
    { SMI_FOUNDRY,              "Foundry"},
    { SMI_VERSANET,             "Versanet"},
    { SMI_REDBACK,              "Redback"},
    { SMI_JUNIPER,              "Juniper Networks"},
    { SMI_APTIS,                "Aptis"},
    { SMI_COSINE,               "CoSine Communications"},
    { SMI_SHASTA,               "Shasta"},
    { SMI_NOMADIX,              "Nomadix"},
    { SMI_UNISPHERE,            "Unisphere Networks"},
    { SMI_ISSANNI,              "Issanni Communications"},
    { SMI_QUINTUM,              "Quintum"},
    { SMI_COLUBRIS,             "Colubris"},
    { SMI_COLUMBIA_UNIVERSITY,  "Columbia University"},
    { SMI_THE3GPP,              "3GPP"},
    { 0, NULL }
};
