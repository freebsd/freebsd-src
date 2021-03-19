/*
 * Platform definitions for Radiotap parser
 * Copyright (c) 2021, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "includes.h"
#include "common.h"

#define get_unaligned_le16(p)	WPA_GET_LE16((void *) (p))
#define get_unaligned_le32(p)	WPA_GET_LE32((void *) (p))

#endif /* PLATFORM_H */
