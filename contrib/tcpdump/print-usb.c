/*
 * Copyright 2009 Bert Vermeulen <bert@biot.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by Paolo Abeni.''
 * The name of author may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Support for USB packets
 *
 */

/* \summary: USB printer */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"

#ifdef DLT_USB_LINUX
/*
 * possible transfer mode
 */
#define URB_TRANSFER_IN   0x80
#define URB_ISOCHRONOUS   0x0
#define URB_INTERRUPT     0x1
#define URB_CONTROL       0x2
#define URB_BULK          0x3

/*
 * possible event type
 */
#define URB_SUBMIT        'S'
#define URB_COMPLETE      'C'
#define URB_ERROR         'E'

/*
 * USB setup header as defined in USB specification.
 * Appears at the front of each Control S-type packet in DLT_USB captures.
 */
typedef struct _usb_setup {
	nd_uint8_t bmRequestType;
	nd_uint8_t bRequest;
	nd_uint16_t wValue;
	nd_uint16_t wIndex;
	nd_uint16_t wLength;
} pcap_usb_setup;

/*
 * Information from the URB for Isochronous transfers.
 */
typedef struct _iso_rec {
	nd_int32_t	error_count;
	nd_int32_t	numdesc;
} iso_rec;

/*
 * Header prepended by linux kernel to each event.
 * Appears at the front of each packet in DLT_USB_LINUX captures.
 */
typedef struct _usb_header {
	nd_uint64_t id;
	nd_uint8_t event_type;
	nd_uint8_t transfer_type;
	nd_uint8_t endpoint_number;
	nd_uint8_t device_address;
	nd_uint16_t bus_id;
	nd_uint8_t setup_flag;/*if !=0 the urb setup header is not present*/
	nd_uint8_t data_flag; /*if !=0 no urb data is present*/
	nd_int64_t ts_sec;
	nd_int32_t ts_usec;
	nd_int32_t status;
	nd_uint32_t urb_len;
	nd_uint32_t data_len; /* amount of urb data really present in this event*/
	pcap_usb_setup setup;
} pcap_usb_header;

/*
 * Header prepended by linux kernel to each event for the 2.6.31
 * and later kernels; for the 2.6.21 through 2.6.30 kernels, the
 * "iso_rec" information, and the fields starting with "interval"
 * are zeroed-out padding fields.
 *
 * Appears at the front of each packet in DLT_USB_LINUX_MMAPPED captures.
 */
typedef struct _usb_header_mmapped {
	nd_uint64_t id;
	nd_uint8_t event_type;
	nd_uint8_t transfer_type;
	nd_uint8_t endpoint_number;
	nd_uint8_t device_address;
	nd_uint16_t bus_id;
	nd_uint8_t setup_flag;/*if !=0 the urb setup header is not present*/
	nd_uint8_t data_flag; /*if !=0 no urb data is present*/
	nd_int64_t ts_sec;
	nd_int32_t ts_usec;
	nd_int32_t status;
	nd_uint32_t urb_len;
	nd_uint32_t data_len; /* amount of urb data really present in this event*/
	union {
		pcap_usb_setup setup;
		iso_rec iso;
	} s;
	nd_int32_t interval;	/* for Interrupt and Isochronous events */
	nd_int32_t start_frame;	/* for Isochronous events */
	nd_uint32_t xfer_flags;	/* copy of URB's transfer flags */
	nd_uint32_t ndesc;	/* number of isochronous descriptors */
} pcap_usb_header_mmapped;

/*
 * Isochronous descriptors; for isochronous transfers there might be
 * one or more of these at the beginning of the packet data.  The
 * number of descriptors is given by the "ndesc" field in the header;
 * as indicated, in older kernels that don't put the descriptors at
 * the beginning of the packet, that field is zeroed out, so that field
 * can be trusted even in captures from older kernels.
 */
typedef struct _usb_isodesc {
	nd_int32_t	status;
	nd_uint32_t	offset;
	nd_uint32_t	len;
	nd_byte		pad[4];
} usb_isodesc;


/* returns direction: 1=inbound 2=outbound -1=invalid */
static int
get_direction(int transfer_type, int event_type)
{
	int direction;

	direction = -1;
	switch(transfer_type){
	case URB_BULK:
	case URB_CONTROL:
	case URB_ISOCHRONOUS:
		switch(event_type) {
		case URB_SUBMIT:
			direction = 2;
			break;
		case URB_COMPLETE:
		case URB_ERROR:
			direction = 1;
			break;
		default:
			direction = -1;
		}
		break;
	case URB_INTERRUPT:
		switch(event_type) {
		case URB_SUBMIT:
			direction = 1;
			break;
		case URB_COMPLETE:
		case URB_ERROR:
			direction = 2;
			break;
		default:
			direction = -1;
		}
		break;
	 default:
		direction = -1;
	}

	return direction;
}

static void
usb_header_print(netdissect_options *ndo, const pcap_usb_header *uh)
{
	int direction;
	uint8_t transfer_type, event_type;

	ndo->ndo_protocol = "usb";

	nd_print_protocol_caps(ndo);
	if (ndo->ndo_qflag)
		return;

	ND_PRINT(" ");
	transfer_type = GET_U_1(uh->transfer_type);
	switch(transfer_type) {
		case URB_ISOCHRONOUS:
			ND_PRINT("ISOCHRONOUS");
			break;
		case URB_INTERRUPT:
			ND_PRINT("INTERRUPT");
			break;
		case URB_CONTROL:
			ND_PRINT("CONTROL");
			break;
		case URB_BULK:
			ND_PRINT("BULK");
			break;
		default:
			ND_PRINT(" ?");
	}

	event_type = GET_U_1(uh->event_type);
	switch(event_type) {
		case URB_SUBMIT:
			ND_PRINT(" SUBMIT");
			break;
		case URB_COMPLETE:
			ND_PRINT(" COMPLETE");
			break;
		case URB_ERROR:
			ND_PRINT(" ERROR");
			break;
		default:
			ND_PRINT(" ?");
	}

	direction = get_direction(transfer_type, event_type);
	if(direction == 1)
		ND_PRINT(" from");
	else if(direction == 2)
		ND_PRINT(" to");
	ND_PRINT(" %u:%u:%u", GET_HE_U_2(uh->bus_id),
		 GET_U_1(uh->device_address),
		 GET_U_1(uh->endpoint_number) & 0x7f);
}

/*
 * This is the top level routine of the printer for captures with a
 * 48-byte header.
 *
 * 'p' points to the header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
usb_linux_48_byte_if_print(netdissect_options *ndo,
                           const struct pcap_pkthdr *h _U_, const u_char *p)
{
	ndo->ndo_protocol = "usb_linux_48_byte";
	ND_TCHECK_LEN(p, sizeof(pcap_usb_header));
	ndo->ndo_ll_hdr_len += sizeof (pcap_usb_header);

	usb_header_print(ndo, (const pcap_usb_header *) p);
}

#ifdef DLT_USB_LINUX_MMAPPED
/*
 * This is the top level routine of the printer for captures with a
 * 64-byte header.
 *
 * 'p' points to the header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
usb_linux_64_byte_if_print(netdissect_options *ndo,
                           const struct pcap_pkthdr *h _U_, const u_char *p)
{
	ndo->ndo_protocol = "usb_linux_64_byte";
	ND_TCHECK_LEN(p, sizeof(pcap_usb_header_mmapped));
	ndo->ndo_ll_hdr_len += sizeof (pcap_usb_header_mmapped);

	usb_header_print(ndo, (const pcap_usb_header *) p);
}
#endif /* DLT_USB_LINUX_MMAPPED */

#endif /* DLT_USB_LINUX */

