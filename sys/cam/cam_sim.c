/*-
 * Common functions for SCSI Interface Modules (SIMs).
 *
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt.h>

#define CAM_PATH_ANY (uint32_t)-1

static MALLOC_DEFINE(M_CAMSIM, "CAM SIM", "CAM SIM buffers");

static struct mtx cam_sim_free_mtx;
MTX_SYSINIT(cam_sim_free_init, &cam_sim_free_mtx, "CAM SIM free lock", MTX_DEF);

struct cam_devq *
cam_simq_alloc(uint32_t max_sim_transactions)
{
	return (cam_devq_alloc(/*size*/0, max_sim_transactions));
}

void
cam_simq_free(struct cam_devq *devq)
{
	cam_devq_free(devq);
}



/**
 * @brief allocate a new sim and fill in the details
 *
 * A Storage Interface Module (SIM) is the interface between CAM and
 * hardware. SIM receives CCBs from CAM via @p sim_action callback and
 * translates them into DMA or other hardware transactions.  During system
 * dumps, it can be polled with the @p sim_poll callback. CCB processing is
 * terminated by calling @c xpt_done().
 *
 * The @p mtx acts as a perimeter lock for the SIM. All calls into the SIM's
 * @p sim_action are made with this lock held. It is also used to hold/release
 * a SIM, managing its reference count. When the lock is NULL, the SIM is 100%
 * responsible for locking (and the reference counting is done with a shared
 * lock.
 *
 * The cam_devq passed in (@c queue) is used to arbitrate the number of
 * outstanding transactions to the SIM. For HBAs that have global limits shared
 * between the different buses, the same devq should be specified for each bus
 * attached to the SIM.
 *
 * @param sim_action	Function to call to process CCBs
 * @param sim_poll	Function to poll the hardware for completions
 * @param sim_name	Name of SIM class
 * @param softc		Software context associated with the SIM
 * @param unit		Unit number of SIM
 * @param mtx		Mutex to lock while interacting with the SIM, or NULL
 *			for a SIM that handle its own locking to enable multi
 *			queue support.
 * @param max_dev_transactions Maximum number of concurrent untagged
 *			transactions possible
 * @param max_tagged_dev_transactions Maximum number of concurrent tagged
 *			transactions possible.
 * @param queue		The cam_devq to use for this SIM.
 */
struct cam_sim *
cam_sim_alloc(sim_action_func sim_action, sim_poll_func sim_poll,
	      const char *sim_name, void *softc, uint32_t unit,
	      struct mtx *mtx, int max_dev_transactions,
	      int max_tagged_dev_transactions, struct cam_devq *queue)
{
	struct cam_sim *sim;

	sim = malloc(sizeof(struct cam_sim), M_CAMSIM, M_ZERO | M_NOWAIT);
	if (sim == NULL)
		return (NULL);

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
	sim->refcount = 1;
	sim->devq = queue;
	sim->mtx = mtx;
	return (sim);
}

/**
 * @brief frees up the sim
 *
 * Frees up the CAM @c sim and optionally the devq. If a mutex is associated
 * with the sim, it must be locked on entry. It will remain locked on
 * return.
 *
 * This function will wait for all outstanding reference to the sim to clear
 * before returning.
 *
 * @param sim          The sim to free
 * @param free_devq    Free the devq associated with the sim at creation.
 */
void
cam_sim_free(struct cam_sim *sim, int free_devq)
{
	struct mtx *mtx;
	int error __diagused;

	if (sim->mtx == NULL) {
		mtx = &cam_sim_free_mtx;
		mtx_lock(mtx);
	} else {
		mtx = sim->mtx;
		mtx_assert(mtx, MA_OWNED);
	}
	KASSERT(sim->refcount >= 1, ("sim->refcount >= 1"));
	sim->refcount--;
	if (sim->refcount > 0) {
		error = msleep(sim, mtx, PRIBIO, "simfree", 0);
		KASSERT(error == 0, ("invalid error value for msleep(9)"));
	}
	KASSERT(sim->refcount == 0, ("sim->refcount == 0"));
	if (mtx == &cam_sim_free_mtx)	/* sim->mtx == NULL */
		mtx_unlock(mtx);

	if (free_devq)
		cam_simq_free(sim->devq);
	free(sim, M_CAMSIM);
}

void
cam_sim_release(struct cam_sim *sim)
{
	struct mtx *mtx;

	if (sim->mtx == NULL)
		mtx = &cam_sim_free_mtx;
	else if (!mtx_owned(sim->mtx))
		mtx = sim->mtx;
	else
		mtx = NULL;	/* We hold the lock. */
	if (mtx)
		mtx_lock(mtx);
	KASSERT(sim->refcount >= 1, ("sim->refcount >= 1"));
	sim->refcount--;
	if (sim->refcount == 0)
		wakeup(sim);
	if (mtx)
		mtx_unlock(mtx);
}

void
cam_sim_hold(struct cam_sim *sim)
{
	struct mtx *mtx;

	if (sim->mtx == NULL)
		mtx = &cam_sim_free_mtx;
	else if (!mtx_owned(sim->mtx))
		mtx = sim->mtx;
	else
		mtx = NULL;	/* We hold the lock. */
	if (mtx)
		mtx_lock(mtx);
	KASSERT(sim->refcount >= 1, ("sim->refcount >= 1"));
	sim->refcount++;
	if (mtx)
		mtx_unlock(mtx);
}

void
cam_sim_set_path(struct cam_sim *sim, uint32_t path_id)
{
	sim->path_id = path_id;
}
