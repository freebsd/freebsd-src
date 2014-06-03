/*-
 * Copyright (c) 2014 Jakub Wojciech Klama <jceel@FreeBSD.org>
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#define	DEBUG
#ifdef DEBUG
#define	debugf(fmt, args...)	printf("evdev: " fmt "\n", ##args)
#else
#define	debugf(fmt, args...)
#endif

MALLOC_DEFINE(M_EVDEV, "evdev", "evdev memory");

static inline void changebit(uint32_t *array, int, int);
static struct evdev_client *evdev_client_alloc(void);
static void evdev_client_push(struct evdev_client *, uint16_t, uint16_t,
    int32_t);

static inline void
changebit(uint32_t *array, int bit, int value)
{
	if (value)
		setbit(array, bit);
	else
		clrbit(array, bit);
}

struct evdev_dev *
evdev_alloc(void)
{
	return malloc(sizeof(struct evdev_dev), M_EVDEV, M_WAITOK | M_ZERO);
}

int
evdev_register(device_t dev, struct evdev_dev *evdev)
{
	int ret;

	device_printf(dev, "registered evdev provider: %s <%s>\n",
	    evdev->ev_name, evdev->ev_serial);

	/* Initialize internal structures */
	evdev->ev_dev = dev;
	strlcpy(evdev->ev_shortname, device_get_nameunit(dev), NAMELEN);
	LIST_INIT(&evdev->ev_clients);

	/* Create char device node */
	ret = evdev_cdev_create(evdev);
	if (ret != 0)
		return (ret);

	return (0);
}

int
evdev_unregister(device_t dev, struct evdev_dev *evdev)
{
	int ret;
	device_printf(dev, "unregistered evdev provider: %s\n", evdev->ev_name);

	ret = evdev_cdev_destroy(evdev);
	if (ret != 0)
		return (ret);

	return (0);
}

inline void
evdev_set_name(struct evdev_dev *evdev, const char *name)
{
	snprintf(evdev->ev_name, NAMELEN, "%s", name);
}

inline void
evdev_set_serial(struct evdev_dev *evdev, const char *serial)
{
	snprintf(evdev->ev_serial, NAMELEN, "%s", serial);
}

inline void
evdev_set_methods(struct evdev_dev *evdev, struct evdev_methods *methods)
{
	evdev->ev_methods = methods;
}

inline void
evdev_set_softc(struct evdev_dev *evdev, void *softc)
{
	evdev->ev_softc = softc;
}

inline void
evdev_support_event(struct evdev_dev *evdev, uint16_t type)
{

	setbit(&evdev->ev_type_flags, type);
}

inline void
evdev_support_key(struct evdev_dev *evdev, uint16_t code)
{

	setbit(&evdev->ev_key_flags, code);
}

inline void
evdev_support_rel(struct evdev_dev *evdev, uint16_t code)
{

	setbit(&evdev->ev_rel_flags, code);
}

inline void
evdev_support_abs(struct evdev_dev *evdev, uint16_t code)
{

	setbit(&evdev->ev_abs_flags, code);
}


inline void
evdev_support_msc(struct evdev_dev *evdev, uint16_t code)
{

	setbit(&evdev->ev_msc_flags, code);
}


inline void
evdev_support_led(struct evdev_dev *evdev, uint16_t code)
{

	setbit(&evdev->ev_led_flags, code);
}

inline void
evdev_support_snd(struct evdev_dev *evdev, uint16_t code)
{

	setbit(&evdev->ev_snd_flags, code);
}

inline void
evdev_support_sw(struct evdev_dev *evdev, uint16_t code)
{

	setbit(&evdev->ev_sw_flags, code);
}

inline void
evdev_set_absinfo(struct evdev_dev *evdev, uint16_t axis,
    struct input_absinfo *absinfo)
{

	memcpy(&evdev->ev_absinfo[axis], absinfo, sizeof(struct input_absinfo));
}

int
evdev_push_event(struct evdev_dev *evdev, uint16_t type, uint16_t code,
    int32_t value)
{
	struct evdev_client *client;

	debugf("%s pushed event %d/%d/%d",
	    device_get_nameunit(evdev->ev_dev), type, code, value);

	/* For certain event types, update device state bits */
	if (type == EV_KEY)
		changebit(evdev->ev_key_states, code, value);

	if (type == EV_LED)
		changebit(evdev->ev_led_states, code, value);

	if (type == EV_SND)
		changebit(evdev->ev_snd_states, code, value);

	if (type == EV_SW)
		changebit(evdev->ev_sw_states, code, value);

	/* For EV_ABS, save last value in absinfo */
	if (type == EV_ABS)
		evdev->ev_absinfo[code].value = value;

	/* Propagate event through all clients */
	LIST_FOREACH(client, &evdev->ev_clients, ec_link) {
		evdev_client_push(client, type, code, value);

		if (client->ec_ev_notify != NULL)
			client->ec_ev_notify(client, client->ec_ev_arg);
	}

	return (0);
}

int
evdev_sync(struct evdev_dev *evdev)
{
	
	return (evdev_push_event(evdev, EV_SYN, SYN_REPORT, 1));
}

int
evdev_register_client(struct evdev_dev *evdev, struct evdev_client **clientp)
{
	struct evdev_client *client;

	/* Initialize client structure */
	client = malloc(sizeof(struct evdev_client), M_EVDEV, M_WAITOK | M_ZERO);
	mtx_init(&client->ec_buffer_mtx, "evmtx", "evdev", MTX_DEF);
	client->ec_evdev = evdev;

	/* Initialize ring buffer */
	client->ec_buffer = malloc(sizeof(struct input_event) * 10, M_EVDEV, M_WAITOK | M_ZERO);
	client->ec_buffer_size = 10;
	client->ec_buffer_head = 0;
	client->ec_buffer_tail = 0;

	debugf("adding new client for device %s", evdev->ev_shortname);

	if (evdev->ev_clients_count == 0 &&
	    evdev->ev_methods->ev_open != NULL) {
		debugf("calling ev_open() on device %s", evdev->ev_shortname);
		evdev->ev_methods->ev_open(evdev, evdev->ev_softc);
	}

	LIST_INSERT_HEAD(&evdev->ev_clients, client, ec_link);
	evdev->ev_clients_count++;
	*clientp = client;
	return (0);
}

int
evdev_dispose_client(struct evdev_client *client)
{
	struct evdev_dev *evdev = client->ec_evdev;

	debugf("removing client for device %s", evdev->ev_shortname);

	evdev->ev_clients_count--;

	if (evdev->ev_clients_count == 0 &&
	    evdev->ev_methods->ev_close != NULL)
		evdev->ev_methods->ev_close(evdev, evdev->ev_softc);

	LIST_REMOVE(client, ec_link);
	free(client->ec_buffer, M_EVDEV);
	free(client, M_EVDEV);
	return (0);
}

static void
evdev_client_push(struct evdev_client *client, uint16_t type, uint16_t code,
    int32_t value)
{
	int count, head, tail;
	mtx_lock(&client->ec_buffer_mtx);
	head = client->ec_buffer_head;
	tail = client->ec_buffer_tail;
	count = client->ec_buffer_size;
	
	/* If queue is full, overwrite last element with SYN_DROPPED event */
	if ((tail + 1) % count == head) {
		debugf("client %p for device %s: buffer overflow", client,
		    client->ec_evdev->ev_shortname);

		microtime(&client->ec_buffer[tail].time);
		client->ec_buffer[tail].type = EV_SYN;
		client->ec_buffer[tail].code = SYN_DROPPED;

		wakeup(client);
		mtx_unlock(&client->ec_buffer_mtx);
		return;
	}

	microtime(&client->ec_buffer[tail].time);
	client->ec_buffer[tail].type = type;
	client->ec_buffer[tail].code = code;
	client->ec_buffer[tail].value = value;
	client->ec_buffer_tail = (tail + 1) % count;

	wakeup(client);
	mtx_unlock(&client->ec_buffer_mtx);
}

