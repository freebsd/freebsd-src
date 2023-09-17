/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

#ifndef	_LINUXKPI_LINUX_PCI_IDS_H
#define	_LINUXKPI_LINUX_PCI_IDS_H

#define	PCI_CLASS_NETWORK_OTHER		0x0280

#define	PCI_BASE_CLASS_DISPLAY		0x03
#define	PCI_CLASS_DISPLAY_VGA		0x0300
#define	PCI_CLASS_DISPLAY_OTHER		0x0380

#define	PCI_BASE_CLASS_BRIDGE		0x06
#define	PCI_CLASS_BRIDGE_ISA		0x0601


/* XXX We should really generate these and use them throughout the tree. */

#define	PCI_VENDOR_ID_APPLE		0x106b
#define	PCI_VENDOR_ID_ASUSTEK		0x1043
#define	PCI_VENDOR_ID_ATHEROS		0x168c
#define	PCI_VENDOR_ID_ATI		0x1002
#define	PCI_VENDOR_ID_BROADCOM			0x14e4
#define	PCI_VENDOR_ID_DELL		0x1028
#define	PCI_VENDOR_ID_HP		0x103c
#define	PCI_VENDOR_ID_IBM		0x1014
#define	PCI_VENDOR_ID_INTEL		0x8086
#define	PCI_VENDOR_ID_MEDIATEK		0x14c3
#define	PCI_VENDOR_ID_MELLANOX			0x15b3
#define	PCI_VENDOR_ID_QCOM		0x17cb
#define	PCI_VENDOR_ID_REALTEK			0x10ec
#define	PCI_VENDOR_ID_REDHAT_QUMRANET	0x1af4
#define	PCI_VENDOR_ID_SERVERWORKS	0x1166
#define	PCI_VENDOR_ID_SONY		0x104d
#define	PCI_VENDOR_ID_TOPSPIN			0x1867
#define	PCI_VENDOR_ID_UBIQUITI			0x0777
#define	PCI_VENDOR_ID_VIA		0x1106
#define	PCI_SUBVENDOR_ID_REDHAT_QUMRANET	0x1af4
#define	PCI_DEVICE_ID_ATI_RADEON_QY	0x5159
#define	PCI_DEVICE_ID_MELLANOX_TAVOR		0x5a44
#define	PCI_DEVICE_ID_MELLANOX_TAVOR_BRIDGE	0x5a46
#define	PCI_DEVICE_ID_MELLANOX_ARBEL_COMPAT	0x6278
#define	PCI_DEVICE_ID_MELLANOX_ARBEL		0x6282
#define	PCI_DEVICE_ID_MELLANOX_SINAI_OLD	0x5e8c
#define	PCI_DEVICE_ID_MELLANOX_SINAI		0x6274
#define	PCI_SUBDEVICE_ID_QEMU		0x1100

#endif	/* _LINUXKPI_LINUX_PCI_IDS_H */
