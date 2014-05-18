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

#ifndef	_DEV_EVDEV_EVDEV_H
#define	_DEV_EVDEV_EVDEV_H

#include <sys/queue.h>
#include <sys/malloc.h>
#include <dev/evdev/input.h>

#define	NAMELEN		32

MALLOC_DECLARE(M_EVDEV);

struct evdev_dev;
struct evdev_client;

typedef int (evdev_open_t)(struct evdev_dev *, void *);
typedef void (evdev_close_t)(struct evdev_dev *, void *);
typedef void (evdev_event_t)(struct evdev_dev *, void *, uint16_t, uint16_t, int32_t);
typedef void (evdev_client_event_t)(struct evdev_client *, void *);

struct evdev_methods
{
	evdev_open_t		*ev_open;
	evdev_close_t		*ev_close;
	evdev_event_t		*ev_event;
};

struct evdev_dev
{
	char			ev_name[NAMELEN];
	char			ev_shortname[NAMELEN];
	char			ev_serial[NAMELEN];
	device_t		ev_dev;
	void *			ev_softc;
	struct cdev *		ev_cdev;

	/* Supported features: */
	uint32_t		ev_type_flags[howmany(EV_CNT, 32)];
	uint32_t		ev_key_flags[howmany(KEY_CNT, 32)];
	uint32_t		ev_rel_flags[howmany(REL_CNT, 32)];
	uint32_t		ev_abs_flags[howmany(ABS_CNT, 32)];
	uint32_t		ev_msc_flags[howmany(MSC_CNT, 32)];
	uint32_t		ev_led_flags[howmany(LED_CNT, 32)];
	uint32_t		ev_snd_flags[howmany(SND_CNT, 32)];
	uint32_t		ev_sw_flags[howmany(SW_CNT, 32)];
	struct input_absinfo *	ev_abs_info;

	/* State: */
	uint32_t		ev_key_states[howmany(KEY_CNT, 32)];
	uint32_t		ev_led_states[howmany(LED_CNT, 32)];
	uint32_t		ev_snd_states[howmany(SND_CNT, 32)];
	uint32_t		ev_sw_states[howmany(SW_CNT, 32)];

	/* Counters: */
	uint64_t		ev_event_count;
	int			ev_clients_count;

	struct evdev_methods *	ev_methods;

	LIST_ENTRY(evdev_dev) ev_link;
	LIST_HEAD(, evdev_client) ev_clients;
};

struct evdev_client
{
	struct evdev_dev *	ec_evdev;
	struct mtx		ec_buffer_mtx;
	struct input_event *	ec_buffer;
	int			ec_buffer_size;
	int			ec_buffer_head;
	int			ec_buffer_tail;
	bool			ec_enabled;
	bool			ec_stall;

	evdev_client_event_t *	ec_ev_notify;
	void *			ec_ev_arg;

	LIST_ENTRY(evdev_client) ec_link;
};

#define	EVDEV_CLIENT_LOCKQ(client)	mtx_lock(&(client)->ec_buffer_mtx)
#define	EVDEV_CLIENT_UNLOCKQ(client)	mtx_unlock(&(client)->ec_buffer_mtx)
#define	EVDEV_CLIENT_EMPTYQ(client) \
    ((client)->ec_buffer_head == (client)->ec_buffer_tail)

/* Input device interface: */
struct evdev_dev *evdev_alloc(void);
void evdev_set_name(struct evdev_dev *, const char *);
void evdev_set_serial(struct evdev_dev *, const char *);
void evdev_set_methods(struct evdev_dev *, struct evdev_methods *);
void evdev_set_softc(struct evdev_dev *, void *);
int evdev_register(device_t, struct evdev_dev *);
int evdev_unregister(device_t, struct evdev_dev *);
int evdev_push_event(struct evdev_dev *, uint16_t type, uint16_t code,
    int32_t value);
int evdev_sync(struct evdev_dev *);
int evdev_cdev_create(struct evdev_dev *);
int evdev_cdev_destroy(struct evdev_dev *);
void evdev_support_event(struct evdev_dev *, uint16_t);
void evdev_support_key(struct evdev_dev *, uint16_t);
void evdev_support_rel(struct evdev_dev *, uint16_t);
void evdev_support_abs(struct evdev_dev *, uint16_t);
void evdev_support_msc(struct evdev_dev *, uint16_t);
void evdev_support_led(struct evdev_dev *, uint16_t);
void evdev_support_snd(struct evdev_dev *, uint16_t);
void evdev_support_sw(struct evdev_dev *, uint16_t);

/* Client interface: */
int evdev_register_client(struct evdev_dev *, struct evdev_client **);
int evdev_dispose_client(struct evdev_client *);

#endif	/* _DEV_EVDEV_EVDEV_H */
