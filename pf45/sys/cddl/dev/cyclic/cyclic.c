/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * Portions Copyright 2008 John Birrell <jb@freebsd.org>
 *
 * $FreeBSD$
 *
 * This is a simplified version of the cyclic timer subsystem from
 * OpenSolaris. In the FreeBSD version, we don't use interrupt levels.
 */

/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 *  The Cyclic Subsystem
 *  --------------------
 *
 *  Prehistory
 *
 *  Historically, most computer architectures have specified interval-based
 *  timer parts (e.g. SPARCstation's counter/timer; Intel's i8254).  While
 *  these parts deal in relative (i.e. not absolute) time values, they are
 *  typically used by the operating system to implement the abstraction of
 *  absolute time.  As a result, these parts cannot typically be reprogrammed
 *  without introducing error in the system's notion of time.
 *
 *  Starting in about 1994, chip architectures began specifying high resolution
 *  timestamp registers.  As of this writing (1999), all major chip families
 *  (UltraSPARC, PentiumPro, MIPS, PowerPC, Alpha) have high resolution
 *  timestamp registers, and two (UltraSPARC and MIPS) have added the capacity
 *  to interrupt based on timestamp values.  These timestamp-compare registers
 *  present a time-based interrupt source which can be reprogrammed arbitrarily
 *  often without introducing error.  Given the low cost of implementing such a
 *  timestamp-compare register (and the tangible benefit of eliminating
 *  discrete timer parts), it is reasonable to expect that future chip
 *  architectures will adopt this feature.
 *
 *  The cyclic subsystem has been designed to take advantage of chip
 *  architectures with the capacity to interrupt based on absolute, high
 *  resolution values of time.
 *
 *  Subsystem Overview
 *
 *  The cyclic subsystem is a low-level kernel subsystem designed to provide
 *  arbitrarily high resolution, per-CPU interval timers (to avoid colliding
 *  with existing terms, we dub such an interval timer a "cyclic").
 *  Alternatively, a cyclic may be specified to be "omnipresent", denoting
 *  firing on all online CPUs.
 *
 *  Cyclic Subsystem Interface Overview
 *  -----------------------------------
 *
 *  The cyclic subsystem has interfaces with the kernel at-large, with other
 *  kernel subsystems (e.g. the processor management subsystem, the checkpoint
 *  resume subsystem) and with the platform (the cyclic backend).  Each
 *  of these interfaces is given a brief synopsis here, and is described
 *  in full above the interface's implementation.
 *
 *  The following diagram displays the cyclic subsystem's interfaces to
 *  other kernel components.  The arrows denote a "calls" relationship, with
 *  the large arrow indicating the cyclic subsystem's consumer interface.
 *  Each arrow is labeled with the section in which the corresponding
 *  interface is described.
 *
 *           Kernel at-large consumers
 *           -----------++------------
 *                      ||
 *                      ||
 *                     _||_
 *                     \  /
 *                      \/
 *            +---------------------+
 *            |                     |
 *            |  Cyclic subsystem   |<-----------  Other kernel subsystems
 *            |                     |
 *            +---------------------+
 *                   ^       |
 *                   |       |
 *                   |       |
 *                   |       v
 *            +---------------------+
 *            |                     |
 *            |   Cyclic backend    |
 *            | (platform specific) |
 *            |                     |
 *            +---------------------+
 *
 *
 *  Kernel At-Large Interfaces
 *
 *      cyclic_add()         <-- Creates a cyclic
 *      cyclic_add_omni()    <-- Creates an omnipresent cyclic
 *      cyclic_remove()      <-- Removes a cyclic
 *
 *  Backend Interfaces
 *
 *      cyclic_init()        <-- Initializes the cyclic subsystem
 *      cyclic_fire()        <-- Interrupt entry point
 *
 *  The backend-supplied interfaces (through the cyc_backend structure) are
 *  documented in detail in <sys/cyclic_impl.h>
 *
 *
 *  Cyclic Subsystem Implementation Overview
 *  ----------------------------------------
 *
 *  The cyclic subsystem is designed to minimize interference between cyclics
 *  on different CPUs.  Thus, all of the cyclic subsystem's data structures
 *  hang off of a per-CPU structure, cyc_cpu.
 *
 *  Each cyc_cpu has a power-of-two sized array of cyclic structures (the
 *  cyp_cyclics member of the cyc_cpu structure).  If cyclic_add() is called
 *  and there does not exist a free slot in the cyp_cyclics array, the size of
 *  the array will be doubled.  The array will never shrink.  Cyclics are
 *  referred to by their index in the cyp_cyclics array, which is of type
 *  cyc_index_t.
 *
 *  The cyclics are kept sorted by expiration time in the cyc_cpu's heap.  The
 *  heap is keyed by cyclic expiration time, with parents expiring earlier
 *  than their children.
 *
 *  Heap Management
 *
 *  The heap is managed primarily by cyclic_fire().  Upon entry, cyclic_fire()
 *  compares the root cyclic's expiration time to the current time.  If the
 *  expiration time is in the past, cyclic_expire() is called on the root
 *  cyclic.  Upon return from cyclic_expire(), the cyclic's new expiration time
 *  is derived by adding its interval to its old expiration time, and a
 *  downheap operation is performed.  After the downheap, cyclic_fire()
 *  examines the (potentially changed) root cyclic, repeating the
 *  cyclic_expire()/add interval/cyclic_downheap() sequence until the root
 *  cyclic has an expiration time in the future.  This expiration time
 *  (guaranteed to be the earliest in the heap) is then communicated to the
 *  backend via cyb_reprogram.  Optimal backends will next call cyclic_fire()
 *  shortly after the root cyclic's expiration time.
 *
 *  To allow efficient, deterministic downheap operations, we implement the
 *  heap as an array (the cyp_heap member of the cyc_cpu structure), with each
 *  element containing an index into the CPU's cyp_cyclics array.
 *
 *  The heap is laid out in the array according to the following:
 *
 *   1.  The root of the heap is always in the 0th element of the heap array
 *   2.  The left and right children of the nth element are element
 *       (((n + 1) << 1) - 1) and element ((n + 1) << 1), respectively.
 *
 *  This layout is standard (see, e.g., Cormen's "Algorithms"); the proof
 *  that these constraints correctly lay out a heap (or indeed, any binary
 *  tree) is trivial and left to the reader.
 *
 *  To see the heap by example, assume our cyclics array has the following
 *  members (at time t):
 *
 *            cy_handler                          cy_expire
 *            ---------------------------------------------
 *     [ 0]   clock()                            t+10000000
 *     [ 1]   deadman()                        t+1000000000
 *     [ 2]   clock_highres_fire()                    t+100
 *     [ 3]   clock_highres_fire()                   t+1000
 *     [ 4]   clock_highres_fire()                    t+500
 *     [ 5]   (free)                                     --
 *     [ 6]   (free)                                     --
 *     [ 7]   (free)                                     --
 *
 *  The heap array could be:
 *
 *                [0]   [1]   [2]   [3]   [4]   [5]   [6]   [7]
 *              +-----+-----+-----+-----+-----+-----+-----+-----+
 *              |     |     |     |     |     |     |     |     |
 *              |  2  |  3  |  4  |  0  |  1  |  x  |  x  |  x  |
 *              |     |     |     |     |     |     |     |     |
 *              +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 *  Graphically, this array corresponds to the following (excuse the ASCII art):
 *
 *                                       2
 *                                       |
 *                    +------------------+------------------+
 *                    3                                     4
 *                    |
 *          +---------+--------+
 *          0                  1
 *
 *  Note that the heap is laid out by layer:  all nodes at a given depth are
 *  stored in consecutive elements of the array.  Moreover, layers of
 *  consecutive depths are in adjacent element ranges.  This property
 *  guarantees high locality of reference during downheap operations.
 *  Specifically, we are guaranteed that we can downheap to a depth of
 *
 *      lg (cache_line_size / sizeof (cyc_index_t))
 *
 *  nodes with at most one cache miss.  On UltraSPARC (64 byte e-cache line
 *  size), this corresponds to a depth of four nodes.  Thus, if there are
 *  fewer than sixteen cyclics in the heap, downheaps on UltraSPARC miss at
 *  most once in the e-cache.
 *
 *  Downheaps are required to compare siblings as they proceed down the
 *  heap.  For downheaps proceeding beyond the one-cache-miss depth, every
 *  access to a left child could potentially miss in the cache.  However,
 *  if we assume
 *
 *      (cache_line_size / sizeof (cyc_index_t)) > 2,
 *
 *  then all siblings are guaranteed to be on the same cache line.  Thus, the
 *  miss on the left child will guarantee a hit on the right child; downheaps
 *  will incur at most one cache miss per layer beyond the one-cache-miss
 *  depth.  The total number of cache misses for heap management during a
 *  downheap operation is thus bounded by
 *
 *      lg (n) - lg (cache_line_size / sizeof (cyc_index_t))
 *
 *  Traditional pointer-based heaps are implemented without regard to
 *  locality.  Downheaps can thus incur two cache misses per layer (one for
 *  each child), but at most one cache miss at the root.  This yields a bound
 *  of
 *
 *      2 * lg (n) - 1
 *
 *  on the total cache misses.
 *
 *  This difference may seem theoretically trivial (the difference is, after
 *  all, constant), but can become substantial in practice -- especially for
 *  caches with very large cache lines and high miss penalties (e.g. TLBs).
 *
 *  Heaps must always be full, balanced trees.  Heap management must therefore
 *  track the next point-of-insertion into the heap.  In pointer-based heaps,
 *  recomputing this point takes O(lg (n)).  Given the layout of the
 *  array-based implementation, however, the next point-of-insertion is
 *  always:
 *
 *      heap[number_of_elements]
 *
 *  We exploit this property by implementing the free-list in the usused
 *  heap elements.  Heap insertion, therefore, consists only of filling in
 *  the cyclic at cyp_cyclics[cyp_heap[number_of_elements]], incrementing
 *  the number of elements, and performing an upheap.  Heap deletion consists
 *  of decrementing the number of elements, swapping the to-be-deleted element
 *  with the element at cyp_heap[number_of_elements], and downheaping.
 *
 *  Filling in more details in our earlier example:
 *
 *                                               +--- free list head
 *                                               |
 *                                               V
 *
 *                [0]   [1]   [2]   [3]   [4]   [5]   [6]   [7]
 *              +-----+-----+-----+-----+-----+-----+-----+-----+
 *              |     |     |     |     |     |     |     |     |
 *              |  2  |  3  |  4  |  0  |  1  |  5  |  6  |  7  |
 *              |     |     |     |     |     |     |     |     |
 *              +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 *  To insert into this heap, we would just need to fill in the cyclic at
 *  cyp_cyclics[5], bump the number of elements (from 5 to 6) and perform
 *  an upheap.
 *
 *  If we wanted to remove, say, cyp_cyclics[3], we would first scan for it
 *  in the cyp_heap, and discover it at cyp_heap[1].  We would then decrement
 *  the number of elements (from 5 to 4), swap cyp_heap[1] with cyp_heap[4],
 *  and perform a downheap from cyp_heap[1].  The linear scan is required
 *  because the cyclic does not keep a backpointer into the heap.  This makes
 *  heap manipulation (e.g. downheaps) faster at the expense of removal
 *  operations.
 *
 *  Expiry processing
 *
 *  As alluded to above, cyclic_expire() is called by cyclic_fire() to expire
 *  a cyclic.  Cyclic subsystem consumers are guaranteed that for an arbitrary
 *  time t in the future, their cyclic handler will have been called
 *  (t - cyt_when) / cyt_interval times. cyclic_expire() simply needs to call
 *  the handler.
 *
 *  Resizing
 *
 *  All of the discussion thus far has assumed a static number of cyclics.
 *  Obviously, static limitations are not practical; we need the capacity
 *  to resize our data structures dynamically.
 *
 *  We resize our data structures lazily, and only on a per-CPU basis.
 *  The size of the data structures always doubles and never shrinks.  We
 *  serialize adds (and thus resizes) on cpu_lock; we never need to deal
 *  with concurrent resizes.  Resizes should be rare; they may induce jitter
 *  on the CPU being resized, but should not affect cyclic operation on other
 *  CPUs.
 *
 *  Three key cyc_cpu data structures need to be resized:  the cyclics array,
 *  nad the heap array.  Resizing is relatively straightforward:
 *
 *    1.  The new, larger arrays are allocated in cyclic_expand() (called
 *        from cyclic_add()).
 *    2.  The contents of the old arrays are copied into the new arrays.
 *    3.  The old cyclics array is bzero()'d
 *    4.  The pointers are updated.
 *
 *  Removals
 *
 *  Cyclic removals should be rare.  To simplify the implementation (and to
 *  allow optimization for the cyclic_fire()/cyclic_expire()
 *  path), we force removals and adds to serialize on cpu_lock.
 *
 */
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/cyclic_impl.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/dtrace_bsd.h>
#include <machine/cpu.h>

static kmem_cache_t *cyclic_id_cache;
static cyc_id_t *cyclic_id_head;
static cyc_backend_t cyclic_backend;

MALLOC_DEFINE(M_CYCLIC, "cyclic", "Cyclic timer subsystem");

/*
 * Returns 1 if the upheap propagated to the root, 0 if it did not.  This
 * allows the caller to reprogram the backend only when the root has been
 * modified.
 */
static int
cyclic_upheap(cyc_cpu_t *cpu, cyc_index_t ndx)
{
	cyclic_t *cyclics;
	cyc_index_t *heap;
	cyc_index_t heap_parent, heap_current = ndx;
	cyc_index_t parent, current;

	if (heap_current == 0)
		return (1);

	heap = cpu->cyp_heap;
	cyclics = cpu->cyp_cyclics;
	heap_parent = CYC_HEAP_PARENT(heap_current);

	for (;;) {
		current = heap[heap_current];
		parent = heap[heap_parent];

		/*
		 * We have an expiration time later than our parent; we're
		 * done.
		 */
		if (cyclics[current].cy_expire >= cyclics[parent].cy_expire)
			return (0);

		/*
		 * We need to swap with our parent, and continue up the heap.
		 */
		heap[heap_parent] = current;
		heap[heap_current] = parent;

		/*
		 * If we just reached the root, we're done.
		 */
		if (heap_parent == 0)
			return (1);

		heap_current = heap_parent;
		heap_parent = CYC_HEAP_PARENT(heap_current);
	}
}

static void
cyclic_downheap(cyc_cpu_t *cpu, cyc_index_t ndx)
{
	cyclic_t *cyclics = cpu->cyp_cyclics;
	cyc_index_t *heap = cpu->cyp_heap;

	cyc_index_t heap_left, heap_right, heap_me = ndx;
	cyc_index_t left, right, me;
	cyc_index_t nelems = cpu->cyp_nelems;

	for (;;) {
		/*
		 * If we don't have a left child (i.e., we're a leaf), we're
		 * done.
		 */
		if ((heap_left = CYC_HEAP_LEFT(heap_me)) >= nelems)
			return;

		left = heap[heap_left];
		me = heap[heap_me];

		heap_right = CYC_HEAP_RIGHT(heap_me);

		/*
		 * Even if we don't have a right child, we still need to compare
		 * our expiration time against that of our left child.
		 */
		if (heap_right >= nelems)
			goto comp_left;

		right = heap[heap_right];

		/*
		 * We have both a left and a right child.  We need to compare
		 * the expiration times of the children to determine which
		 * expires earlier.
		 */
		if (cyclics[right].cy_expire < cyclics[left].cy_expire) {
			/*
			 * Our right child is the earlier of our children.
			 * We'll now compare our expiration time to its; if
			 * ours is the earlier, we're done.
			 */
			if (cyclics[me].cy_expire <= cyclics[right].cy_expire)
				return;

			/*
			 * Our right child expires earlier than we do; swap
			 * with our right child, and descend right.
			 */
			heap[heap_right] = me;
			heap[heap_me] = right;
			heap_me = heap_right;
			continue;
		}

comp_left:
		/*
		 * Our left child is the earlier of our children (or we have
		 * no right child).  We'll now compare our expiration time
		 * to its; if ours is the earlier, we're done.
		 */
		if (cyclics[me].cy_expire <= cyclics[left].cy_expire)
			return;

		/*
		 * Our left child expires earlier than we do; swap with our
		 * left child, and descend left.
		 */
		heap[heap_left] = me;
		heap[heap_me] = left;
		heap_me = heap_left;
	}
}

static void
cyclic_expire(cyc_cpu_t *cpu, cyc_index_t ndx, cyclic_t *cyclic)
{
	cyc_func_t handler = cyclic->cy_handler;
	void *arg = cyclic->cy_arg;

	(*handler)(arg);
}

/*
 *  cyclic_fire(cpu_t *)
 *
 *  Overview
 *
 *    cyclic_fire() is the cyclic subsystem's interrupt handler.
 *    Called by the cyclic backend.
 *
 *  Arguments and notes
 *
 *    The only argument is the CPU on which the interrupt is executing;
 *    backends must call into cyclic_fire() on the specified CPU.
 *
 *    cyclic_fire() may be called spuriously without ill effect.  Optimal
 *    backends will call into cyclic_fire() at or shortly after the time
 *    requested via cyb_reprogram().  However, calling cyclic_fire()
 *    arbitrarily late will only manifest latency bubbles; the correctness
 *    of the cyclic subsystem does not rely on the timeliness of the backend.
 *
 *    cyclic_fire() is wait-free; it will not block or spin.
 *
 *  Return values
 *
 *    None.
 *
 */
static void
cyclic_fire(cpu_t *c)
{
	cyc_cpu_t *cpu = c->cpu_cyclic;
	cyc_backend_t *be = cpu->cyp_backend;
	cyc_index_t *heap = cpu->cyp_heap;
	cyclic_t *cyclic, *cyclics = cpu->cyp_cyclics;
	void *arg = be->cyb_arg;
	hrtime_t now = gethrtime();
	hrtime_t exp;

	if (cpu->cyp_nelems == 0) {
		/* This is a spurious fire. */
		return;
	}

	for (;;) {
		cyc_index_t ndx = heap[0];

		cyclic = &cyclics[ndx];

		ASSERT(!(cyclic->cy_flags & CYF_FREE));

		if ((exp = cyclic->cy_expire) > now)
			break;

		cyclic_expire(cpu, ndx, cyclic);

		/*
		 * If this cyclic will be set to next expire in the distant
		 * past, we have one of two situations:
		 *
		 *   a)	This is the first firing of a cyclic which had
		 *	cy_expire set to 0.
		 *
		 *   b)	We are tragically late for a cyclic -- most likely
		 *	due to being in the debugger.
		 *
		 * In either case, we set the new expiration time to be the
		 * the next interval boundary.  This assures that the
		 * expiration time modulo the interval is invariant.
		 *
		 * We arbitrarily define "distant" to be one second (one second
		 * is chosen because it's shorter than any foray to the
		 * debugger while still being longer than any legitimate
		 * stretch).
		 */
		exp += cyclic->cy_interval;

		if (now - exp > NANOSEC) {
			hrtime_t interval = cyclic->cy_interval;

			exp += ((now - exp) / interval + 1) * interval;
		}

		cyclic->cy_expire = exp;
		cyclic_downheap(cpu, 0);
	}

	/*
	 * Now we have a cyclic in the root slot which isn't in the past;
	 * reprogram the interrupt source.
	 */
	be->cyb_reprogram(arg, exp);
}

static void
cyclic_expand_xcall(cyc_xcallarg_t *arg)
{
	cyc_cpu_t *cpu = arg->cyx_cpu;
	cyc_index_t new_size = arg->cyx_size, size = cpu->cyp_size, i;
	cyc_index_t *new_heap = arg->cyx_heap;
	cyclic_t *cyclics = cpu->cyp_cyclics, *new_cyclics = arg->cyx_cyclics;

	/* Disable preemption and interrupts. */
	mtx_lock_spin(&cpu->cyp_mtx);

	/*
	 * Assert that the new size is a power of 2.
	 */
	ASSERT((new_size & (new_size - 1)) == 0);
	ASSERT(new_size == (size << 1));
	ASSERT(cpu->cyp_heap != NULL && cpu->cyp_cyclics != NULL);

	bcopy(cpu->cyp_heap, new_heap, sizeof (cyc_index_t) * size);
	bcopy(cyclics, new_cyclics, sizeof (cyclic_t) * size);

	/*
	 * Set up the free list, and set all of the new cyclics to be CYF_FREE.
	 */
	for (i = size; i < new_size; i++) {
		new_heap[i] = i;
		new_cyclics[i].cy_flags = CYF_FREE;
	}

	/*
	 * We can go ahead and plow the value of cyp_heap and cyp_cyclics;
	 * cyclic_expand() has kept a copy.
	 */
	cpu->cyp_heap = new_heap;
	cpu->cyp_cyclics = new_cyclics;
	cpu->cyp_size = new_size;
	mtx_unlock_spin(&cpu->cyp_mtx);
}

/*
 * cyclic_expand() will cross call onto the CPU to perform the actual
 * expand operation.
 */
static void
cyclic_expand(cyc_cpu_t *cpu)
{
	cyc_index_t new_size, old_size;
	cyc_index_t *new_heap, *old_heap;
	cyclic_t *new_cyclics, *old_cyclics;
	cyc_xcallarg_t arg;
	cyc_backend_t *be = cpu->cyp_backend;

	ASSERT(MUTEX_HELD(&cpu_lock));

	old_heap = cpu->cyp_heap;
	old_cyclics = cpu->cyp_cyclics;

	if ((new_size = ((old_size = cpu->cyp_size) << 1)) == 0) {
		new_size = CY_DEFAULT_PERCPU;
		ASSERT(old_heap == NULL && old_cyclics == NULL);
	}

	/*
	 * Check that the new_size is a power of 2.
	 */
	ASSERT(((new_size - 1) & new_size) == 0);

	new_heap = malloc(sizeof(cyc_index_t) * new_size, M_CYCLIC, M_WAITOK);
	new_cyclics = malloc(sizeof(cyclic_t) * new_size, M_CYCLIC, M_ZERO | M_WAITOK);

	arg.cyx_cpu = cpu;
	arg.cyx_heap = new_heap;
	arg.cyx_cyclics = new_cyclics;
	arg.cyx_size = new_size;

	be->cyb_xcall(be->cyb_arg, cpu->cyp_cpu,
	    (cyc_func_t)cyclic_expand_xcall, &arg);

	if (old_cyclics != NULL) {
		ASSERT(old_heap != NULL);
		ASSERT(old_size != 0);
		free(old_cyclics, M_CYCLIC);
		free(old_heap, M_CYCLIC);
	}
}

static void
cyclic_add_xcall(cyc_xcallarg_t *arg)
{
	cyc_cpu_t *cpu = arg->cyx_cpu;
	cyc_handler_t *hdlr = arg->cyx_hdlr;
	cyc_time_t *when = arg->cyx_when;
	cyc_backend_t *be = cpu->cyp_backend;
	cyc_index_t ndx, nelems;
	cyb_arg_t bar = be->cyb_arg;
	cyclic_t *cyclic;

	ASSERT(cpu->cyp_nelems < cpu->cyp_size);

	/* Disable preemption and interrupts. */
	mtx_lock_spin(&cpu->cyp_mtx);
	nelems = cpu->cyp_nelems++;

	if (nelems == 0) {
		/*
		 * If this is the first element, we need to enable the
		 * backend on this CPU.
		 */
		be->cyb_enable(bar);
	}

	ndx = cpu->cyp_heap[nelems];
	cyclic = &cpu->cyp_cyclics[ndx];

	ASSERT(cyclic->cy_flags == CYF_FREE);
	cyclic->cy_interval = when->cyt_interval;

	if (when->cyt_when == 0) {
		/*
		 * If a start time hasn't been explicitly specified, we'll
		 * start on the next interval boundary.
		 */
		cyclic->cy_expire = (gethrtime() / cyclic->cy_interval + 1) *
		    cyclic->cy_interval;
	} else {
		cyclic->cy_expire = when->cyt_when;
	}

	cyclic->cy_handler = hdlr->cyh_func;
	cyclic->cy_arg = hdlr->cyh_arg;
	cyclic->cy_flags = arg->cyx_flags;

	if (cyclic_upheap(cpu, nelems)) {
		hrtime_t exp = cyclic->cy_expire;

		/*
		 * If our upheap propagated to the root, we need to
		 * reprogram the interrupt source.
		 */
		be->cyb_reprogram(bar, exp);
	}
	mtx_unlock_spin(&cpu->cyp_mtx);

	arg->cyx_ndx = ndx;
}

static cyc_index_t
cyclic_add_here(cyc_cpu_t *cpu, cyc_handler_t *hdlr,
    cyc_time_t *when, uint16_t flags)
{
	cyc_backend_t *be = cpu->cyp_backend;
	cyb_arg_t bar = be->cyb_arg;
	cyc_xcallarg_t arg;

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(!(cpu->cyp_cpu->cpu_flags & CPU_OFFLINE));
	ASSERT(when->cyt_when >= 0 && when->cyt_interval > 0);

	if (cpu->cyp_nelems == cpu->cyp_size) {
		/*
		 * This is expensive; it will cross call onto the other
		 * CPU to perform the expansion.
		 */
		cyclic_expand(cpu);
		ASSERT(cpu->cyp_nelems < cpu->cyp_size);
	}

	/*
	 * By now, we know that we're going to be able to successfully
	 * perform the add.  Now cross call over to the CPU of interest to
	 * actually add our cyclic.
	 */
	arg.cyx_cpu = cpu;
	arg.cyx_hdlr = hdlr;
	arg.cyx_when = when;
	arg.cyx_flags = flags;

	be->cyb_xcall(bar, cpu->cyp_cpu, (cyc_func_t)cyclic_add_xcall, &arg);

	return (arg.cyx_ndx);
}

static void
cyclic_remove_xcall(cyc_xcallarg_t *arg)
{
	cyc_cpu_t *cpu = arg->cyx_cpu;
	cyc_backend_t *be = cpu->cyp_backend;
	cyb_arg_t bar = be->cyb_arg;
	cyc_index_t ndx = arg->cyx_ndx, nelems = cpu->cyp_nelems, i;
	cyc_index_t *heap = cpu->cyp_heap, last;
	cyclic_t *cyclic;

	ASSERT(nelems > 0);

	/* Disable preemption and interrupts. */
	mtx_lock_spin(&cpu->cyp_mtx);
	cyclic = &cpu->cyp_cyclics[ndx];

	/*
	 * Grab the current expiration time.  If this cyclic is being
	 * removed as part of a juggling operation, the expiration time
	 * will be used when the cyclic is added to the new CPU.
	 */
	if (arg->cyx_when != NULL) {
		arg->cyx_when->cyt_when = cyclic->cy_expire;
		arg->cyx_when->cyt_interval = cyclic->cy_interval;
	}

	/*
	 * Now set the flags to CYF_FREE.  We don't need a membar_enter()
	 * between zeroing pend and setting the flags because we're at
	 * CY_HIGH_LEVEL (that is, the zeroing of pend and the setting
	 * of cy_flags appear atomic to softints).
	 */
	cyclic->cy_flags = CYF_FREE;

	for (i = 0; i < nelems; i++) {
		if (heap[i] == ndx)
			break;
	}

	if (i == nelems)
		panic("attempt to remove non-existent cyclic");

	cpu->cyp_nelems = --nelems;

	if (nelems == 0) {
		/*
		 * If we just removed the last element, then we need to
		 * disable the backend on this CPU.
		 */
		be->cyb_disable(bar);
	}

	if (i == nelems) {
		/*
		 * If we just removed the last element of the heap, then
		 * we don't have to downheap.
		 */
		goto out;
	}

	/*
	 * Swap the last element of the heap with the one we want to
	 * remove, and downheap (this has the implicit effect of putting
	 * the newly freed element on the free list).
	 */
	heap[i] = (last = heap[nelems]);
	heap[nelems] = ndx;

	if (i == 0) {
		cyclic_downheap(cpu, 0);
	} else {
		if (cyclic_upheap(cpu, i) == 0) {
			/*
			 * The upheap didn't propagate to the root; if it
			 * didn't propagate at all, we need to downheap.
			 */
			if (heap[i] == last) {
				cyclic_downheap(cpu, i);
			}
			goto out;
		}
	}

	/*
	 * We're here because we changed the root; we need to reprogram
	 * the clock source.
	 */
	cyclic = &cpu->cyp_cyclics[heap[0]];

	ASSERT(nelems != 0);
	be->cyb_reprogram(bar, cyclic->cy_expire);
out:
	mtx_unlock_spin(&cpu->cyp_mtx);
}

static int
cyclic_remove_here(cyc_cpu_t *cpu, cyc_index_t ndx, cyc_time_t *when, int wait)
{
	cyc_backend_t *be = cpu->cyp_backend;
	cyc_xcallarg_t arg;

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(wait == CY_WAIT || wait == CY_NOWAIT);

	arg.cyx_ndx = ndx;
	arg.cyx_cpu = cpu;
	arg.cyx_when = when;
	arg.cyx_wait = wait;

	be->cyb_xcall(be->cyb_arg, cpu->cyp_cpu,
	    (cyc_func_t)cyclic_remove_xcall, &arg);

	return (1);
}

static void
cyclic_configure(cpu_t *c)
{
	cyc_cpu_t *cpu = malloc(sizeof(cyc_cpu_t), M_CYCLIC, M_ZERO | M_WAITOK);
	cyc_backend_t *nbe = malloc(sizeof(cyc_backend_t), M_CYCLIC, M_ZERO | M_WAITOK);

	ASSERT(MUTEX_HELD(&cpu_lock));

	if (cyclic_id_cache == NULL)
		cyclic_id_cache = kmem_cache_create("cyclic_id_cache",
		    sizeof (cyc_id_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	cpu->cyp_cpu = c;

	cpu->cyp_size = 1;
	cpu->cyp_heap = malloc(sizeof(cyc_index_t), M_CYCLIC, M_ZERO | M_WAITOK);
	cpu->cyp_cyclics = malloc(sizeof(cyclic_t), M_CYCLIC, M_ZERO | M_WAITOK);
	cpu->cyp_cyclics->cy_flags = CYF_FREE;

	mtx_init(&cpu->cyp_mtx, "cyclic cpu", NULL, MTX_SPIN);

	/*
	 * Setup the backend for this CPU.
	 */
	bcopy(&cyclic_backend, nbe, sizeof (cyc_backend_t));
	if (nbe->cyb_configure != NULL)
		nbe->cyb_arg = nbe->cyb_configure(c);
	cpu->cyp_backend = nbe;

	/*
	 * On platforms where stray interrupts may be taken during startup,
	 * the CPU's cpu_cyclic pointer serves as an indicator that the
	 * cyclic subsystem for this CPU is prepared to field interrupts.
	 */
	membar_producer();

	c->cpu_cyclic = cpu;
}

static void
cyclic_unconfigure(cpu_t *c)
{
	cyc_cpu_t *cpu = c->cpu_cyclic;
	cyc_backend_t *be = cpu->cyp_backend;
	cyb_arg_t bar = be->cyb_arg;

	ASSERT(MUTEX_HELD(&cpu_lock));

	c->cpu_cyclic = NULL;

	/*
	 * Let the backend know that the CPU is being yanked, and free up
	 * the backend structure.
	 */
	if (be->cyb_unconfigure != NULL)
		be->cyb_unconfigure(bar);
	free(be, M_CYCLIC);
	cpu->cyp_backend = NULL;

	mtx_destroy(&cpu->cyp_mtx);

	/* Finally, clean up our remaining dynamic structures. */
	free(cpu->cyp_cyclics, M_CYCLIC);
	free(cpu->cyp_heap, M_CYCLIC);
	free(cpu, M_CYCLIC);
}

static void
cyclic_omni_start(cyc_id_t *idp, cyc_cpu_t *cpu)
{
	cyc_omni_handler_t *omni = &idp->cyi_omni_hdlr;
	cyc_omni_cpu_t *ocpu = malloc(sizeof(cyc_omni_cpu_t), M_CYCLIC , M_WAITOK);
	cyc_handler_t hdlr;
	cyc_time_t when;

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(idp->cyi_cpu == NULL);

	hdlr.cyh_func = NULL;
	hdlr.cyh_arg = NULL;

	when.cyt_when = 0;
	when.cyt_interval = 0;

	omni->cyo_online(omni->cyo_arg, cpu->cyp_cpu, &hdlr, &when);

	ASSERT(hdlr.cyh_func != NULL);
	ASSERT(when.cyt_when >= 0 && when.cyt_interval > 0);

	ocpu->cyo_cpu = cpu;
	ocpu->cyo_arg = hdlr.cyh_arg;
	ocpu->cyo_ndx = cyclic_add_here(cpu, &hdlr, &when, 0);
	ocpu->cyo_next = idp->cyi_omni_list;
	idp->cyi_omni_list = ocpu;
}

static void
cyclic_omni_stop(cyc_id_t *idp, cyc_cpu_t *cpu)
{
	cyc_omni_handler_t *omni = &idp->cyi_omni_hdlr;
	cyc_omni_cpu_t *ocpu = idp->cyi_omni_list, *prev = NULL;

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(idp->cyi_cpu == NULL);
	ASSERT(ocpu != NULL);

	while (ocpu != NULL && ocpu->cyo_cpu != cpu) {
		prev = ocpu;
		ocpu = ocpu->cyo_next;
	}

	/*
	 * We _must_ have found an cyc_omni_cpu which corresponds to this
	 * CPU -- the definition of an omnipresent cyclic is that it runs
	 * on all online CPUs.
	 */
	ASSERT(ocpu != NULL);

	if (prev == NULL) {
		idp->cyi_omni_list = ocpu->cyo_next;
	} else {
		prev->cyo_next = ocpu->cyo_next;
	}

	(void) cyclic_remove_here(ocpu->cyo_cpu, ocpu->cyo_ndx, NULL, CY_WAIT);

	/*
	 * The cyclic has been removed from this CPU; time to call the
	 * omnipresent offline handler.
	 */
	if (omni->cyo_offline != NULL)
		omni->cyo_offline(omni->cyo_arg, cpu->cyp_cpu, ocpu->cyo_arg);

	free(ocpu, M_CYCLIC);
}

static cyc_id_t *
cyclic_new_id(void)
{
	cyc_id_t *idp;

	ASSERT(MUTEX_HELD(&cpu_lock));

	idp = kmem_cache_alloc(cyclic_id_cache, KM_SLEEP);

	/*
	 * The cyi_cpu field of the cyc_id_t structure tracks the CPU
	 * associated with the cyclic.  If and only if this field is NULL, the
	 * cyc_id_t is an omnipresent cyclic.  Note that cyi_omni_list may be
	 * NULL for an omnipresent cyclic while the cyclic is being created
	 * or destroyed.
	 */
	idp->cyi_cpu = NULL;
	idp->cyi_ndx = 0;

	idp->cyi_next = cyclic_id_head;
	idp->cyi_prev = NULL;
	idp->cyi_omni_list = NULL;

	if (cyclic_id_head != NULL) {
		ASSERT(cyclic_id_head->cyi_prev == NULL);
		cyclic_id_head->cyi_prev = idp;
	}

	cyclic_id_head = idp;

	return (idp);
}

/*
 *  cyclic_id_t cyclic_add(cyc_handler_t *, cyc_time_t *)
 *
 *  Overview
 *
 *    cyclic_add() will create an unbound cyclic with the specified handler and
 *    interval.  The cyclic will run on a CPU which both has interrupts enabled
 *    and is in the system CPU partition.
 *
 *  Arguments and notes
 *
 *    As its first argument, cyclic_add() takes a cyc_handler, which has the
 *    following members:
 *
 *      cyc_func_t cyh_func    <-- Cyclic handler
 *      void *cyh_arg          <-- Argument to cyclic handler
 *
 *    In addition to a cyc_handler, cyclic_add() takes a cyc_time, which
 *    has the following members:
 *
 *       hrtime_t cyt_when     <-- Absolute time, in nanoseconds since boot, at
 *                                 which to start firing
 *       hrtime_t cyt_interval <-- Length of interval, in nanoseconds
 *
 *    gethrtime() is the time source for nanoseconds since boot.  If cyt_when
 *    is set to 0, the cyclic will start to fire when cyt_interval next
 *    divides the number of nanoseconds since boot.
 *
 *    The cyt_interval field _must_ be filled in by the caller; one-shots are
 *    _not_ explicitly supported by the cyclic subsystem (cyclic_add() will
 *    assert that cyt_interval is non-zero).  The maximum value for either
 *    field is INT64_MAX; the caller is responsible for assuring that
 *    cyt_when + cyt_interval <= INT64_MAX.  Neither field may be negative.
 *
 *    For an arbitrary time t in the future, the cyclic handler is guaranteed
 *    to have been called (t - cyt_when) / cyt_interval times.  This will
 *    be true even if interrupts have been disabled for periods greater than
 *    cyt_interval nanoseconds.  In order to compensate for such periods,
 *    the cyclic handler may be called a finite number of times with an
 *    arbitrarily small interval.
 *
 *    The cyclic subsystem will not enforce any lower bound on the interval;
 *    if the interval is less than the time required to process an interrupt,
 *    the CPU will wedge.  It's the responsibility of the caller to assure that
 *    either the value of the interval is sane, or that its caller has
 *    sufficient privilege to deny service (i.e. its caller is root).
 *
 *  Return value
 *
 *    cyclic_add() returns a cyclic_id_t, which is guaranteed to be a value
 *    other than CYCLIC_NONE.  cyclic_add() cannot fail.
 *
 *  Caller's context
 *
 *    cpu_lock must be held by the caller, and the caller must not be in
 *    interrupt context.  cyclic_add() will perform a KM_SLEEP kernel
 *    memory allocation, so the usual rules (e.g. p_lock cannot be held)
 *    apply.  A cyclic may be added even in the presence of CPUs that have
 *    not been configured with respect to the cyclic subsystem, but only
 *    configured CPUs will be eligible to run the new cyclic.
 *
 *  Cyclic handler's context
 *
 *    Cyclic handlers will be executed in the interrupt context corresponding
 *    to the specified level (i.e. either high, lock or low level).  The
 *    usual context rules apply.
 *
 *    A cyclic handler may not grab ANY locks held by the caller of any of
 *    cyclic_add() or cyclic_remove(); the implementation of these functions
 *    may require blocking on cyclic handler completion.
 *    Moreover, cyclic handlers may not make any call back into the cyclic
 *    subsystem.
 */
cyclic_id_t
cyclic_add(cyc_handler_t *hdlr, cyc_time_t *when)
{
	cyc_id_t *idp = cyclic_new_id();
	solaris_cpu_t *c = &solaris_cpu[curcpu];

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(when->cyt_when >= 0 && when->cyt_interval > 0);

	idp->cyi_cpu = c->cpu_cyclic;
	idp->cyi_ndx = cyclic_add_here(idp->cyi_cpu, hdlr, when, 0);

	return ((uintptr_t)idp);
}

/*
 *  cyclic_id_t cyclic_add_omni(cyc_omni_handler_t *)
 *
 *  Overview
 *
 *    cyclic_add_omni() will create an omnipresent cyclic with the specified
 *    online and offline handlers.  Omnipresent cyclics run on all online
 *    CPUs, including CPUs which have unbound interrupts disabled.
 *
 *  Arguments
 *
 *    As its only argument, cyclic_add_omni() takes a cyc_omni_handler, which
 *    has the following members:
 *
 *      void (*cyo_online)()   <-- Online handler
 *      void (*cyo_offline)()  <-- Offline handler
 *      void *cyo_arg          <-- Argument to be passed to on/offline handlers
 *
 *  Online handler
 *
 *    The cyo_online member is a pointer to a function which has the following
 *    four arguments:
 *
 *      void *                 <-- Argument (cyo_arg)
 *      cpu_t *                <-- Pointer to CPU about to be onlined
 *      cyc_handler_t *        <-- Pointer to cyc_handler_t; must be filled in
 *                                 by omni online handler
 *      cyc_time_t *           <-- Pointer to cyc_time_t; must be filled in by
 *                                 omni online handler
 *
 *    The omni cyclic online handler is always called _before_ the omni
 *    cyclic begins to fire on the specified CPU.  As the above argument
 *    description implies, the online handler must fill in the two structures
 *    passed to it:  the cyc_handler_t and the cyc_time_t.  These are the
 *    same two structures passed to cyclic_add(), outlined above.  This
 *    allows the omni cyclic to have maximum flexibility; different CPUs may
 *    optionally
 *
 *      (a)  have different intervals
 *      (b)  be explicitly in or out of phase with one another
 *      (c)  have different handlers
 *      (d)  have different handler arguments
 *      (e)  fire at different levels
 *
 *    Of these, (e) seems somewhat dubious, but is nonetheless allowed.
 *
 *    The omni online handler is called in the same context as cyclic_add(),
 *    and has the same liberties:  omni online handlers may perform KM_SLEEP
 *    kernel memory allocations, and may grab locks which are also acquired
 *    by cyclic handlers.  However, omni cyclic online handlers may _not_
 *    call back into the cyclic subsystem, and should be generally careful
 *    about calling into arbitrary kernel subsystems.
 *
 *  Offline handler
 *
 *    The cyo_offline member is a pointer to a function which has the following
 *    three arguments:
 *
 *      void *                 <-- Argument (cyo_arg)
 *      cpu_t *                <-- Pointer to CPU about to be offlined
 *      void *                 <-- CPU's cyclic argument (that is, value
 *                                 to which cyh_arg member of the cyc_handler_t
 *                                 was set in the omni online handler)
 *
 *    The omni cyclic offline handler is always called _after_ the omni
 *    cyclic has ceased firing on the specified CPU.  Its purpose is to
 *    allow cleanup of any resources dynamically allocated in the omni cyclic
 *    online handler.  The context of the offline handler is identical to
 *    that of the online handler; the same constraints and liberties apply.
 *
 *    The offline handler is optional; it may be NULL.
 *
 *  Return value
 *
 *    cyclic_add_omni() returns a cyclic_id_t, which is guaranteed to be a
 *    value other than CYCLIC_NONE.  cyclic_add_omni() cannot fail.
 *
 *  Caller's context
 *
 *    The caller's context is identical to that of cyclic_add(), specified
 *    above.
 */
cyclic_id_t
cyclic_add_omni(cyc_omni_handler_t *omni)
{
	cyc_id_t *idp = cyclic_new_id();
	cyc_cpu_t *cpu;
	cpu_t *c;
	int i;

	ASSERT(MUTEX_HELD(&cpu_lock));
	ASSERT(omni != NULL && omni->cyo_online != NULL);

	idp->cyi_omni_hdlr = *omni;

	CPU_FOREACH(i) {
		c = &solaris_cpu[i];
		if ((cpu = c->cpu_cyclic) == NULL)
			continue;
		cyclic_omni_start(idp, cpu);
	}

	/*
	 * We must have found at least one online CPU on which to run
	 * this cyclic.
	 */
	ASSERT(idp->cyi_omni_list != NULL);
	ASSERT(idp->cyi_cpu == NULL);

	return ((uintptr_t)idp);
}

/*
 *  void cyclic_remove(cyclic_id_t)
 *
 *  Overview
 *
 *    cyclic_remove() will remove the specified cyclic from the system.
 *
 *  Arguments and notes
 *
 *    The only argument is a cyclic_id returned from either cyclic_add() or
 *    cyclic_add_omni().
 *
 *    By the time cyclic_remove() returns, the caller is guaranteed that the
 *    removed cyclic handler has completed execution (this is the same
 *    semantic that untimeout() provides).  As a result, cyclic_remove() may
 *    need to block, waiting for the removed cyclic to complete execution.
 *    This leads to an important constraint on the caller:  no lock may be
 *    held across cyclic_remove() that also may be acquired by a cyclic
 *    handler.
 *
 *  Return value
 *
 *    None; cyclic_remove() always succeeds.
 *
 *  Caller's context
 *
 *    cpu_lock must be held by the caller, and the caller must not be in
 *    interrupt context.  The caller may not hold any locks which are also
 *    grabbed by any cyclic handler.  See "Arguments and notes", above.
 */
void
cyclic_remove(cyclic_id_t id)
{
	cyc_id_t *idp = (cyc_id_t *)id;
	cyc_id_t *prev = idp->cyi_prev, *next = idp->cyi_next;
	cyc_cpu_t *cpu = idp->cyi_cpu;

	ASSERT(MUTEX_HELD(&cpu_lock));

	if (cpu != NULL) {
		(void) cyclic_remove_here(cpu, idp->cyi_ndx, NULL, CY_WAIT);
	} else {
		ASSERT(idp->cyi_omni_list != NULL);
		while (idp->cyi_omni_list != NULL)
			cyclic_omni_stop(idp, idp->cyi_omni_list->cyo_cpu);
	}

	if (prev != NULL) {
		ASSERT(cyclic_id_head != idp);
		prev->cyi_next = next;
	} else {
		ASSERT(cyclic_id_head == idp);
		cyclic_id_head = next;
	}

	if (next != NULL)
		next->cyi_prev = prev;

	kmem_cache_free(cyclic_id_cache, idp);
}

static void
cyclic_init(cyc_backend_t *be)
{
	ASSERT(MUTEX_HELD(&cpu_lock));

	/*
	 * Copy the passed cyc_backend into the backend template.  This must
	 * be done before the CPU can be configured.
	 */
	bcopy(be, &cyclic_backend, sizeof (cyc_backend_t));

	cyclic_configure(&solaris_cpu[curcpu]);
}

/*
 * It is assumed that cyclic_mp_init() is called some time after cyclic
 * init (and therefore, after cpu0 has been initialized).  We grab cpu_lock,
 * find the already initialized CPU, and initialize every other CPU with the
 * same backend.
 */
static void
cyclic_mp_init(void)
{
	cpu_t *c;
	int i;

	mutex_enter(&cpu_lock);

	CPU_FOREACH(i) {
		c = &solaris_cpu[i];
		if (c->cpu_cyclic == NULL)
			cyclic_configure(c);
	}

	mutex_exit(&cpu_lock);
}

static void
cyclic_uninit(void)
{
	cpu_t *c;
	int id;

	CPU_FOREACH(id) {
		c = &solaris_cpu[id];
		if (c->cpu_cyclic == NULL)
			continue;
		cyclic_unconfigure(c);
	}

	if (cyclic_id_cache != NULL)
		kmem_cache_destroy(cyclic_id_cache);
}

#include "cyclic_machdep.c"

/*
 *  Cyclic subsystem initialisation.
 */
static void
cyclic_load(void *dummy)
{
	mutex_enter(&cpu_lock);

	/* Initialise the machine-dependent backend. */
	cyclic_machdep_init();

	mutex_exit(&cpu_lock);
}

SYSINIT(cyclic_register, SI_SUB_CYCLIC, SI_ORDER_SECOND, cyclic_load, NULL);

static void
cyclic_unload(void)
{
	mutex_enter(&cpu_lock);

	/* Uninitialise the machine-dependent backend. */
	cyclic_machdep_uninit();

	mutex_exit(&cpu_lock);
}

SYSUNINIT(cyclic_unregister, SI_SUB_CYCLIC, SI_ORDER_SECOND, cyclic_unload, NULL);

/* ARGSUSED */
static int
cyclic_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		break;

	case MOD_UNLOAD:
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}
	return (error);
}

DEV_MODULE(cyclic, cyclic_modevent, NULL);
MODULE_VERSION(cyclic, 1);
MODULE_DEPEND(cyclic, opensolaris, 1, 1, 1);
