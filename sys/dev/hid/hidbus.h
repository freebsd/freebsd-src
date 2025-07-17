/*-
 * Copyright (c) 2019 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 */

#ifndef _HID_HIDBUS_H_
#define _HID_HIDBUS_H_

enum {
	HIDBUS_IVAR_USAGE,
	HIDBUS_IVAR_INDEX,
	HIDBUS_IVAR_FLAGS,
#define	HIDBUS_FLAG_AUTOCHILD	(0<<1)	/* Child is autodiscovered */
#define	HIDBUS_FLAG_CAN_POLL	(1<<1)	/* Child can work during panic */
	HIDBUS_IVAR_DRIVER_INFO,
	HIDBUS_IVAR_LOCK,
};

#define HIDBUS_ACCESSOR(A, B, T)					\
	__BUS_ACCESSOR(hidbus, A, HIDBUS, B, T)

HIDBUS_ACCESSOR(usage,		USAGE,		int32_t)
HIDBUS_ACCESSOR(index,		INDEX,		uint8_t)
HIDBUS_ACCESSOR(flags,		FLAGS,		uint32_t)
HIDBUS_ACCESSOR(driver_info,	DRIVER_INFO,	uintptr_t)
HIDBUS_ACCESSOR(lock,		LOCK,		struct mtx *)

/*
 * The following structure is used when looking up an HID driver for
 * an HID device. It is inspired by the structure called "usb_device_id".
 * which is originated in Linux and ported to FreeBSD.
 */
struct hid_device_id {

	/* Select which fields to match against */
#if BYTE_ORDER == LITTLE_ENDIAN
	uint16_t
		match_flag_page:1,
		match_flag_usage:1,
		match_flag_bus:1,
		match_flag_vendor:1,
		match_flag_product:1,
		match_flag_ver_lo:1,
		match_flag_ver_hi:1,
		match_flag_pnp:1,
		match_flag_unused:8;
#else
	uint16_t
		match_flag_unused:8,
		match_flag_pnp:1,
		match_flag_ver_hi:1,
		match_flag_ver_lo:1,
		match_flag_product:1,
		match_flag_vendor:1,
		match_flag_bus:1,
		match_flag_usage:1,
		match_flag_page:1;
#endif

	/* Used for top level collection usage matches */
	uint16_t page;
	uint16_t usage;

	/* Used for product specific matches; the Version range is inclusive */
	uint8_t idBus;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t idVersion_lo;
	uint16_t idVersion_hi;
	char *idPnP;

	/* Hook for driver specific information */
	uintptr_t driver_info;
};

#define	HID_STD_PNP_INFO					\
  "M16:mask;U16:page;U16:usage;U8:bus;U16:vendor;U16:product;"	\
  "L16:version;G16:version;Z:_HID"
#define HID_PNP_INFO(table)			\
  MODULE_PNP_INFO(HID_STD_PNP_INFO, hidbus, table, table, nitems(table))

#define HID_TLC(pg,usg)				\
  .match_flag_page = 1, .match_flag_usage = 1, .page = (pg), .usage = (usg)

#define HID_BUS(bus)				\
  .match_flag_bus = 1, .idBus = (bus)

#define HID_VENDOR(vend)			\
  .match_flag_vendor = 1, .idVendor = (vend)

#define HID_PRODUCT(prod)			\
  .match_flag_product = 1, .idProduct = (prod)

#define HID_VP(vend,prod)			\
  HID_VENDOR(vend), HID_PRODUCT(prod)

#define HID_BVP(bus,vend,prod)			\
  HID_BUS(bus), HID_VENDOR(vend), HID_PRODUCT(prod)

#define HID_BVPI(bus,vend,prod,info)		\
  HID_BUS(bus), HID_VENDOR(vend), HID_PRODUCT(prod), HID_DRIVER_INFO(info)

#define HID_VERSION_GTEQ(lo)	/* greater than or equal */	\
  .match_flag_ver_lo = 1, .idVersion_lo = (lo)

#define HID_VERSION_LTEQ(hi)	/* less than or equal */	\
  .match_flag_ver_hi = 1, .idVersion_hi = (hi)

#define	HID_PNP(pnp)				\
  .match_flag_pnp = 1, .idPnP = (pnp)

#define HID_DRIVER_INFO(n)			\
  .driver_info = (n)

#define HID_GET_DRIVER_INFO(did)		\
  (did)->driver_info

#define	HIDBUS_LOOKUP_ID(d, h)	hidbus_lookup_id((d), (h), nitems(h))
#define	HIDBUS_LOOKUP_DRIVER_INFO(d, h)		\
	hidbus_lookup_driver_info((d), (h), nitems(h))

/*
 * Walk through all HID items hi belonging Top Level Collection #tlc_index
 */
#define	HIDBUS_FOREACH_ITEM(hd, hi, tlc_index)				\
	for (uint8_t _iter = 0;						\
	    _iter <= (tlc_index) && hid_get_item((hd), (hi));		\
	    _iter += (hi)->kind == hid_endcollection && (hi)->collevel == 0) \
		if (_iter == (tlc_index))

int	hidbus_locate(const void *desc, hid_size_t size, int32_t u,
	    enum hid_kind k, uint8_t tlc_index, uint8_t index,
	    struct hid_location *loc, uint32_t *flags, uint8_t *id,
	    struct hid_absinfo *ai);
bool	hidbus_is_collection(const void *, hid_size_t, int32_t, uint8_t);

const struct hid_device_id *hidbus_lookup_id(device_t,
		    const struct hid_device_id *, int);
struct hid_rdesc_info *hidbus_get_rdesc_info(device_t);
int		hidbus_lookup_driver_info(device_t,
		    const struct hid_device_id *, int);
void		hidbus_set_intr(device_t, hid_intr_t*, void *);
void		hidbus_set_desc(device_t, const char *);
device_t	hidbus_find_child(device_t, int32_t);

/* hidbus HID interface */
int	hid_get_report_descr(device_t, void **, hid_size_t *);
int	hid_set_report_descr(device_t, const void *, hid_size_t);

const struct hid_device_info *hid_get_device_info(device_t);

#endif	/* _HID_HIDBUS_H_ */
