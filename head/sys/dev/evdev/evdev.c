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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#ifdef DEBUG
#define	debugf(fmt, args...)	printf("evdev: " fmt "\n", ##args)
#else
#define	debugf(fmt, args...)
#endif

#define	DEF_RING_SIZE	16

MALLOC_DEFINE(M_EVDEV, "evdev", "evdev memory");

static inline void set_bit(unsigned long *, int);
static inline void clr_bit(unsigned long *, int);
static inline void change_bit(unsigned long *, int, int);
static void evdev_assign_id(struct evdev_dev *);
#if 0
static void evdev_start_repeat(struct evdev_dev *, int32_t);
static void evdev_stop_repeat(struct evdev_dev *);
#endif
static int evdev_check_event(struct evdev_dev *, uint16_t, uint16_t, int32_t);
static void evdev_client_push(struct evdev_client *, uint16_t, uint16_t,
    int32_t);

static inline void
set_bit(unsigned long *array, int bit)
{
	array[bit / LONG_WIDTH] |= (1LL << (bit % LONG_WIDTH));
}

static inline void
clr_bit(unsigned long *array, int bit)
{
	array[bit / LONG_WIDTH] &= ~(1LL << (bit % LONG_WIDTH));
}

static inline void
change_bit(unsigned long *array, int bit, int value)
{
	if (value)
		set_bit(array, bit);
	else
		clr_bit(array, bit);
}

struct evdev_dev *
evdev_alloc(void)
{

	return malloc(sizeof(struct evdev_dev), M_EVDEV, M_WAITOK | M_ZERO);
}

void
evdev_free(struct evdev_dev *evdev)
{

	free(evdev, M_EVDEV);
}

int
evdev_register(device_t dev, struct evdev_dev *evdev)
{
	int ret;

	device_printf(dev, "registered evdev provider: %s <%s>\n",
	    evdev->ev_name, evdev->ev_serial);

	/* Initialize internal structures */
	evdev->ev_dev = dev;
	mtx_init(&evdev->ev_mtx, "evmtx", "evdev", MTX_DEF);
	LIST_INIT(&evdev->ev_clients);

	if (dev != NULL)
		strlcpy(evdev->ev_shortname, device_get_nameunit(dev), NAMELEN);
	
	if (evdev->ev_repeat_mode == EVDEV_REPEAT) {
		/* Initialize callout */
		callout_init(&evdev->ev_rep_callout, 1);

		if (evdev->ev_rep[REP_DELAY] == 0 &&
		    evdev->ev_rep[REP_PERIOD] == 0) {
			/* Supply default values */
			evdev->ev_rep[REP_DELAY] = 300;
			evdev->ev_rep[REP_PERIOD] = 50;
		}
	}

	/* Retrieve bus info */
	evdev_assign_id(evdev);

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
evdev_set_phys(struct evdev_dev *evdev, const char *name)
{

	snprintf(evdev->ev_shortname, NAMELEN, "%s", name);
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

inline int
evdev_support_event(struct evdev_dev *evdev, uint16_t type)
{

	if (type >= EV_CNT)
		return (EINVAL);

	set_bit(evdev->ev_type_flags, type);
	return (0);
}

inline int
evdev_support_key(struct evdev_dev *evdev, uint16_t code)
{

	if (code >= KEY_CNT)
		return (EINVAL);

	set_bit(evdev->ev_key_flags, code);
	return (0);
}

inline int
evdev_support_rel(struct evdev_dev *evdev, uint16_t code)
{

	if (code >= REL_CNT)
		return (EINVAL);

	set_bit(evdev->ev_rel_flags, code);
	return (0);
}

inline int
evdev_support_abs(struct evdev_dev *evdev, uint16_t code)
{

	if (code >= ABS_CNT)
		return (EINVAL);

	set_bit(evdev->ev_abs_flags, code);
	return (0);
}


inline int
evdev_support_msc(struct evdev_dev *evdev, uint16_t code)
{

	if (code >= MSC_CNT)
		return (EINVAL);

	set_bit(evdev->ev_msc_flags, code);
	return (0);
}


inline int
evdev_support_led(struct evdev_dev *evdev, uint16_t code)
{

	if (code >= LED_CNT)
		return (EINVAL);

	set_bit(evdev->ev_led_flags, code);
	return (0);
}

inline int
evdev_support_snd(struct evdev_dev *evdev, uint16_t code)
{

	if (code >= SND_CNT)
		return (EINVAL);

	set_bit(evdev->ev_snd_flags, code);
	return (0);
}

inline int
evdev_support_sw(struct evdev_dev *evdev, uint16_t code)
{
	if (code >= SW_CNT)
		return (EINVAL);

	set_bit(evdev->ev_sw_flags, code);
	return (0);
}

inline int
evdev_support_repeat(struct evdev_dev *evdev, enum evdev_repeat_mode mode)
{

	if (mode != NO_REPEAT)
		set_bit(evdev->ev_type_flags, EV_REP);

	evdev->ev_repeat_mode = mode;
	return (0);
}


inline void
evdev_set_absinfo(struct evdev_dev *evdev, uint16_t axis,
    struct input_absinfo *absinfo)
{

	memcpy(&evdev->ev_absinfo[axis], absinfo, sizeof(struct input_absinfo));
}

inline void
evdev_set_repeat_params(struct evdev_dev *evdev, uint16_t property, int value)
{

	KASSERT(property < REP_CNT, ("invalid evdev repeat property"));
	evdev->ev_rep[property] = value;
}

static int
evdev_check_event(struct evdev_dev *evdev, uint16_t type, uint16_t code,
    int32_t value)
{

	if (type == EV_KEY) {
		if (code >= KEY_CNT)
			return (EINVAL);
	} else if (type == EV_REL) {
		if (code >= REL_CNT)
			return (EINVAL);
	} else if (type == EV_ABS) {
		if (code >= ABS_CNT)
			return (EINVAL);
	} else if (type == EV_MSC) {
		if (code >= MSC_CNT)
			return (EINVAL);
	} else if (type == EV_LED) {
		if (code >= LED_CNT)
			return (EINVAL);
	} else if (type == EV_SND) {
		if (code >= SND_CNT)
			return (EINVAL);
	} else if (type == EV_SW) {
		if (code >= SW_CNT)
			return (EINVAL);
	} else
		return (EINVAL);

	return (0);
}

int
evdev_push_event(struct evdev_dev *evdev, uint16_t type, uint16_t code,
    int32_t value)
{
	struct evdev_client *client;

	if (evdev_check_event(evdev, type, code, value) != 0)
		return (EINVAL);

	debugf("%s pushed event %d/%d/%d",
	    evdev->ev_shortname, type, code, value);

	/* For certain event types, update device state bits */
	if (type == EV_KEY)
		change_bit(evdev->ev_key_states, code, value);

	if (type == EV_LED)
		change_bit(evdev->ev_led_states, code, value);

	if (type == EV_SND)
		change_bit(evdev->ev_snd_states, code, value);

	if (type == EV_SW)
		change_bit(evdev->ev_sw_states, code, value);

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
evdev_inject_event(struct evdev_dev *evdev, uint16_t type, uint16_t code,
    int32_t value)
{

	if (evdev->ev_methods->ev_event != NULL) {
		evdev->ev_methods->ev_event(evdev, evdev->ev_softc, type,
		    code, value);
	}

	return (0);
}

inline int
evdev_sync(struct evdev_dev *evdev)
{
	
	return (evdev_push_event(evdev, EV_SYN, SYN_REPORT, 1));
}


inline int
evdev_mt_sync(struct evdev_dev *evdev)
{
	
	return (evdev_push_event(evdev, EV_SYN, SYN_MT_REPORT, 1));
}

int
evdev_register_client(struct evdev_dev *evdev, struct evdev_client **clientp)
{
	struct evdev_client *client;

	/* Initialize client structure */
	client = malloc(sizeof(struct evdev_client), M_EVDEV, M_WAITOK | M_ZERO);
	mtx_init(&client->ec_buffer_mtx, "evclient", "evdev", MTX_DEF);
	client->ec_evdev = evdev;

	/* Initialize ring buffer */
	client->ec_buffer = malloc(sizeof(struct input_event) * DEF_RING_SIZE,
	    M_EVDEV, M_WAITOK | M_ZERO);
	client->ec_buffer_size = DEF_RING_SIZE;
	client->ec_buffer_head = 0;
	client->ec_buffer_tail = 0;
	client->ec_enabled = true;

	debugf("adding new client for device %s", evdev->ev_shortname);

	if (evdev->ev_clients_count == 0 && evdev->ev_methods != NULL &&
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

	if (evdev->ev_clients_count == 0 && evdev->ev_methods != NULL &&
	    evdev->ev_methods->ev_close != NULL)
		evdev->ev_methods->ev_close(evdev, evdev->ev_softc);

	LIST_REMOVE(client, ec_link);
	free(client->ec_buffer, M_EVDEV);
	free(client, M_EVDEV);
	return (0);
}

int
evdev_grab_client(struct evdev_client *client)
{
	struct evdev_dev *evdev = client->ec_evdev;
	struct evdev_client *iter;

	EVDEV_LOCK(evdev);
	if (evdev->ev_grabbed) {
		EVDEV_UNLOCK(evdev);
		return (EBUSY);
	}

	evdev->ev_grabbed = true;

	/* Disable all other clients */
	LIST_FOREACH(iter, &evdev->ev_clients, ec_link) {
		if (iter != client)
			iter->ec_enabled = false;
	}

	EVDEV_UNLOCK(evdev);
	return (0);
}

int
evdev_release_client(struct evdev_client *client)
{
	struct evdev_dev *evdev = client->ec_evdev;
	struct evdev_client *iter;

	EVDEV_LOCK(evdev);
	if (!evdev->ev_grabbed) {
		EVDEV_UNLOCK(evdev);
		return (EINVAL);
	}

	evdev->ev_grabbed = false;

	/* Enable all other clients */
	LIST_FOREACH(iter, &evdev->ev_clients, ec_link) {
		iter->ec_enabled = true;
	}

	EVDEV_UNLOCK(evdev);
	return (0);
}

static void
evdev_assign_id(struct evdev_dev *dev)
{
	device_t parent;
	devclass_t devclass;
	const char *classname;

	if (dev->ev_dev == NULL) {
		dev->ev_id.bustype = BUS_VIRTUAL;
		return;
	}

	parent = device_get_parent(dev->ev_dev);
	if (parent == NULL) {
		dev->ev_id.bustype = BUS_HOST;
		return;
	}

	devclass = device_get_devclass(parent);
	classname = devclass_get_name(devclass);

	debugf("parent bus classname: %s", classname);

	if (strcmp(classname, "pci") == 0) {
		dev->ev_id.bustype = BUS_PCI;
		dev->ev_id.vendor = pci_get_vendor(dev->ev_dev);
		dev->ev_id.product = pci_get_device(dev->ev_dev);
		dev->ev_id.version = pci_get_revid(dev->ev_dev);
		return;
	}

	if (strcmp(classname, "uhub") == 0) {
		struct usb_attach_arg *uaa = device_get_ivars(dev->ev_dev);
		dev->ev_id.bustype = BUS_USB;
		dev->ev_id.vendor = uaa->info.idVendor;
		dev->ev_id.product = uaa->info.idProduct;
		return;
	}

	dev->ev_id.bustype = BUS_HOST;
}

#if 0
static void
evdev_start_repeat(struct evdev_dev *dev, int32_t key)
{
	
}

static void
evdev_stop_repeat(struct evdev_dev *dev)
{

}
#endif

static void
evdev_client_push(struct evdev_client *client, uint16_t type, uint16_t code,
    int32_t value)
{
	int count, head, tail;
	
	EVDEV_CLIENT_LOCKQ(client);
	head = client->ec_buffer_head;
	tail = client->ec_buffer_tail;
	count = client->ec_buffer_size;

	if (!client->ec_enabled) {
		EVDEV_CLIENT_UNLOCKQ(client);
		return;
	}
	
	/* If queue is full, overwrite last element with SYN_DROPPED event */
	if ((tail + 1) % count == head) {
		debugf("client %p for device %s: buffer overflow", client,
		    client->ec_evdev->ev_shortname);

		/* Check whether we placed SYN_DROPPED packet already */
		if (client->ec_buffer[tail - 2 % count].type == EV_SYN &&
		    client->ec_buffer[tail - 2 % count].code == SYN_DROPPED) {
			wakeup(client);
			EVDEV_CLIENT_UNLOCKQ(client);
			return;
		}

		microtime(&client->ec_buffer[tail - 1].time);
		client->ec_buffer[tail - 1].type = EV_SYN;
		client->ec_buffer[tail - 1].code = SYN_DROPPED;

		wakeup(client);
		EVDEV_CLIENT_UNLOCKQ(client);
		return;
	}

	microtime(&client->ec_buffer[tail].time);
	client->ec_buffer[tail].type = type;
	client->ec_buffer[tail].code = code;
	client->ec_buffer[tail].value = value;
	client->ec_buffer_tail = (tail + 1) % count;

	wakeup(client);
	EVDEV_CLIENT_UNLOCKQ(client);
}

void
evdev_client_dumpqueue(struct evdev_client *client)
{
	struct input_event *event;
	int i, head, tail, size;

	head = client->ec_buffer_head;
	tail = client->ec_buffer_tail;
	size = client->ec_buffer_size;

	printf("evdev client: %p\n", client);
	printf("evdev provider name: %s\n", client->ec_evdev->ev_name);
	printf("event queue: head=%d tail=%d size=%d\n", tail, head, size);

	printf("queue contents:\n");

	for (i = 0; i < size; i++) {
		event = &client->ec_buffer[i];
		printf("%d: ", i);

		if (i < head || i > tail)
			printf("unused\n");
		else
			printf("type=%d code=%d value=%d ", event->type,
			    event->code, event->value);

		if (i == head)
			printf("<- head\n");
		else if (i == tail)
			printf("<- tail\n");
		else
			printf("\n");
	}
}

void
evdev_client_filter_queue(struct evdev_client *client, uint16_t type)
{
	struct input_event *event;
	int head, tail, count, i;
	bool last_was_syn = false;

	EVDEV_CLIENT_LOCKQ(client);

	i = head = client->ec_buffer_head;
	tail = client->ec_buffer_tail;
	count = client->ec_buffer_size;

	while (i != client->ec_buffer_tail) {
		event = &client->ec_buffer[i];
		i = (i + 1) % count;

		/* Skip event of given type */
		if (event->type == type)
			continue;

		/* Remove empty SYN_REPORT events */
		if (event->type == EV_SYN && event->code == SYN_REPORT &&
		    last_was_syn)
			continue;

		/* Rewrite entry */
		memcpy(&client->ec_buffer[tail], event,
		    sizeof(struct input_event));
	
		last_was_syn = (event->type == EV_SYN &&
		    event->code == SYN_REPORT);

		tail = (tail + 1) % count;
	}

	client->ec_buffer_head = i;
	client->ec_buffer_tail = tail;

	EVDEV_CLIENT_UNLOCKQ(client);
}
