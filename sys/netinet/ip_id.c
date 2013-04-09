
/*-
 * Copyright (c) 2008 Michael J. Silbersack.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * IP ID generation is a fascinating topic.
 *
 * In order to avoid ID collisions during packet reassembly, common sense
 * dictates that the period between reuse of IDs be as large as possible.
 * This leads to the classic implementation of a system-wide counter, thereby
 * ensuring that IDs repeat only once every 2^16 packets.
 *
 * Subsequent security researchers have pointed out that using a global
 * counter makes ID values predictable.  This predictability allows traffic
 * analysis, idle scanning, and even packet injection in specific cases.
 * These results suggest that IP IDs should be as random as possible.
 *
 * The "searchable queues" algorithm used in this IP ID implementation was
 * proposed by Amit Klein.  It is a compromise between the above two
 * viewpoints that has provable behavior that can be tuned to the user's
 * requirements.
 *
 * The basic concept is that we supplement a standard random number generator
 * with a queue of the last L IDs that we have handed out to ensure that all
 * IDs have a period of at least L.
 *
 * To efficiently implement this idea, we keep two data structures: a
 * circular array of IDs of size L and a bitstring of 65536 bits.
 *
 * To start, we ask the RNG for a new ID.  A quick index into the bitstring
 * is used to determine if this is a recently used value.  The process is
 * repeated until a value is returned that is not in the bitstring.
 *
 * Having found a usable ID, we remove the ID stored at the current position
 * in the queue from the bitstring and replace it with our new ID.  Our new
 * ID is then added to the bitstring and the queue pointer is incremented.
 *
 * The lower limit of 512 was chosen because there doesn't seem to be much
 * point to having a smaller value.  The upper limit of 32768 was chosen for
 * two reasons.  First, every step above 32768 decreases the entropy.  Taken
 * to an extreme, 65533 would offer 1 bit of entropy.  Second, the number of
 * attempts it takes the algorithm to find an unused ID drastically
 * increases, killing performance.  The default value of 8192 was chosen
 * because it provides a good tradeoff between randomness and non-repetition.
 *
 * With L=8192, the queue will use 16K of memory.  The bitstring always
 * uses 8K of memory.  No memory is allocated until the use of random ids is
 * enabled.
 */

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <sys/bitstring.h>

static MALLOC_DEFINE(M_IPID, "ipid", "randomized ip id state");

static u_int16_t 	*id_array = NULL;
static bitstr_t		*id_bits = NULL;
static int		 array_ptr = 0;
static int		 array_size = 8192;
static int		 random_id_collisions = 0;
static int		 random_id_total = 0;
static struct mtx_padalign  ip_id_mtx;

static void	ip_initid(void);
static int	sysctl_ip_id_change(SYSCTL_HANDLER_ARGS);

MTX_SYSINIT(ip_id_mtx, &ip_id_mtx, "ip_id_mtx", MTX_DEF);

SYSCTL_DECL(_net_inet_ip);
SYSCTL_PROC(_net_inet_ip, OID_AUTO, random_id_period, CTLTYPE_INT|CTLFLAG_RW,
    &array_size, 0, sysctl_ip_id_change, "IU", "IP ID Array size");
SYSCTL_INT(_net_inet_ip, OID_AUTO, random_id_collisions, CTLFLAG_RD,
    &random_id_collisions, 0, "Count of IP ID collisions");
SYSCTL_INT(_net_inet_ip, OID_AUTO, random_id_total, CTLFLAG_RD,
    &random_id_total, 0, "Count of IP IDs created");

static int
sysctl_ip_id_change(SYSCTL_HANDLER_ARGS)
{
	int error, new;

	new = array_size;
	error = sysctl_handle_int(oidp, &new, 0, req);
	if (error == 0 && req->newptr) {
		if (new >= 512 && new <= 32768) {
			mtx_lock(&ip_id_mtx);
			array_size = new;
			ip_initid();
			mtx_unlock(&ip_id_mtx);
		} else
			error = EINVAL;
	}
	return (error);
}

/*
 * ip_initid() runs with a mutex held and may execute in a network context.
 * As a result, it uses M_NOWAIT.  Ideally, we would always do this
 * allocation from the sysctl contact and have it be an invariant that if
 * this random ID allocation mode is selected, the buffers are present.  This
 * would also avoid potential network context failures of IP ID generation.
 */
static void
ip_initid(void)
{

	mtx_assert(&ip_id_mtx, MA_OWNED);

	if (id_array != NULL) {
		free(id_array, M_IPID);
		free(id_bits, M_IPID);
	}
	random_id_collisions = 0;
	random_id_total = 0;
	array_ptr = 0;
	id_array = (u_int16_t *) malloc(array_size * sizeof(u_int16_t),
	    M_IPID, M_NOWAIT | M_ZERO);
	id_bits = (bitstr_t *) malloc(bitstr_size(65536), M_IPID,
	    M_NOWAIT | M_ZERO);
	if (id_array == NULL || id_bits == NULL) {
		/* Neither or both. */
		if (id_array != NULL) {
			free(id_array, M_IPID);
			id_array = NULL;
		}
		if (id_bits != NULL) {
			free(id_bits, M_IPID);
			id_bits = NULL;
		}
	}
}

u_int16_t
ip_randomid(void)
{
	u_int16_t new_id;

	mtx_lock(&ip_id_mtx);
	if (id_array == NULL)
		ip_initid();

	/*
	 * Fail gracefully; return a fixed id if memory allocation failed;
	 * ideally we wouldn't do allocation in this context in order to
	 * avoid the possibility of this failure mode.
	 */
	if (id_array == NULL) {
		mtx_unlock(&ip_id_mtx);
		return (1);
	}

	/*
	 * To avoid a conflict with the zeros that the array is initially
	 * filled with, we never hand out an id of zero.
	 */
	new_id = 0;
	do {
		if (new_id != 0)
			random_id_collisions++;
		arc4rand(&new_id, sizeof(new_id), 0);
	} while (bit_test(id_bits, new_id) || new_id == 0);
	bit_clear(id_bits, id_array[array_ptr]);
	bit_set(id_bits, new_id);
	id_array[array_ptr] = new_id;
	array_ptr++;
	if (array_ptr == array_size)
		array_ptr = 0;
	random_id_total++;
	mtx_unlock(&ip_id_mtx);
	return (new_id);
}
