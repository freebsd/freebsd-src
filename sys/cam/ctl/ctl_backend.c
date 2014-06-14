/*-
 * Copyright (c) 2003 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_backend.c#3 $
 */
/*
 * CTL backend driver registration routines
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_frontend_internal.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_private.h>
#include <cam/ctl/ctl_debug.h>

extern struct ctl_softc *control_softc;

int
ctl_backend_register(struct ctl_backend_driver *be)
{
	struct ctl_softc *ctl_softc;
	struct ctl_backend_driver *be_tmp;

	ctl_softc = control_softc;

	mtx_lock(&ctl_softc->ctl_lock);
	/*
	 * Sanity check, make sure this isn't a duplicate registration.
	 */
	STAILQ_FOREACH(be_tmp, &ctl_softc->be_list, links) {
		if (strcmp(be_tmp->name, be->name) == 0) {
			mtx_unlock(&ctl_softc->ctl_lock);
			return (-1);
		}
	}
	mtx_unlock(&ctl_softc->ctl_lock);

	/*
	 * Call the backend's initialization routine.
	 */
	be->init();

	mtx_lock(&ctl_softc->ctl_lock);
	
	STAILQ_INSERT_TAIL(&ctl_softc->be_list, be, links);

	ctl_softc->num_backends++;

	/*
	 * Don't want to increment the usage count for internal consumers,
	 * we won't be able to unload otherwise.
	 */
	/* XXX KDM find a substitute for this? */
#if 0
	if ((be->flags & CTL_BE_FLAG_INTERNAL) == 0)
		MOD_INC_USE_COUNT;
#endif

#ifdef CS_BE_CONFIG_MOVE_DONE_IS_NOT_USED
	be->config_move_done = ctl_config_move_done;
#endif
	/* XXX KDM fix this! */
	be->num_luns = 0;
#if 0
	atomic_set(&be->num_luns, 0);
#endif

	mtx_unlock(&ctl_softc->ctl_lock);

	return (0);
}

int
ctl_backend_deregister(struct ctl_backend_driver *be)
{
	struct ctl_softc *ctl_softc;

	ctl_softc = control_softc;

	mtx_lock(&ctl_softc->ctl_lock);

#if 0
	if (atomic_read(&be->num_luns) != 0) {
#endif
	/* XXX KDM fix this! */
	if (be->num_luns != 0) {
		mtx_unlock(&ctl_softc->ctl_lock);
		return (-1);
	}

	STAILQ_REMOVE(&ctl_softc->be_list, be, ctl_backend_driver, links);

	ctl_softc->num_backends--;

	/* XXX KDM find a substitute for this? */
#if 0
	if ((be->flags & CTL_BE_FLAG_INTERNAL) == 0)
		MOD_DEC_USE_COUNT;
#endif

	mtx_unlock(&ctl_softc->ctl_lock);

	return (0);
}

struct ctl_backend_driver *
ctl_backend_find(char *backend_name)
{
	struct ctl_softc *ctl_softc;
	struct ctl_backend_driver *be_tmp;

	ctl_softc = control_softc;

	mtx_lock(&ctl_softc->ctl_lock);

	STAILQ_FOREACH(be_tmp, &ctl_softc->be_list, links) {
		if (strcmp(be_tmp->name, backend_name) == 0) {
			mtx_unlock(&ctl_softc->ctl_lock);
			return (be_tmp);
		}
	}

	mtx_unlock(&ctl_softc->ctl_lock);

	return (NULL);
}

void
ctl_init_opts(struct ctl_be_lun *be_lun, struct ctl_lun_req *req)
{
	struct ctl_be_lun_option *opt;
	int i;

	STAILQ_INIT(&be_lun->options);
	for (i = 0; i < req->num_be_args; i++) {
		opt = malloc(sizeof(*opt), M_CTL, M_WAITOK);
		opt->name = malloc(strlen(req->kern_be_args[i].kname) + 1, M_CTL, M_WAITOK);
		strcpy(opt->name, req->kern_be_args[i].kname);
		opt->value = malloc(strlen(req->kern_be_args[i].kvalue) + 1, M_CTL, M_WAITOK);
		strcpy(opt->value, req->kern_be_args[i].kvalue);
		STAILQ_INSERT_TAIL(&be_lun->options, opt, links);
	}
}

void
ctl_free_opts(struct ctl_be_lun *be_lun)
{
	struct ctl_be_lun_option *opt;

	while ((opt = STAILQ_FIRST(&be_lun->options)) != NULL) {
		STAILQ_REMOVE_HEAD(&be_lun->options, links);
		free(opt->name, M_CTL);
		free(opt->value, M_CTL);
		free(opt, M_CTL);
	}
}

char *
ctl_get_opt(struct ctl_be_lun *be_lun, const char *name)
{
	struct ctl_be_lun_option *opt;

	STAILQ_FOREACH(opt, &be_lun->options, links) {
		if (strcmp(opt->name, name) == 0) {
			return (opt->value);
		}
	}
	return (NULL);
}
