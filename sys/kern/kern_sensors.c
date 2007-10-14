/*	$FreeBSD$	*/
/*	$OpenBSD: kern_sensors.c,v 1.19 2007/06/04 18:42:05 deraadt Exp $	*/
/*	$OpenBSD: kern_sysctl.c,v 1.154 2007/06/01 17:29:10 beck Exp $	*/

/*-
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2006 Constantine A. Murenin <cnst+openbsd@bugmail.mojo.ru>
 * Copyright (c) 2007 Constantine A. Murenin <cnst+GSoC2007@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/sysctl.h>
#include <sys/sensors.h>

int			sensordev_count = 0;
SLIST_HEAD(, ksensordev) sensordev_list = SLIST_HEAD_INITIALIZER(sensordev_list);

struct ksensordev	*sensordev_get(int);
struct ksensor		*sensor_find(struct ksensordev *, enum sensor_type, int);

struct sensor_task {
	void				*arg;
	void				(*func)(void *);

	int				period;
	time_t				nextrun;
	volatile int			running;
	TAILQ_ENTRY(sensor_task)	entry;
};

void	sensor_task_thread(void *);
void	sensor_task_schedule(struct sensor_task *);

TAILQ_HEAD(, sensor_task) tasklist = TAILQ_HEAD_INITIALIZER(tasklist);

#ifndef NOSYSCTL8HACK
void	sensor_sysctl8magic_install(struct ksensordev *);
void	sensor_sysctl8magic_deinstall(struct ksensordev *);
#endif

void
sensordev_install(struct ksensordev *sensdev)
{
	struct ksensordev *v, *nv;

	mtx_lock(&Giant);
	if (sensordev_count == 0) {
		sensdev->num = 0;
		SLIST_INSERT_HEAD(&sensordev_list, sensdev, list);
	} else {
		for (v = SLIST_FIRST(&sensordev_list);
		    (nv = SLIST_NEXT(v, list)) != NULL; v = nv)
			if (nv->num - v->num > 1)
				break;
		sensdev->num = v->num + 1;
		SLIST_INSERT_AFTER(v, sensdev, list);
	}
	sensordev_count++;
	mtx_unlock(&Giant);

#ifndef NOSYSCTL8HACK
	sensor_sysctl8magic_install(sensdev);
#endif
}

void
sensor_attach(struct ksensordev *sensdev, struct ksensor *sens)
{
	struct ksensor *v, *nv;
	struct ksensors_head *sh;
	int i;

	mtx_lock(&Giant);
	sh = &sensdev->sensors_list;
	if (sensdev->sensors_count == 0) {
		for (i = 0; i < SENSOR_MAX_TYPES; i++)
			sensdev->maxnumt[i] = 0;
		sens->numt = 0;
		SLIST_INSERT_HEAD(sh, sens, list);
	} else {
		for (v = SLIST_FIRST(sh);
		    (nv = SLIST_NEXT(v, list)) != NULL; v = nv)
			if (v->type == sens->type && (v->type != nv->type || 
			    (v->type == nv->type && nv->numt - v->numt > 1)))
				break;
		/* sensors of the same type go after each other */
		if (v->type == sens->type)
			sens->numt = v->numt + 1;
		else
			sens->numt = 0;
		SLIST_INSERT_AFTER(v, sens, list);
	}
	/* we only increment maxnumt[] if the sensor was added
	 * to the last position of sensors of this type
	 */
	if (sensdev->maxnumt[sens->type] == sens->numt)
		sensdev->maxnumt[sens->type]++;
	sensdev->sensors_count++;
	mtx_unlock(&Giant);
}

void
sensordev_deinstall(struct ksensordev *sensdev)
{
	mtx_lock(&Giant);
	sensordev_count--;
	SLIST_REMOVE(&sensordev_list, sensdev, ksensordev, list);
	mtx_unlock(&Giant);

#ifndef NOSYSCTL8HACK
	sensor_sysctl8magic_deinstall(sensdev);
#endif
}

void
sensor_detach(struct ksensordev *sensdev, struct ksensor *sens)
{
	struct ksensors_head *sh;

	mtx_lock(&Giant);
	sh = &sensdev->sensors_list;
	sensdev->sensors_count--;
	SLIST_REMOVE(sh, sens, ksensor, list);
	/* we only decrement maxnumt[] if this is the tail 
	 * sensor of this type
	 */
	if (sens->numt == sensdev->maxnumt[sens->type] - 1)
		sensdev->maxnumt[sens->type]--;
	mtx_unlock(&Giant);
}

struct ksensordev *
sensordev_get(int num)
{
	struct ksensordev *sd;

	SLIST_FOREACH(sd, &sensordev_list, list)
		if (sd->num == num)
			return (sd);

	return (NULL);
}

struct ksensor *
sensor_find(struct ksensordev *sensdev, enum sensor_type type, int numt)
{
	struct ksensor *s;
	struct ksensors_head *sh;

	sh = &sensdev->sensors_list;
	SLIST_FOREACH(s, sh, list)
		if (s->type == type && s->numt == numt)
			return (s);

	return (NULL);
}

int
sensor_task_register(void *arg, void (*func)(void *), int period)
{
	struct sensor_task	*st;
	int			 create_thread = 0;

	st = malloc(sizeof(struct sensor_task), M_DEVBUF, M_NOWAIT);
	if (st == NULL)
		return (1);

	st->arg = arg;
	st->func = func;
	st->period = period;

	st->running = 1;

	if (TAILQ_EMPTY(&tasklist))
		create_thread = 1;

	st->nextrun = 0;
	TAILQ_INSERT_HEAD(&tasklist, st, entry);

	if (create_thread)
		if (kthread_create(sensor_task_thread, NULL, NULL, 0, 0,
		    "sensors") != 0)
			panic("sensors kthread");
	
	wakeup(&tasklist);

	return (0);
}

void
sensor_task_unregister(void *arg)
{
	struct sensor_task	*st;

	TAILQ_FOREACH(st, &tasklist, entry)
		if (st->arg == arg)
			st->running = 0;
}

void
sensor_task_thread(void *arg)
{
	struct sensor_task	*st, *nst;
	time_t			now;

	while (!TAILQ_EMPTY(&tasklist)) {
		while ((nst = TAILQ_FIRST(&tasklist))->nextrun >
		    (now = time_uptime))
			tsleep(&tasklist, PWAIT, "timeout",
			    (nst->nextrun - now) * hz);

		while ((st = nst) != NULL) {
			nst = TAILQ_NEXT(st, entry);

			if (st->nextrun > now)
				break;

			/* take it out while we work on it */
			TAILQ_REMOVE(&tasklist, st, entry);

			if (!st->running) {
				free(st, M_DEVBUF);
				continue;
			}

			/* run the task */
			st->func(st->arg);
			/* stick it back in the tasklist */
			sensor_task_schedule(st);
		}
	}

	kthread_exit(0);
}

void
sensor_task_schedule(struct sensor_task *st)
{
	struct sensor_task 	*cst;

	st->nextrun = time_uptime + st->period;

	TAILQ_FOREACH(cst, &tasklist, entry) {
		if (cst->nextrun > st->nextrun) {
			TAILQ_INSERT_BEFORE(cst, st, entry);
			return;
		}
	}

	/* must be an empty list, or at the end of the list */
	TAILQ_INSERT_TAIL(&tasklist, st, entry);
}

/*
 * sysctl glue code
 */
int sysctl_handle_sensordev(SYSCTL_HANDLER_ARGS);
int sysctl_handle_sensor(SYSCTL_HANDLER_ARGS);
int sysctl_sensors_handler(SYSCTL_HANDLER_ARGS);


#ifndef NOSYSCTL8HACK

SYSCTL_NODE(_hw, OID_AUTO, sensors, CTLFLAG_RD, NULL,
    "Hardware Sensors sysctl internal magic");
SYSCTL_NODE(_hw, HW_SENSORS, _sensors, CTLFLAG_RD, sysctl_sensors_handler,
    "Hardware Sensors XP MIB interface");

#else /* NOSYSCTL8HACK */

SYSCTL_NODE(_hw, HW_SENSORS, sensors, CTLFLAG_RD, sysctl_sensors_handler,
    "Hardware Sensors");

#endif /* !NOSYSCTL8HACK */


#ifndef NOSYSCTL8HACK

/*
 * XXX:
 * FreeBSD's sysctl(9) .oid_handler functionality is not accustomed
 * for the CTLTYPE_NODE handler to handle the undocumented sysctl
 * magic calls.  As soon as such functionality is developed, 
 * sysctl_sensors_handler() should be converted to handle all such
 * calls, and these sysctl_add_oid(9) calls should be removed 
 * "with a big axe".  This whole sysctl_add_oid(9) business is solely
 * to please sysctl(8).
 */

void
sensor_sysctl8magic_install(struct ksensordev *sensdev)
{
	struct sysctl_oid_list *ol;	
	struct sysctl_ctx_list *cl = &sensdev->clist;
	struct ksensor *s;
	struct ksensors_head *sh = &sensdev->sensors_list;

	sysctl_ctx_init(cl);
	ol = SYSCTL_CHILDREN(SYSCTL_ADD_NODE(cl, &SYSCTL_NODE_CHILDREN(_hw,
	    sensors), sensdev->num, sensdev->xname, CTLFLAG_RD, NULL, ""));
	SLIST_FOREACH(s, sh, list) {
		char n[32];

		snprintf(n, sizeof(n), "%s%d", sensor_type_s[s->type], s->numt);
		SYSCTL_ADD_PROC(cl, ol, OID_AUTO, n, CTLTYPE_STRUCT |
		    CTLFLAG_RD, s, 0, sysctl_handle_sensor, "S,sensor", "");
	}
}

void
sensor_sysctl8magic_deinstall(struct ksensordev *sensdev)
{
	struct sysctl_ctx_list *cl = &sensdev->clist;

	sysctl_ctx_free(cl);
}

#endif /* !NOSYSCTL8HACK */


int
sysctl_handle_sensordev(SYSCTL_HANDLER_ARGS)
{
	struct ksensordev *ksd = arg1;
	struct sensordev *usd;
	int error;

	if (req->newptr)
		return (EPERM);

	/* Grab a copy, to clear the kernel pointers */
	usd = malloc(sizeof(*usd), M_TEMP, M_WAITOK);
	bzero(usd, sizeof(*usd));
	usd->num = ksd->num;
	strlcpy(usd->xname, ksd->xname, sizeof(usd->xname));
	memcpy(usd->maxnumt, ksd->maxnumt, sizeof(usd->maxnumt));
	usd->sensors_count = ksd->sensors_count;

	error = SYSCTL_OUT(req, usd, sizeof(struct sensordev));

	free(usd, M_TEMP);
	return (error);
}

int
sysctl_handle_sensor(SYSCTL_HANDLER_ARGS)
{
	struct ksensor *ks = arg1;
	struct sensor *us;
	int error;

	if (req->newptr)
		return (EPERM);

	/* Grab a copy, to clear the kernel pointers */
	us = malloc(sizeof(*us), M_TEMP, M_WAITOK);
	bzero(us, sizeof(*us));
	memcpy(us->desc, ks->desc, sizeof(ks->desc));
	us->tv = ks->tv;
	us->value = ks->value;
	us->type = ks->type;
	us->status = ks->status;
	us->numt = ks->numt;
	us->flags = ks->flags;

	error = SYSCTL_OUT(req, us, sizeof(struct sensor));

	free(us, M_TEMP);
	return (error);
}

int
sysctl_sensors_handler(SYSCTL_HANDLER_ARGS)
{
	int *name = arg1;
	u_int namelen = arg2;
	struct ksensordev *ksd;
	struct ksensor *ks;
	int dev, numt;
	enum sensor_type type;

	if (namelen != 1 && namelen != 3)
		return (ENOTDIR);

	dev = name[0];
	if ((ksd = sensordev_get(dev)) == NULL)
		return (ENOENT);
	if (namelen == 1)
		return (sysctl_handle_sensordev(NULL, ksd, 0, req));

	type = name[1];
	numt = name[2];
	if ((ks = sensor_find(ksd, type, numt)) == NULL)
		return (ENOENT);
	return (sysctl_handle_sensor(NULL, ks, 0, req));
}
