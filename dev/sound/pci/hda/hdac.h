/*-
 * Copyright (c) 2006 Stephane E. Potvin <sepotvin@videotron.ca>
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
 *
 * $FreeBSD$
 */

#ifndef _HDAC_H_
#define _HDAC_H_


#if 0
/****************************************************************************
 * Miscellanious defines
 ****************************************************************************/

/****************************************************************************
 * Helper Macros
 ****************************************************************************/

/****************************************************************************
 * Simplified Accessors for HDA devices
 ****************************************************************************/
enum hdac_device_ivars {
    HDAC_IVAR_CODEC_ID,
    HDAC_IVAR_NODE_ID,
    HDAC_IVAR_VENDOR_ID,
    HDAC_IVAR_DEVICE_ID,
    HDAC_IVAR_REVISION_ID,
    HDAC_IVAR_STEPPING_ID,
    HDAC_IVAR_NODE_TYPE,
};

#define HDAC_ACCESSOR(var, ivar, type)					\
    __BUS_ACCESSOR(hdac, var, HDAC, ivar, type)

HDAC_ACCESSOR(codec_id,		CODEC_ID,	uint8_t);
HDAC_ACCESSOR(node_id,		NODE_ID,	uint8_t);
HDAC_ACCESSOR(vendor_id,	VENDOR_ID,	uint16_t);
HDAC_ACCESSOR(device_id,	DEVICE_ID,	uint16_t);
HDAC_ACCESSOR(revision_id,	REVISION_ID,	uint8_t);
HDAC_ACCESSOR(stepping_id,	STEPPING_ID,	uint8_t);
HDAC_ACCESSOR(node_type,	NODE_TYPE,	uint8_t);
#endif

#define PCIS_MULTIMEDIA_HDA    0x03

#endif
