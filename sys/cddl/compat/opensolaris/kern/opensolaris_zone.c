/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/jail.h>
#include <sys/priv.h>
#include <sys/zone.h>

static MALLOC_DEFINE(M_ZONES, "zones_data", "Zones data");

/*
 * Structure to record list of ZFS datasets exported to a zone.
 */
typedef struct zone_dataset {
	LIST_ENTRY(zone_dataset) zd_next;
	char	zd_dataset[0];
} zone_dataset_t;

LIST_HEAD(zone_dataset_head, zone_dataset);

static struct prison_service *zone_prison_service = NULL;

int
zone_dataset_attach(struct ucred *cred, const char *dataset, int jailid)
{
	struct zone_dataset_head *head;
	zone_dataset_t *zd, *zd2;
	struct prison *pr;
	int error;

	if ((error = priv_check_cred(cred, PRIV_ZFS_JAIL, 0)) != 0)
		return (error);

	/* Allocate memory before we grab prison's mutex. */
	zd = malloc(sizeof(*zd) + strlen(dataset) + 1, M_ZONES, M_WAITOK);

	sx_slock(&allprison_lock);
	pr = prison_find(jailid);	/* Locks &pr->pr_mtx. */
	sx_sunlock(&allprison_lock);
	if (pr == NULL) {
		free(zd, M_ZONES);
		return (ENOENT);
	}

	head = prison_service_data_get(zone_prison_service, pr);
	LIST_FOREACH(zd2, head, zd_next) {
		if (strcmp(dataset, zd2->zd_dataset) == 0) {
			free(zd, M_ZONES);
			error = EEXIST;
			goto failure;
		}
	}
	strcpy(zd->zd_dataset, dataset);
	LIST_INSERT_HEAD(head, zd, zd_next);
failure:
	mtx_unlock(&pr->pr_mtx);
	return (error);
}

int
zone_dataset_detach(struct ucred *cred, const char *dataset, int jailid)
{
	struct zone_dataset_head *head;
	zone_dataset_t *zd;
	struct prison *pr;
	int error;

	if ((error = priv_check_cred(cred, PRIV_ZFS_JAIL, 0)) != 0)
		return (error);

	sx_slock(&allprison_lock);
	pr = prison_find(jailid);
	sx_sunlock(&allprison_lock);
	if (pr == NULL)
		return (ENOENT);
	head = prison_service_data_get(zone_prison_service, pr);
	LIST_FOREACH(zd, head, zd_next) {
		if (strcmp(dataset, zd->zd_dataset) == 0) {
			LIST_REMOVE(zd, zd_next);
			free(zd, M_ZONES);
			goto success;
		}
	}
	error = ENOENT;
success:
	mtx_unlock(&pr->pr_mtx);
	return (error);
}

/*
 * Returns true if the named dataset is visible in the current zone.
 * The 'write' parameter is set to 1 if the dataset is also writable.
 */
int
zone_dataset_visible(const char *dataset, int *write)
{
	struct zone_dataset_head *head;
	zone_dataset_t *zd;
	struct prison *pr;
	size_t len;
	int ret = 0;

	if (dataset[0] == '\0')
		return (0);
	if (INGLOBALZONE(curproc)) {
		if (write != NULL)
			*write = 1;
		return (1);
	}
	pr = curthread->td_ucred->cr_prison;
	mtx_lock(&pr->pr_mtx);
	head = prison_service_data_get(zone_prison_service, pr);

	/*
	 * Walk the list once, looking for datasets which match exactly, or
	 * specify a dataset underneath an exported dataset.  If found, return
	 * true and note that it is writable.
	 */
	LIST_FOREACH(zd, head, zd_next) {
		len = strlen(zd->zd_dataset);
		if (strlen(dataset) >= len &&
		    bcmp(dataset, zd->zd_dataset, len) == 0 &&
		    (dataset[len] == '\0' || dataset[len] == '/' ||
		    dataset[len] == '@')) {
			if (write)
				*write = 1;
			ret = 1;
			goto end;
		}
	}

	/*
	 * Walk the list a second time, searching for datasets which are parents
	 * of exported datasets.  These should be visible, but read-only.
	 *
	 * Note that we also have to support forms such as 'pool/dataset/', with
	 * a trailing slash.
	 */
	LIST_FOREACH(zd, head, zd_next) {
		len = strlen(dataset);
		if (dataset[len - 1] == '/')
			len--;	/* Ignore trailing slash */
		if (len < strlen(zd->zd_dataset) &&
		    bcmp(dataset, zd->zd_dataset, len) == 0 &&
		    zd->zd_dataset[len] == '/') {
			if (write)
				*write = 0;
			ret = 1;
			goto end;
		}
	}
end:
	mtx_unlock(&pr->pr_mtx);
	return (ret);
}

static int
zone_create(struct prison_service *psrv, struct prison *pr)
{
	struct zone_dataset_head *head;

	head = malloc(sizeof(*head), M_ZONES, M_WAITOK);
	LIST_INIT(head);
	mtx_lock(&pr->pr_mtx);
	prison_service_data_set(psrv, pr, head);
	mtx_unlock(&pr->pr_mtx);
	return (0);
}

static int
zone_destroy(struct prison_service *psrv, struct prison *pr)
{
	struct zone_dataset_head *head;
	zone_dataset_t *zd;

	mtx_lock(&pr->pr_mtx);
	head = prison_service_data_del(psrv, pr);
	mtx_unlock(&pr->pr_mtx);
	while ((zd = LIST_FIRST(head)) != NULL) {
		LIST_REMOVE(zd, zd_next);
		free(zd, M_ZONES);
	}
	free(head, M_ZONES);
	return (0);
}

static void
zone_sysinit(void *arg __unused)
{

	zone_prison_service = prison_service_register("zfs", zone_create,
	    zone_destroy);
}

static void
zone_sysuninit(void *arg __unused)
{

	prison_service_deregister(zone_prison_service);
}

SYSINIT(zone_sysinit, SI_SUB_DRIVERS, SI_ORDER_ANY, zone_sysinit, NULL);
SYSUNINIT(zone_sysuninit, SI_SUB_DRIVERS, SI_ORDER_ANY, zone_sysuninit, NULL);
