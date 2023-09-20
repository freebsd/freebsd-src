/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023 Val Packett <val@packett.cool>
 * Copyright (c) 2023 Vladimir Kondratyev <wulf@FreeBSD.org>
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

#include "opt_hid.h"
#include "opt_spi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/crc16.h>
#include <sys/endian.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>

#include <dev/backlight/backlight.h>

#include <dev/evdev/input.h>

#define	HID_DEBUG_VAR atopcase_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidquirk.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#include "spibus_if.h"

#include "atopcase_reg.h"
#include "atopcase_var.h"

#define	ATOPCASE_IN_KDB()	(SCHEDULER_STOPPED() || kdb_active)
#define	ATOPCASE_IN_POLLING_MODE(sc)					\
	(((sc)->sc_gpe_bit == 0 && ((sc)->sc_irq_ih == NULL)) || cold ||\
	ATOPCASE_IN_KDB())
#define	ATOPCASE_WAKEUP(sc, chan) do {					\
	if (!ATOPCASE_IN_POLLING_MODE(sc)) {				\
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "wakeup: %p\n", chan);	\
		wakeup(chan);						\
	}								\
} while (0)
#define	ATOPCASE_SPI_PAUSE()	DELAY(100)
#define	ATOPCASE_SPI_NO_SLEEP_FLAG(sc)					\
	((sc)->sc_irq_ih != NULL ? SPI_FLAG_NO_SLEEP : 0)

/* Tunables */
static SYSCTL_NODE(_hw_hid, OID_AUTO, atopcase, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Apple MacBook Topcase HID driver");

#ifdef HID_DEBUG
enum atopcase_log_level atopcase_debug = ATOPCASE_LLEVEL_DISABLED;

SYSCTL_INT(_hw_hid_atopcase, OID_AUTO, debug, CTLFLAG_RWTUN,
    &atopcase_debug, ATOPCASE_LLEVEL_DISABLED, "atopcase log level");
#endif /* !HID_DEBUG */

static const uint8_t booted[] = { 0xa0, 0x80, 0x00, 0x00 };
static const uint8_t status_ok[] = { 0xac, 0x27, 0x68, 0xd5 };

static inline struct atopcase_child *
atopcase_get_child_by_device(struct atopcase_softc *sc, uint8_t device)
{
	switch (device) {
	case ATOPCASE_DEV_KBRD:
		return (&sc->sc_kb);
	case ATOPCASE_DEV_TPAD:
		return (&sc->sc_tp);
	default:
		return (NULL);
	}
}

static int
atopcase_receive_status(struct atopcase_softc *sc)
{
	struct spi_command cmd = SPI_COMMAND_INITIALIZER;
	uint8_t dummy_buffer[4] = { 0 };
	uint8_t status_buffer[4] = { 0 };
	int err;

	cmd.tx_cmd = dummy_buffer;
	cmd.tx_cmd_sz = sizeof(dummy_buffer);
	cmd.rx_cmd = status_buffer;
	cmd.rx_cmd_sz = sizeof(status_buffer);
	cmd.flags = ATOPCASE_SPI_NO_SLEEP_FLAG(sc);

	err = SPIBUS_TRANSFER(device_get_parent(sc->sc_dev), sc->sc_dev, &cmd);
	ATOPCASE_SPI_PAUSE();
	if (err) {
		device_printf(sc->sc_dev, "SPI error: %d\n", err);
		return (err);
	}

	DPRINTFN(ATOPCASE_LLEVEL_TRACE, "Status: %*D\n", 4, status_buffer, " ");

	if (memcmp(status_buffer, status_ok, sizeof(status_ok)) == 0) {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "Wrote command\n");
		ATOPCASE_WAKEUP(sc, sc->sc_dev);
	} else {
		device_printf(sc->sc_dev, "Failed to write command\n");
		return (EIO);
	}

	return (0);
}

static int
atopcase_process_message(struct atopcase_softc *sc, uint8_t device, void *msg,
    uint16_t msg_len)
{
	struct atopcase_header *hdr = msg;
	struct atopcase_child *ac;
	void *payload;
	uint16_t pl_len, crc;

	payload = (uint8_t *)msg + sizeof(*hdr);
	pl_len = le16toh(hdr->len);

	if (pl_len + sizeof(*hdr) + sizeof(crc) != msg_len) {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG,
		    "message with length overflow\n");
		return (EIO);
	}

	crc = le16toh(*(uint16_t *)((uint8_t *)payload + pl_len));
	if (crc != crc16(0, msg, msg_len - sizeof(crc))) {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG,
		    "message with failed checksum\n");
		return (EIO);
	}

#define CPOFF(dst, len, off)	do {					\
	unsigned _len = le16toh(len);					\
	unsigned _off = le16toh(off);					\
	if (pl_len >= _len + _off) {					\
		memcpy(dst, (uint8_t*)payload + _off, MIN(_len, sizeof(dst)));\
		(dst)[MIN(_len, sizeof(dst) - 1)] = '\0';		\
	}} while (0);

	if ((ac = atopcase_get_child_by_device(sc, device)) != NULL
	    && hdr->type == ATOPCASE_MSG_TYPE_REPORT(device)) {
		if (ac->open)
			ac->intr_handler(ac->intr_ctx, payload, pl_len);
	} else if (device == ATOPCASE_DEV_INFO
	    && hdr->type == ATOPCASE_MSG_TYPE_INFO(ATOPCASE_INFO_IFACE)
	    && (ac = atopcase_get_child_by_device(sc, hdr->type_arg)) != NULL) {
		struct atopcase_iface_info_payload *iface = payload;
		CPOFF(ac->name, iface->name_len, iface->name_off);
		DPRINTF("Interface #%d name: %s\n", ac->device, ac->name);
	} else if (device == ATOPCASE_DEV_INFO
	    && hdr->type == ATOPCASE_MSG_TYPE_INFO(ATOPCASE_INFO_DESCRIPTOR)
	    && (ac = atopcase_get_child_by_device(sc, hdr->type_arg)) != NULL) {
		memcpy(ac->rdesc, payload, pl_len);
		ac->rdesc_len = ac->hw.rdescsize = pl_len;
		DPRINTF("%s HID report descriptor: %*D\n", ac->name,
		    (int) ac->hw.rdescsize, ac->rdesc, " ");
	} else if (device == ATOPCASE_DEV_INFO
	    && hdr->type == ATOPCASE_MSG_TYPE_INFO(ATOPCASE_INFO_DEVICE)
	    && hdr->type_arg == ATOPCASE_INFO_DEVICE) {
		struct atopcase_device_info_payload *dev = payload;
		sc->sc_vid = le16toh(dev->vid);
		sc->sc_pid = le16toh(dev->pid);
		sc->sc_ver = le16toh(dev->ver);
		CPOFF(sc->sc_vendor, dev->vendor_len, dev->vendor_off);
		CPOFF(sc->sc_product, dev->product_len, dev->product_off);
		CPOFF(sc->sc_serial, dev->serial_len, dev->serial_off);
		if (bootverbose) {
			device_printf(sc->sc_dev, "Device info descriptor:\n");
			printf("  Vendor:  %s\n", sc->sc_vendor);
			printf("  Product: %s\n", sc->sc_product);
			printf("  Serial:  %s\n", sc->sc_serial);
		}
	}

	return (0);
}

int
atopcase_receive_packet(struct atopcase_softc *sc)
{
	struct atopcase_packet pkt = { 0 };
	struct spi_command cmd = SPI_COMMAND_INITIALIZER;
	void *msg;
	int err;
	uint16_t length, remaining, offset, msg_len;

	bzero(&sc->sc_junk, sizeof(struct atopcase_packet));
	cmd.tx_cmd = &sc->sc_junk;
	cmd.tx_cmd_sz = sizeof(struct atopcase_packet);
	cmd.rx_cmd = &pkt;
	cmd.rx_cmd_sz = sizeof(struct atopcase_packet);
	cmd.flags = ATOPCASE_SPI_NO_SLEEP_FLAG(sc);
	err = SPIBUS_TRANSFER(device_get_parent(sc->sc_dev), sc->sc_dev, &cmd);
	ATOPCASE_SPI_PAUSE();
	if (err) {
		device_printf(sc->sc_dev, "SPI error: %d\n", err);
		return (err);
	}

	DPRINTFN(ATOPCASE_LLEVEL_TRACE, "Response: %*D\n", 256, &pkt, " ");

	if (le16toh(pkt.checksum) != crc16(0, &pkt, sizeof(pkt) - 2)) {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "packet with failed checksum\n");
		return (EIO);
	}

	/*
	 * When we poll and nothing has arrived we get a particular packet
	 * starting with '80 11 00 01'
	 */
	if (pkt.direction == ATOPCASE_DIR_NOTHING) {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "'Nothing' packet: %*D\n", 4,
		    &pkt, " ");
		return (EAGAIN);
	}

	if (pkt.direction != ATOPCASE_DIR_READ &&
	    pkt.direction != ATOPCASE_DIR_WRITE) {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG,
		         "unknown message direction 0x%x\n", pkt.direction);
		return (EIO);
	}

	length = le16toh(pkt.length);
	remaining = le16toh(pkt.remaining);
	offset = le16toh(pkt.offset);

	if (length > sizeof(pkt.data)) {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG,
		    "packet with length overflow: %u\n", length);
		return (EIO);
	}

	if (pkt.direction == ATOPCASE_DIR_READ &&
	    pkt.device == ATOPCASE_DEV_INFO &&
	    length == sizeof(booted) &&
	    memcmp(pkt.data, booted, length) == 0) {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "GPE boot packet\n");
		sc->sc_booted = true;
		ATOPCASE_WAKEUP(sc, sc);
		return (0);
	}

	/* handle multi-packet messages */
	if (remaining != 0 || offset != 0) {
		if (offset != sc->sc_msg_len) {
			DPRINTFN(ATOPCASE_LLEVEL_DEBUG,
			    "Unexpected offset (got %u, expected %u)\n",
			    offset, sc->sc_msg_len);
			sc->sc_msg_len = 0;
			return (EIO);
		}

		if ((size_t)remaining + length + offset > sizeof(sc->sc_msg)) {
			DPRINTFN(ATOPCASE_LLEVEL_DEBUG,
			    "Message with length overflow: %zu\n",
			    (size_t)remaining + length + offset);
			sc->sc_msg_len = 0;
			return (EIO);
		}

		memcpy(sc->sc_msg + offset, &pkt.data, length);
		sc->sc_msg_len += length;

		if (remaining != 0)
			return (0);

		msg = sc->sc_msg;
		msg_len = sc->sc_msg_len;
	} else {
		msg = pkt.data;
		msg_len = length;
	}
	sc->sc_msg_len = 0;

	err = atopcase_process_message(sc, pkt.device, msg, msg_len);
	if (err == 0 && pkt.direction == ATOPCASE_DIR_WRITE) {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "Write ack\n");
		ATOPCASE_WAKEUP(sc, sc);
	}

	return (err);
}

static int
atopcase_send(struct atopcase_softc *sc, struct atopcase_packet *pkt)
{
	struct spi_command cmd = SPI_COMMAND_INITIALIZER;
	int err, retries;

	cmd.tx_cmd = pkt;
	cmd.tx_cmd_sz = sizeof(struct atopcase_packet);
	cmd.rx_cmd = &sc->sc_junk;
	cmd.rx_cmd_sz = sizeof(struct atopcase_packet);
	cmd.flags = SPI_FLAG_KEEP_CS | ATOPCASE_SPI_NO_SLEEP_FLAG(sc);

	DPRINTFN(ATOPCASE_LLEVEL_TRACE, "Request: %*D\n",
	    (int)sizeof(struct atopcase_packet), cmd.tx_cmd, " ");

	if (!ATOPCASE_IN_POLLING_MODE(sc)) {
		if (sc->sc_irq_ih != NULL)
			mtx_lock(&sc->sc_mtx);
		else
			sx_xlock(&sc->sc_sx);
	}
	sc->sc_wait_for_status = true;
	err = SPIBUS_TRANSFER(device_get_parent(sc->sc_dev), sc->sc_dev, &cmd);
	ATOPCASE_SPI_PAUSE();
	if (!ATOPCASE_IN_POLLING_MODE(sc)) {
		if (sc->sc_irq_ih != NULL)
			mtx_unlock(&sc->sc_mtx);
		else
			sx_xunlock(&sc->sc_sx);
	}
	if (err != 0) {
		device_printf(sc->sc_dev, "SPI error: %d\n", err);
		goto exit;
	}

	if (ATOPCASE_IN_POLLING_MODE(sc)) {
		err = atopcase_receive_status(sc);
	} else {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "wait for: %p\n", sc->sc_dev);
		err = tsleep(sc->sc_dev, 0, "atcstat", hz / 10);
	}
	sc->sc_wait_for_status = false;
	if (err != 0) {
		DPRINTF("Write status read failed: %d\n", err);
		goto exit;
	}

	if (ATOPCASE_IN_POLLING_MODE(sc)) {
		/* Backlight setting may require a lot of time */
		retries = 20;
		while ((err = atopcase_receive_packet(sc)) == EAGAIN &&
		    --retries != 0)
			DELAY(1000);
	} else {
		DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "wait for: %p\n", sc);
		err = tsleep(sc, 0, "atcack", hz / 10);
	}
	if (err != 0)
		DPRINTF("Write ack read failed: %d\n", err);

exit:
	if (err == EWOULDBLOCK)
		err = EIO;

	return (err);
}

static void
atopcase_create_message(struct atopcase_packet *pkt, uint8_t device,
    uint16_t type, uint8_t type_arg, const void *payload, uint8_t len,
    uint16_t resp_len)
{
	struct atopcase_header *hdr = (struct atopcase_header *)pkt->data;
	uint16_t msg_checksum;
	static uint8_t seq_no;

	KASSERT(len <= ATOPCASE_DATA_SIZE - sizeof(struct atopcase_header),
	    ("outgoing msg must be 1 packet"));

	bzero(pkt, sizeof(struct atopcase_packet));
	pkt->direction = ATOPCASE_DIR_WRITE;
	pkt->device = device;
	pkt->length = htole16(sizeof(*hdr) + len + 2);

	hdr->type = htole16(type);
	hdr->type_arg = type_arg;
	hdr->seq_no = seq_no++;
	hdr->resp_len = htole16((resp_len == 0) ? len : resp_len);
	hdr->len = htole16(len);

	memcpy(pkt->data + sizeof(*hdr), payload, len);
	msg_checksum = htole16(crc16(0, pkt->data, pkt->length - 2));
	memcpy(pkt->data + sizeof(*hdr) + len, &msg_checksum, 2);
	pkt->checksum = htole16(crc16(0, (uint8_t*)pkt, sizeof(*pkt) - 2));

	return;
}

static int
atopcase_request_desc(struct atopcase_softc *sc, uint16_t type, uint8_t device)
{
	atopcase_create_message(
	   &sc->sc_buf, ATOPCASE_DEV_INFO, type, device, NULL, 0, 0x200);
	return (atopcase_send(sc, &sc->sc_buf));
}

int
atopcase_intr(struct atopcase_softc *sc)
{
	int err;

	DPRINTFN(ATOPCASE_LLEVEL_DEBUG, "Interrupt event\n");

	if (sc->sc_wait_for_status) {
		err = atopcase_receive_status(sc);
		sc->sc_wait_for_status = false;
	} else
		err = atopcase_receive_packet(sc);

	return (err);
}

static int
atopcase_add_child(struct atopcase_softc *sc, struct atopcase_child *ac,
    uint8_t device)
{
	device_t hidbus;
	int err = 0;

	ac->device = device;

	/* fill device info */
	strlcpy(ac->hw.name, "Apple MacBook", sizeof(ac->hw.name));
	ac->hw.idBus = BUS_SPI;
	ac->hw.idVendor = sc->sc_vid;
	ac->hw.idProduct = sc->sc_pid;
	ac->hw.idVersion = sc->sc_ver;
	strlcpy(ac->hw.idPnP, sc->sc_hid, sizeof(ac->hw.idPnP));
	strlcpy(ac->hw.serial, sc->sc_serial, sizeof(ac->hw.serial));
	/*
	 * HID write and set_report methods executed on Apple SPI topcase
	 * hardware do the same request on SPI layer. Set HQ_NOWRITE quirk to
	 * force hidmap to convert writes to set_reports. That makes HID bus
	 * write handler unnecessary and reduces code duplication.
	 */
	hid_add_dynamic_quirk(&ac->hw, HQ_NOWRITE);

	DPRINTF("Get the interface #%d descriptor\n", device);
	err = atopcase_request_desc(sc,
	    ATOPCASE_MSG_TYPE_INFO(ATOPCASE_INFO_IFACE), device);
	if (err) {
		device_printf(sc->sc_dev, "can't receive iface descriptor\n");
		goto exit;
	}

	DPRINTF("Get the \"%s\" HID report descriptor\n", ac->name);
	err = atopcase_request_desc(sc,
	    ATOPCASE_MSG_TYPE_INFO(ATOPCASE_INFO_DESCRIPTOR), device);
	if (err) {
		device_printf(sc->sc_dev, "can't receive report descriptor\n");
		goto exit;
	}

	hidbus = device_add_child(sc->sc_dev, "hidbus", -1);
	if (hidbus == NULL) {
		device_printf(sc->sc_dev, "can't add child\n");
		err = ENOMEM;
		goto exit;
	}
	device_set_ivars(hidbus, &ac->hw);
	ac->hidbus = hidbus;

exit:
	return (err);
}

int
atopcase_init(struct atopcase_softc *sc)
{
	int err;

	/* Wait until we know we're getting reasonable responses */
	if(!sc->sc_booted && tsleep(sc, 0, "atcboot", hz / 20) != 0) {
		device_printf(sc->sc_dev, "can't establish communication\n");
		err = EIO;
		goto err;
	}

	/*
	 * Management device may send a message on first boot after power off.
	 * Let interrupt handler to read and discard it.
	 */
	DELAY(2000);

	DPRINTF("Get the device descriptor\n");
	err = atopcase_request_desc(sc,
	    ATOPCASE_MSG_TYPE_INFO(ATOPCASE_INFO_DEVICE),
	    ATOPCASE_INFO_DEVICE);
	if (err) {
		device_printf(sc->sc_dev, "can't receive device descriptor\n");
		goto err;
	}

	err = atopcase_add_child(sc, &sc->sc_kb, ATOPCASE_DEV_KBRD);
	if (err != 0)
		goto err;
	err = atopcase_add_child(sc, &sc->sc_tp, ATOPCASE_DEV_TPAD);
	if (err != 0)
		goto err;

	/* TODO: skip on 2015 models where it's controlled by asmc */
	sc->sc_backlight = backlight_register("atopcase", sc->sc_dev);
	if (!sc->sc_backlight) {
		device_printf(sc->sc_dev, "can't register backlight\n");
		err = ENOMEM;
	}

	if (sc->sc_tq != NULL)
		taskqueue_enqueue_timeout(sc->sc_tq, &sc->sc_task, hz / 120);

	return (bus_generic_attach(sc->sc_dev));

err:
	return (err);
}

int
atopcase_destroy(struct atopcase_softc *sc)
{
	int err;

	err = device_delete_children(sc->sc_dev);
	if (err)
		return (err);

	if (sc->sc_backlight)
		backlight_destroy(sc->sc_backlight);

	return (0);
}

static struct atopcase_child *
atopcase_get_child_by_hidbus(device_t child)
{
	device_t parent = device_get_parent(child);
	struct atopcase_softc *sc = device_get_softc(parent);

	if (child == sc->sc_kb.hidbus)
		return (&sc->sc_kb);
	if (child == sc->sc_tp.hidbus)
		return (&sc->sc_tp);
	panic("unknown child");
}

void
atopcase_intr_setup(device_t dev, device_t child, hid_intr_t intr,
    void *context, struct hid_rdesc_info *rdesc)
{
	struct atopcase_child *ac = atopcase_get_child_by_hidbus(child);

	if (intr == NULL)
		return;

	rdesc->rdsize = ATOPCASE_MSG_SIZE - sizeof(struct atopcase_header) - 2;
	rdesc->grsize = 0;
	rdesc->srsize = ATOPCASE_DATA_SIZE - sizeof(struct atopcase_header) - 2;
	rdesc->wrsize = 0;

	ac->intr_handler = intr;
	ac->intr_ctx = context;
}

void
atopcase_intr_unsetup(device_t dev, device_t child)
{
}

int
atopcase_intr_start(device_t dev, device_t child)
{
	struct atopcase_softc *sc = device_get_softc(dev);
	struct atopcase_child *ac = atopcase_get_child_by_hidbus(child);

	if (ATOPCASE_IN_POLLING_MODE(sc))
		sx_xlock(&sc->sc_write_sx);
	else if (sc->sc_irq_ih != NULL)
		mtx_lock(&sc->sc_mtx);
	else
		sx_xlock(&sc->sc_sx);
	ac->open = true;
	if (ATOPCASE_IN_POLLING_MODE(sc))
		sx_xunlock(&sc->sc_write_sx);
	else if (sc->sc_irq_ih != NULL)
		mtx_unlock(&sc->sc_mtx);
	else
		sx_xunlock(&sc->sc_sx);

	return (0);
}

int
atopcase_intr_stop(device_t dev, device_t child)
{
	struct atopcase_softc *sc = device_get_softc(dev);
	struct atopcase_child *ac = atopcase_get_child_by_hidbus(child);

	if (ATOPCASE_IN_POLLING_MODE(sc))
		sx_xlock(&sc->sc_write_sx);
	else if (sc->sc_irq_ih != NULL)
		mtx_lock(&sc->sc_mtx);
	else
		sx_xlock(&sc->sc_sx);
	ac->open = false;
	if (ATOPCASE_IN_POLLING_MODE(sc))
		sx_xunlock(&sc->sc_write_sx);
	else if (sc->sc_irq_ih != NULL)
		mtx_unlock(&sc->sc_mtx);
	else
		sx_xunlock(&sc->sc_sx);

	return (0);
}

void
atopcase_intr_poll(device_t dev, device_t child)
{
	struct atopcase_softc *sc = device_get_softc(dev);

	(void)atopcase_receive_packet(sc);
}

int
atopcase_get_rdesc(device_t dev, device_t child, void *buf, hid_size_t len)
{
	struct atopcase_child *ac = atopcase_get_child_by_hidbus(child);

	if (ac->rdesc_len != len)
		return (ENXIO);
	memcpy(buf, ac->rdesc, len);

	return (0);
}

int
atopcase_set_report(device_t dev, device_t child, const void *buf,
    hid_size_t len, uint8_t type __unused, uint8_t id)
{
	struct atopcase_softc *sc = device_get_softc(dev);
	struct atopcase_child *ac = atopcase_get_child_by_hidbus(child);
	int err;

	if (len >= ATOPCASE_DATA_SIZE - sizeof(struct atopcase_header) - 2)
		return (EINVAL);

	DPRINTF("%s HID command SET_REPORT %d (len %d): %*D\n",
	    ac->name, id, len, len, buf, " ");

	if (!ATOPCASE_IN_KDB())
		sx_xlock(&sc->sc_write_sx);
	atopcase_create_message(&sc->sc_buf, ac->device,
	    ATOPCASE_MSG_TYPE_SET_REPORT(ac->device, id), 0, buf, len, 0);
	err = atopcase_send(sc, &sc->sc_buf);
	if (!ATOPCASE_IN_KDB())
		sx_xunlock(&sc->sc_write_sx);

	return (err);
}

int
atopcase_backlight_update_status(device_t dev, struct backlight_props *props)
{
	struct atopcase_softc *sc = device_get_softc(dev);
	struct atopcase_bl_payload payload = { 0 };

	payload.report_id = ATOPCASE_BKL_REPORT_ID;
	payload.device = ATOPCASE_DEV_KBRD;
	/*
	 * Hardware range is 32-255 for visible backlight,
	 * convert from percentages
	 */
	payload.level = (props->brightness == 0) ? 0 :
		(32 + (223 * props->brightness / 100));
	payload.status = (payload.level > 0) ? 0x01F4 : 0x1;

	return (atopcase_set_report(dev, sc->sc_kb.hidbus, &payload,
	    sizeof(payload), HID_OUTPUT_REPORT, ATOPCASE_BKL_REPORT_ID));
}

int
atopcase_backlight_get_status(device_t dev, struct backlight_props *props)
{
	struct atopcase_softc *sc = device_get_softc(dev);

	props->brightness = sc->sc_backlight_level;
	props->nlevels = 0;

	return (0);
}

int
atopcase_backlight_get_info(device_t dev, struct backlight_info *info)
{
	info->type = BACKLIGHT_TYPE_KEYBOARD;
	strlcpy(info->name, "Apple MacBook Keyboard", BACKLIGHTMAXNAMELENGTH);

	return (0);
}
