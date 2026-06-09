# Deferred work in net80211

## Overview

The work of driving the driver, interface and node state machines is
partially implemented as a set of deferred work tasks which are
serialised on a driver task queue.  This way the order of
control plane operations can be guaranteed and work can be done
without complicated lock ordering strategies.

## Implementation

The current net80211 implementation uses FreeBSD taskqueues to
provide a place for both net80211 and driver specific state machine
tasks to be serialised into and run.  This replaced the bulk
of per-driver taskqueues for state management.

Each (struct ieee80211com) has an entry (ic_tq) which represents
the state taskqueue.  The FreeBSD implementation of taskqueues
requires the caller create, initialise and add their own task
to the queue.

### net80211 and driver API

net80211 and drivers have the following API:

Initialising tasks currently just uses the FreeBSD taskqueue macros:

 * struct task - the FreeBSD taskqueue work item
 * TASK_INIT() - initialise a task with state pointer and callback

Work is handled via two calls:

 * ieee80211_runtask() - will schedule the given task to run
 * ieee80211_draintask() - will wait for the given task to complete if scheduled

Tasks are run in their order they are scheduled.

In addition, the following functions leverage taskqueues to provide
known good states for certain control plane operations such as
suspend, resume, interface stop, etc:

 * ieee80211_waitfor_parent() - will block the taskqueue and then wait for
   (some) pending work to complete.

Other parts of driver/net80211 code currently calls the taskqueue_*
routines directly on the ic_tq rather than a platform API to
abstract it.

### Why use this versus mutexes and state variables?

net80211 has to handle a variety of state changes from a variety of sources.
Here are some examples:

 * userland - (ioctl calls from hostapd/wpa_supplicant, ifconfig, other tools);
 * timers - eg a BAR timeout causing A-MPDU TX state to be torn out,
   nodes expiring, association / authentication timeouts;
 * transmit errors
 * received frames - plenty of 802.11 state changes based on received
   frames!
 * driver input - the driver / firmware itself may trigger state changes
   due to packet errors, firmware command success/failure, notifications
   about node timeouts, and much more.

These could all be implemented by holding mutexes whilst state changes
occur, but in a lot of cases there may be other mutexes being held inside
net80211, the ioctl layer, the driver stack, the upper layers of the network
stack and .. well, a variety of other places.  This also can lead into
situations where the driver and net80211 end up calling into each other
in circles just to get work done.

Here's an example - notification of an 802.11n channel width via a call to
ieee80211_update_chw().

 * This happens when an 802.11 IE is received which indicates the channel
   width should change.
 * This will end up calling into the 802.11 stack to signal the channel
   width change.
 * It will also need to call back into the driver to potentially change
   the currently configured channel width.

If this were done without a deferred task, the flow would be driver ->
net80211 -> driver (and then potentially -> net80211 again.)

Instead, deferring the work addresses a few things:

 * Any locks held across the driver receive path don't matter here,
   as the serialising is done via the task queue order, not by
   mutexes being held;
 * The work is serialised based on the order of received state changes
   (ie receving packets A, B and C which cause state changes A, B and
   C to be scheduled should result in A, B and C happening in that order);
 * There's no recursion from driver -> net80211 -> driver, or net80211 ->
   driver -> net80211 (except for some macros/utility calls.)

### VAP state change handling / ieee80211_new_state()

The VAP newstate handling is an example of where the current task API
falls short and it would benefit from being more dynamic.

The older net80211 code had a single task for newstate.  Each call
to ieee80211_new_state() to change the VAP state (AUTH, RUN, INIT, etc)
would attempt to update the VAP state via a deferred task.
However if multiple state changes came in quickly, the requested new
state and argument would end up replacing the existing queued one,
and the driver would not see the intermediate state changes.

This is fine for some - eg back to back channel width changes
can be coalesced into one - but others such as the VAP state
machine should not!

This changed in FreeBSD-14 / FreeBSD-15 to leverage an array of
newstates to attempt to deal with this happening.  ieee80211_new_state_locked()
would request a free slot, and then ieee80211_newstate_cb() would
get the next pending state from the list to handle.

### Deferred tasks versus data path and control path

There is no implicit synchronisation between deferred tasks, the control
path and the data path.  Deferred tasks can and will run in parallel with
packet transmit and receive and with ioctl / other control paths.

The goals of task deferral is to serialise these tasks between itself and other
tasks.  This has the side effect of happening outside of all the locking
that may be occuring if it were done inline, but it does not preclude
tasks themselves from having to use locks to serialise with the data/control
paths.

Thus, data path, control path and deferred tasks must still use the
appropriate mutexes to protect any state changes (global, vap, node, etc.)
In most cases it'll be the driver lock (via IEEE80211_LOCK() ), but
it may also involve the node table lock, power save queue lock, etc.

### net80211 driver tasks

The following are a list of tasks which are global to the device and
are defined in struct ieee80211com .

 * ic_parent_task - deferred parent processing
 * ic_promisc_task - deferred promiscuous configuration change processing
 * ic_mcast_task - deferred multicast config/filter processing
 * ic_chan_task - deferred global device channel change
 * ic_bmiss_task - deferred beacon miss handler
 * ic_chw_task - deferred HT channel width (20/40MHz) update processing
 * ic_restart_task - deferred device restart

The following are a list of tasks which are per-VAP and are defined in
struct ieee80211vap .

 * iv_swbmiss_task - deferred per-vap beacon miss processing
 * iv_nstate_task (with iv_nstates, iv_nstate_args) - VAP state transition
   handling tasks
 * iv_wme_task - deferred WME (QoS configuration) update
 * iv_slot_task - deferred slot time update
 * iv_erp_protmode_task - deferred ERP/11g protection mode update
 * iv_preamble_task - deferred 802.11b preamble update
 * iv_ht_protmode_update - deferred 802.11n protection mode update

## Future work

 * There's currently no way to schedule multiple instances of a task
   with some state into the taskqueue.  Eg, the newstate task used to "miss"
   state changes; it now works around this by having an array of newstate task
   entries.

 * The task API is very much a thin wrapper around FreeBSD's taskqueue API.
   It really should become a platform API which is defined and implemented
   in ieee80211_freebsd.[ch].

 * Tasks are fire and forget.  There's currently no way for the submitter
   to be called when thas task runs or is canceled; it can only run and
   then block via calling drain until its called or cancelled.

 * There's currently no way to suspend transmit and receive handling
   around state processing.  This is not always desirable for packet
   performance and latency reasons but there are cases where this
   would be desirable (eg a channel width change would benefit from
   stopping and queueing transmit, waiting for the driver to finish
   transmitting, then change the channel width and then unblock
   transmit to continue.)
