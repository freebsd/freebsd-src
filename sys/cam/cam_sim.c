/*
 * Common functions for SCSI Interface Modules (SIMs).
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_queue.h>

#define CAM_PATH_ANY (u_int32_t)-1

struct cam_devq *
cam_simq_alloc(u_int32_t max_sim_transactions)
{
	return (cam_devq_alloc(/*size*/0, max_sim_transactions));
}

void
cam_simq_free(struct cam_devq *devq)
{
	cam_devq_free(devq);
}

struct cam_sim *
cam_sim_alloc(sim_action_func sim_action, sim_poll_func sim_poll,
	      char *sim_name, void *softc, u_int32_t unit,
	      int max_dev_transactions,
	      int max_tagged_dev_transactions, struct cam_devq *queue)
{
	struct cam_sim *sim;

	/*
	 * If this is the xpt layer creating a sim, then it's OK
	 * to wait for an allocation.
	 *
	 * XXX Should we pass in a flag to indicate that wait is OK?
	 */
	if (strcmp(sim_name, "xpt") == 0)
		sim = (struct cam_sim *)malloc(sizeof(struct cam_sim),
					       M_DEVBUF, M_WAITOK);
	else
		sim = (struct cam_sim *)malloc(sizeof(struct cam_sim),
					       M_DEVBUF, M_NOWAIT);

	if (sim != NULL) {
		sim->sim_action = sim_action;
		sim->sim_poll = sim_poll;
		sim->sim_name = sim_name;
		sim->softc = softc;
		sim->path_id = CAM_PATH_ANY;
		sim->unit_number = unit;
		sim->bus_id = 0;	/* set in xpt_bus_register */
		sim->max_tagged_dev_openings = max_tagged_dev_transactions;
		sim->max_dev_openings = max_dev_transactions;
		sim->flags = 0;
		callout_handle_init(&sim->c_handle);
		sim->devq = queue;
	}

	return (sim);
}

void
cam_sim_free(struct cam_sim *sim, int free_devq)
{
	if (free_devq)
		cam_simq_free(sim->devq);
	free(sim, M_DEVBUF);
}

void
cam_sim_set_path(struct cam_sim *sim, u_int32_t path_id)
{
	sim->path_id = path_id;
}
