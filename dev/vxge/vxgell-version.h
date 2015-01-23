/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	_VXGELL_VERSION_H_
#define	_VXGELL_VERSION_H_

#define	XGELL_VERSION_MAJOR     2
#define	XGELL_VERSION_MINOR     5
#define	XGELL_VERSION_FIX       1
#define	XGELL_VERSION_BUILD	GENERATED_BUILD_VERSION

#define	VXGE_FW_VERSION(major, minor, build)	\
	((major << 16) + (minor << 8) + build)

#define	VXGE_FW_MAJ_MIN_VERSION(major, minor)	\
	((major << 16) + (minor << 8))

/* Adapter should be running with this fw version for using FW_UPGRADE API's */
#define	VXGE_BASE_FW_MAJOR_VERSION	1
#define	VXGE_BASE_FW_MINOR_VERSION	4
#define	VXGE_BASE_FW_BUILD_VERSION	4

#define	VXGE_BASE_FW_VERSION					\
	VXGE_FW_VERSION(VXGE_BASE_FW_MAJOR_VERSION,		\
			VXGE_BASE_FW_MINOR_VERSION,		\
			VXGE_BASE_FW_BUILD_VERSION)

#define	VXGE_DRV_FW_VERSION					\
	VXGE_FW_VERSION(VXGE_MIN_FW_MAJOR_VERSION,		\
			VXGE_MIN_FW_MINOR_VERSION,		\
			VXGE_MIN_FW_BUILD_NUMBER)

#define	VXGE_DRV_FW_MAJ_MIN_VERSION				\
	VXGE_FW_MAJ_MIN_VERSION(VXGE_MIN_FW_MAJOR_VERSION,	\
				VXGE_MIN_FW_MINOR_VERSION)

#define	VXGE_FW_ARRAY_NAME	X3fw_ncf
#define	VXGE_COPYRIGHT		"Copyright(c) 2002-2011 Exar Corp.\n"
#define	VXGE_ADAPTER_NAME	"Neterion x3100 10GbE PCIe Server Adapter " \
				"(Rev %d)"

#endif				/* _VXGELL_VERSION_H_ */
