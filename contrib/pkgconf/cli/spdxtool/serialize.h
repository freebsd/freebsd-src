/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#include <stdlib.h>
#include <string.h>
#include "util.h"

#ifndef CLI__SPDXTOOL__SERIALIZE_H
#define CLI__SPDXTOOL__SERIALIZE_H

#ifdef __cplusplus
extern "C" {
#endif

void
spdxtool_serialize_parm_and_string(pkgconf_buffer_t *buffer, char *parm, char *string, unsigned int level, bool comma);

void
spdxtool_serialize_parm_and_char(pkgconf_buffer_t *buffer, char *parm, char ch, unsigned int level, bool comma);

void
spdxtool_serialize_parm_and_int(pkgconf_buffer_t *buffer, char *parm, int integer, unsigned int level, bool comma);

void
spdxtool_serialize_string(pkgconf_buffer_t *buffer, char *string, unsigned int level, bool comma);

void
spdxtool_serialize_obj_start(pkgconf_buffer_t *buffer, unsigned int level);

void
spdxtool_serialize_obj_end(pkgconf_buffer_t *buffer, unsigned int level, bool comma);

void
spdxtool_serialize_array_start(pkgconf_buffer_t *buffer, unsigned int level);

void
spdxtool_serialize_array_end(pkgconf_buffer_t *buffer, unsigned int level, bool comma);

#ifdef __cplusplus
}
#endif

#endif
