/*-
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * Portions of this software were developed by Björn Zeeb
 * under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Portions of this software may have been developed with reference to
 * the SD Simplified Specification.  The following disclaimer may apply:
 *
 * The following conditions apply to the release of the simplified
 * specification ("Simplified Specification") by the SD Card Association and
 * the SD Group. The Simplified Specification is a subset of the complete SD
 * Specification which is owned by the SD Card Association and the SD
 * Group. This Simplified Specification is provided on a non-confidential
 * basis subject to the disclaimers below. Any implementation of the
 * Simplified Specification may require a license from the SD Card
 * Association, SD Group, SD-3C LLC or other third parties.
 *
 * Disclaimers:
 *
 * The information contained in the Simplified Specification is presented only
 * as a standard specification for SD Cards and SD Host/Ancillary products and
 * is provided "AS-IS" without any representations or warranties of any
 * kind. No responsibility is assumed by the SD Group, SD-3C LLC or the SD
 * Card Association for any damages, any infringements of patents or other
 * right of the SD Group, SD-3C LLC, the SD Card Association or any third
 * parties, which may result from its use. No license is granted by
 * implication, estoppel or otherwise under any patent or other rights of the
 * SD Group, SD-3C LLC, the SD Card Association or any third party. Nothing
 * herein shall be construed as an obligation by the SD Group, the SD-3C LLC
 * or the SD Card Association to disclose or distribute any technical
 * information, know-how or other confidential information to any third party.
 *
 */

#ifndef _SDIOB_H
#define	_SDIOB_H

#define	SDIOB_NAME			sdiob
#define	_SDIOB_NAME_S(x)		__STRING(x)
#define	SDIOB_NAME_S			_SDIOB_NAME_S(SDIOB_NAME)

#ifdef _SYS_BUS_H_
/* Ivars for sdiob. */
enum sdiob_dev_enum {
	SDIOB_IVAR_SUPPORT_MULTIBLK,
	SDIOB_IVAR_FUNCTION,
	SDIOB_IVAR_FUNCNUM,
	SDIOB_IVAR_CLASS,
	SDIOB_IVAR_VENDOR,
	SDIOB_IVAR_DEVICE,
	SDIOB_IVAR_DRVDATA,
};

#define	SDIOB_ACCESSOR(var, ivar, type)				\
    __BUS_ACCESSOR(sdio, var, SDIOB, ivar, type)

SDIOB_ACCESSOR(support_multiblk,SUPPORT_MULTIBLK,	bool)
SDIOB_ACCESSOR(function,	FUNCTION,		struct sdio_func *)
SDIOB_ACCESSOR(funcnum,		FUNCNUM,		uint8_t)
SDIOB_ACCESSOR(class,		CLASS,			uint8_t)
SDIOB_ACCESSOR(vendor,		VENDOR,			uint16_t)
SDIOB_ACCESSOR(device,		DEVICE,			uint16_t)
SDIOB_ACCESSOR(drvdata,		DRVDATA,		void *)

#undef SDIOB_ACCESSOR
#endif  /* _SYS_BUS_H_ */

#endif	/* _SDIOB_H */
