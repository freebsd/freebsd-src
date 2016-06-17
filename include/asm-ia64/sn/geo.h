/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef __SYS_GEO_H__
#define __SYS_GEO_H__

/* Include a platform-specific geo.h.  It must define at least:
 *   geoid_t:		Geographic identifier data type
 *   geo_type_t:	Data type for the kind of geoid this is
 *   GEO_TYPE_xxx:	Values for geo_type_t vars, eg. GEO_TYPE_NODE
 *   GEO_MAX_LEN:	The maximum length of a geoid, formatted for printing
 */

#include <asm/sn/sn2/geo.h>

/* Declarations applicable to all platforms */

/* parameter for hwcfg_format_geoid() */
#define GEO_FORMAT_HWGRAPH	1
#define GEO_FORMAT_BRIEF	2

/* (the parameter for hwcfg_format_geoid_compt() is defined in the
 * platform-specific geo.h file) */

/* Routines for manipulating geoid_t values */

extern moduleid_t geo_module(geoid_t g);
extern slabid_t geo_slab(geoid_t g);
extern geo_type_t geo_type(geoid_t g);
extern int geo_valid(geoid_t g);
extern int geo_cmp(geoid_t g0, geoid_t g1);
extern geoid_t geo_new(geo_type_t type, ...);

extern geoid_t hwcfg_parse_geoid(char *buffer);
extern void hwcfg_format_geoid(char *buffer, geoid_t m, int fmt);
extern void hwcfg_format_geoid_compt(char *buffer, geoid_t m, int compt);
extern geoid_t hwcfg_geo_get_self(geo_type_t type);
extern geoid_t hwcfg_geo_get_by_nasid(geo_type_t type, nasid_t nasid);

#endif /* __SYS_GEO_H__ */
