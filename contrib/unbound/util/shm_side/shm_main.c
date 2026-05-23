/*
 * util/shm_side/shm_main.c - SHM for statistics transport
 *
 * Copyright (c) 2017, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file contains functions for the SHM implementation.
 */

#include "config.h"
#include <ctype.h>
#include <stdarg.h>
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_SHM_H
#include <sys/shm.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include "shm_main.h"
#include "daemon/daemon.h"
#include "daemon/worker.h"
#include "daemon/stats.h"
#include "services/mesh.h"
#include "services/cache/rrset.h"
#include "services/cache/infra.h"
#include "validator/validator.h"
#include "util/config_file.h"
#include "util/fptr_wlist.h"
#include "util/log.h"

#ifdef HAVE_SHMGET
/** subtract timers and the values do not overflow or become negative */
static void
stat_timeval_subtract(long long *d_sec, long long *d_usec, const struct timeval* end,
	const struct timeval* start)
{
#ifndef S_SPLINT_S
	time_t end_usec = end->tv_usec;
	*d_sec = end->tv_sec - start->tv_sec;
	if(end_usec < start->tv_usec) {
		end_usec += 1000000;
		(*d_sec)--;
	}
	*d_usec = end_usec - start->tv_usec;
#endif
}
#endif /* HAVE_SHMGET */

int shm_main_init(struct daemon* daemon)
{
#ifdef HAVE_SHMGET
	struct ub_shm_stat_info *shm_stat;
	size_t shm_size;
	
	/* sanitize */
	if(!daemon)
		return 0;
	if(!daemon->cfg->shm_enable)
		return 1;
	if(daemon->cfg->stat_interval == 0)
		log_warn("shm-enable is yes but statistics-interval is 0");

	/* Statistics to maintain the number of thread + total */
	shm_size = (sizeof(struct ub_stats_info) * (daemon->num + 1));

	/* Allocation of needed memory */
	daemon->shm_info = (struct shm_main_info*)calloc(1, shm_size);

	/* Sanitize */
	if(!daemon->shm_info) {
		log_err("shm fail: malloc failure");
		return 0;
	}

	daemon->shm_info->key = daemon->cfg->shm_key;

	/* Check for previous create SHM */
	daemon->shm_info->id_ctl = shmget(daemon->shm_info->key, sizeof(int), SHM_R);
	daemon->shm_info->id_arr = shmget(daemon->shm_info->key + 1, sizeof(int), SHM_R);

	/* Destroy previous SHM */
	if (daemon->shm_info->id_ctl >= 0)
		shmctl(daemon->shm_info->id_ctl, IPC_RMID, NULL);

	/* Destroy previous SHM */
	if (daemon->shm_info->id_arr >= 0)
		shmctl(daemon->shm_info->id_arr, IPC_RMID, NULL);

	/* SHM: Create the segment */
	daemon->shm_info->id_ctl = shmget(daemon->shm_info->key, sizeof(struct ub_shm_stat_info), IPC_CREAT | 0644);

	if (daemon->shm_info->id_ctl < 0)
	{
		log_err("SHM failed(id_ctl) cannot shmget(key %d) %s",
			daemon->shm_info->key, strerror(errno));

		/* Just release memory unused */
		free(daemon->shm_info);
		daemon->shm_info = NULL;

		return 0;
	}

	daemon->shm_info->id_arr = shmget(daemon->shm_info->key + 1, shm_size, IPC_CREAT | 0644);

	if (daemon->shm_info->id_arr < 0)
	{
		log_err("SHM failed(id_arr) cannot shmget(key %d + 1) %s",
			daemon->shm_info->key, strerror(errno));

		/* Just release memory unused */
		free(daemon->shm_info);
		daemon->shm_info = NULL;

		return 0;
	}

	/* SHM: attach the segment  */
	daemon->shm_info->ptr_ctl = (struct ub_shm_stat_info*)
		shmat(daemon->shm_info->id_ctl, NULL, 0);
	if(daemon->shm_info->ptr_ctl == (void *) -1) {
		log_err("SHM failed(ctl) cannot shmat(%d) %s",
			daemon->shm_info->id_ctl, strerror(errno));

		/* Just release memory unused */
		free(daemon->shm_info);
		daemon->shm_info = NULL;

		return 0;
	}

	daemon->shm_info->ptr_arr = (struct ub_stats_info*)
		shmat(daemon->shm_info->id_arr, NULL, 0);

	if (daemon->shm_info->ptr_arr == (void *) -1)
	{
		log_err("SHM failed(arr) cannot shmat(%d) %s",
			daemon->shm_info->id_arr, strerror(errno));

		/* Just release memory unused */
		free(daemon->shm_info);
		daemon->shm_info = NULL;

		return 0;
	}

	/* Zero fill SHM to stand clean while is not filled by other events */
	memset(daemon->shm_info->ptr_ctl, 0, sizeof(struct ub_shm_stat_info));
	memset(daemon->shm_info->ptr_arr, 0, shm_size);

	shm_stat = daemon->shm_info->ptr_ctl;
	shm_stat->num_threads = daemon->num;

	lock_basic_init(&daemon->shm_info->lock);
	daemon->shm_info->volley_in_progress = 0;
	daemon->shm_info->thread_volley = (int*)calloc(
		daemon->num, sizeof(int));
	if(!daemon->shm_info->thread_volley) {
		log_err("shm fail: malloc failure");
		free(daemon->shm_info);
		daemon->shm_info = NULL;
		return 0;
	}
#else
	(void)daemon;
#endif /* HAVE_SHMGET */
	return 1;
}

void shm_main_shutdown(struct daemon* daemon)
{
#ifdef HAVE_SHMGET
	/* web are OK, just disabled */
	if(!daemon->cfg->shm_enable || !daemon->shm_info)
		return;

	verbose(VERB_DETAIL, "SHM shutdown - KEY [%d] - ID CTL [%d] ARR [%d] - PTR CTL [%p] ARR [%p]",
		daemon->shm_info->key, daemon->shm_info->id_ctl, daemon->shm_info->id_arr, daemon->shm_info->ptr_ctl, daemon->shm_info->ptr_arr);

	/* Destroy previous SHM */
	if (daemon->shm_info->id_ctl >= 0)
		shmctl(daemon->shm_info->id_ctl, IPC_RMID, NULL);

	if (daemon->shm_info->id_arr >= 0)
		shmctl(daemon->shm_info->id_arr, IPC_RMID, NULL);

	if (daemon->shm_info->ptr_ctl)
		shmdt(daemon->shm_info->ptr_ctl);

	if (daemon->shm_info->ptr_arr)
		shmdt(daemon->shm_info->ptr_arr);

	lock_basic_destroy(&daemon->shm_info->lock);
	free(daemon->shm_info->thread_volley);

	free(daemon->shm_info);
	daemon->shm_info = NULL;
#else
	(void)daemon;
#endif /* HAVE_SHMGET */
}

#ifdef HAVE_SHMGET
/** Copy general info into the stat structure. */
static void
shm_general_info(struct worker* worker)
{
	struct ub_shm_stat_info *shm_stat;
	/* Point to data into SHM */
#ifndef S_SPLINT_S
	shm_stat = worker->daemon->shm_info->ptr_ctl;
	shm_stat->time.now_sec = (long long)worker->env.now_tv->tv_sec;
	shm_stat->time.now_usec = (long long)worker->env.now_tv->tv_usec;
#endif

	stat_timeval_subtract(&shm_stat->time.up_sec, &shm_stat->time.up_usec, worker->env.now_tv, &worker->daemon->time_boot);
	stat_timeval_subtract(&shm_stat->time.elapsed_sec, &shm_stat->time.elapsed_usec, worker->env.now_tv, &worker->daemon->time_last_stat);

	shm_stat->mem.msg = (long long)slabhash_get_mem(worker->env.msg_cache);
	shm_stat->mem.rrset = (long long)slabhash_get_mem(&worker->env.rrset_cache->table);
	shm_stat->mem.dnscrypt_shared_secret = 0;
#ifdef USE_DNSCRYPT
	if(worker->daemon->dnscenv) {
		shm_stat->mem.dnscrypt_shared_secret = (long long)slabhash_get_mem(
			worker->daemon->dnscenv->shared_secrets_cache);
		shm_stat->mem.dnscrypt_nonce = (long long)slabhash_get_mem(
			worker->daemon->dnscenv->nonces_cache);
	}
#endif
	shm_stat->mem.val = (long long)mod_get_mem(&worker->env, "validator");
	shm_stat->mem.iter = (long long)mod_get_mem(&worker->env, "iterator");
	shm_stat->mem.respip = (long long)mod_get_mem(&worker->env, "respip");

	/* subnet mem value is available in shm, also when not enabled,
	 * to make the struct easier to memmap by other applications,
	 * independent of the configuration of unbound */
	shm_stat->mem.subnet = 0;
#ifdef CLIENT_SUBNET
	shm_stat->mem.subnet = (long long)mod_get_mem(&worker->env,
		"subnetcache");
#endif
	/* ipsecmod mem value is available in shm, also when not enabled,
	 * to make the struct easier to memmap by other applications,
	 * independent of the configuration of unbound */
	shm_stat->mem.ipsecmod = 0;
#ifdef USE_IPSECMOD
	shm_stat->mem.ipsecmod = (long long)mod_get_mem(&worker->env,
		"ipsecmod");
#endif
#ifdef WITH_DYNLIBMODULE
	shm_stat->mem.dynlib = (long long)mod_get_mem(&worker->env, "dynlib");
#endif
}
#endif /* HAVE_SHMGET */

#ifdef HAVE_SHMGET
/** See if the thread is first. Caller has lock. */
static int
shm_thread_is_first(struct shm_main_info* shm_info, int thread_num,
	struct daemon* daemon)
{
	/* The usual method, all threads executed last time, and there
	 * is no statistics callback in progress. */
	if(!shm_info->volley_in_progress)
		return 1;
	/* See if we are already active, if so, the timer seems to have fired
	 * twice for this thread which means other thread(s) have not gone
	 * through their stats callbacks yet.
	 * (There should have been a last thread to reset
	 * shm_info->volley_in_progress and shm_info->thread_volley)
	 * The other thread(s) are not yet active during this statistics round,
	 * so this thread must be the first of this new round disregarding the
	 * other busy thread(s).
	 * When the other thread(s) have time again, they will process their
	 * stats callback and hopefully properly end a stats round where all
	 * threads got to calculate their statistics. */
	if(shm_info->thread_volley[thread_num] != 0) {
		/* The new round starts and zeroes the total. The previous
		 * partial total is discarded. That means while other thread(s)
		 * are performing a long task, eg. loading a large zone
		 * perhaps, the total is not updated and stays the same in the
		 * shared memory area. Once that other thread(s) perform the
		 * statistic callback again, the total is updated again.
		 *
		 * The threads busy with long tasks have 0 in the array.
		 * The array is inited for a new round. */
		memset(shm_info->thread_volley, 0,
			((size_t)daemon->num) * sizeof(int));
		return 1;
	}
	return 0;
}
#endif /* HAVE_SHMGET */

#ifdef HAVE_SHMGET
/** See if the thread is last. Caller has lock. */
static int
shm_thread_is_last(struct daemon* daemon)
{
	/* Being last means that all threads have been active for this stats
	 * round and this thread is the last one; also active. All the
	 * thread_volley values should be true then. */
	int i;
	for(i=0; i<daemon->num; i++) {
		if(!daemon->shm_info->thread_volley[i])
			return 0;
	}
	return 1;
}
#endif /* HAVE_SHMGET */

void shm_main_run(struct worker *worker)
{
#ifdef HAVE_SHMGET
	struct ub_stats_info *stat_total;
	struct ub_stats_info *stat_info;
	int offset;
	double total_mesh_time_median;
	struct shm_main_info* shm_info = worker->daemon->shm_info;

#ifndef S_SPLINT_S
	verbose(VERB_DETAIL, "SHM run - worker [%d] - daemon [%p] - timenow(%u) - timeboot(%u)",
		worker->thread_num, worker->daemon, (unsigned)worker->env.now_tv->tv_sec, (unsigned)worker->daemon->time_boot.tv_sec);
#endif

	offset = worker->thread_num + 1;
	stat_total = shm_info->ptr_arr;
	stat_info = shm_info->ptr_arr + offset;

	/* Copy data to the current position */
	server_stats_compile(worker, stat_info, 0);

	/* Lock the lock and see if this thread is first or last of the
	 * stat threads. It can then zero value or sum up values. */
	lock_basic_lock(&shm_info->lock);
	if(shm_thread_is_first(shm_info, worker->thread_num, worker->daemon)) {
		/* First thread, zero fill total. */
		memset(&shm_info->total_in_progress, 0,
			sizeof(struct ub_stats_info));
		shm_info->volley_in_progress = 1;
	}
	shm_info->thread_volley[worker->thread_num] = 1;
	if(worker->thread_num == 0) {
		/* Thread 0, copy general info. */
		shm_general_info(worker);
	}
	/* Add thread data to the total */
	total_mesh_time_median = shm_info->total_in_progress.mesh_time_median;
	server_stats_add(&shm_info->total_in_progress, stat_info);
	/* By adding the value/num per stat thread, for the median,
	 * it is going to add up to the sum/num. */
	shm_info->total_in_progress.mesh_time_median = total_mesh_time_median +
		(stat_info->mesh_time_median/(double)worker->daemon->num);

	if(shm_thread_is_last(worker->daemon)) {
		/* Copy over the total */
		memcpy(stat_total, &shm_info->total_in_progress,
			sizeof(struct ub_stats_info));
		shm_info->volley_in_progress = 0;
		memset(shm_info->thread_volley, 0,
			((size_t)worker->daemon->num) * sizeof(int));
	}
	lock_basic_unlock(&shm_info->lock);
#else
	(void)worker;
#endif /* HAVE_SHMGET */
}
