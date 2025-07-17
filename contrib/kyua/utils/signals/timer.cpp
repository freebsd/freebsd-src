// Copyright 2010 The Kyua Authors.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "utils/signals/timer.hpp"

extern "C" {
#include <sys/time.h>

#include <signal.h>
}

#include <cerrno>
#include <map>
#include <set>
#include <vector>

#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/signals/interrupts.hpp"
#include "utils/signals/programmer.hpp"

namespace datetime = utils::datetime;
namespace signals = utils::signals;

using utils::none;
using utils::optional;

namespace {


static void sigalrm_handler(const int);


/// Calls setitimer(2) with exception-based error reporting.
///
/// This does not currently support intervals.
///
/// \param delta The time to the first activation of the programmed timer.
/// \param old_timeval If not NULL, pointer to a timeval into which to store the
///     existing system timer.
///
/// \throw system_error If the call to setitimer(2) fails.
static void
safe_setitimer(const datetime::delta& delta, itimerval* old_timeval)
{
    ::itimerval timeval;
    timerclear(&timeval.it_interval);
    timeval.it_value.tv_sec = delta.seconds;
    timeval.it_value.tv_usec = delta.useconds;

    if (::setitimer(ITIMER_REAL, &timeval, old_timeval) == -1) {
        const int original_errno = errno;
        throw signals::system_error("Failed to program system's interval timer",
                                    original_errno);
    }
}


/// Deadline scheduler for all user timers on top of the unique system timer.
class global_state : utils::noncopyable {
    /// Collection of active timers.
    ///
    /// Because this is a collection of pointers, all timers are guaranteed to
    /// be unique, and we want all of these pointers to be valid.
    typedef std::set< signals::timer* > timers_set;

    /// Sequence of ordered timers.
    typedef std::vector< signals::timer* > timers_vector;

    /// Collection of active timestamps by their activation timestamp.
    ///
    /// This collection is ordered intentionally so that it can be scanned
    /// sequentially to find either expired or expiring-now timers.
    typedef std::map< datetime::timestamp, timers_set > timers_by_timestamp_map;

    /// The original timer before any timer was programmed.
    ::itimerval _old_timeval;

    /// Programmer for the SIGALRM handler.
    std::unique_ptr< signals::programmer > _sigalrm_programmer;

    /// Time of the current activation of the timer.
    datetime::timestamp _timer_activation;

    /// Mapping of all active timers using their timestamp as the key.
    timers_by_timestamp_map _all_timers;

    /// Adds a timer to the _all_timers map.
    ///
    /// \param timer The timer to add.
    void
    add_to_all_timers(signals::timer* timer)
    {
        timers_set& timers = _all_timers[timer->when()];
        INV(timers.find(timer) == timers.end());
        timers.insert(timer);
    }

    /// Removes a timer from the _all_timers map.
    ///
    /// This ensures that empty vectors are removed from _all_timers if the
    /// removal of the timer causes its bucket to be emptied.
    ///
    /// \param timer The timer to remove.
    void
    remove_from_all_timers(signals::timer* timer)
    {
        // We may not find the timer in _all_timers if the timer has fired,
        // because fire() took it out from the map.
        timers_by_timestamp_map::iterator iter = _all_timers.find(
            timer->when());
        if (iter != _all_timers.end()) {
            timers_set& timers = (*iter).second;
            INV(timers.find(timer) != timers.end());
            timers.erase(timer);
            if (timers.empty()) {
                _all_timers.erase(iter);
            }
        }
    }

    /// Calculates all timers to execute at this timestamp.
    ///
    /// \param now The current timestamp.
    ///
    /// \post _all_timers is updated to contain only the timers that are
    /// strictly in the future.
    ///
    /// \return A sequence of valid timers that need to be invoked in the order
    /// of activation.  These are all previously registered timers with
    /// activations in the past.
    timers_vector
    compute_timers_to_run_and_prune_old(
        const datetime::timestamp& now,
        const signals::interrupts_inhibiter& /* inhibiter */)
    {
        timers_vector to_run;

        timers_by_timestamp_map::iterator iter = _all_timers.begin();
        while (iter != _all_timers.end() && (*iter).first <= now) {
            const timers_set& timers = (*iter).second;
            to_run.insert(to_run.end(), timers.begin(), timers.end());

            // Remove expired entries here so that we can always assume that
            // the first entry in all_timers corresponds to the next
            // activation.
            const timers_by_timestamp_map::iterator previous_iter = iter;
            ++iter;
            _all_timers.erase(previous_iter);
        }

        return to_run;
    }

    /// Adjusts the global system timer to point to the next activation.
    ///
    /// \param now The current timestamp.
    ///
    /// \throw system_error If the programming fails.
    void
    reprogram_system_timer(
        const datetime::timestamp& now,
        const signals::interrupts_inhibiter& /* inhibiter */)
    {
        if (_all_timers.empty()) {
            // Nothing to do.  We can reach this case if all the existing timers
            // are in the past and they all fired.  Just ignore the request and
            // leave the global timer as is.
            return;
        }

        // While fire() prunes old entries from the list of timers, it is
        // possible for this routine to run with "expired" timers (i.e. timers
        // whose deadline lies in the past but that have not yet fired for
        // whatever reason that is out of our control) in the list.  We have to
        // iterate until we find the next activation instead of assuming that
        // the first entry represents the desired value.
        timers_by_timestamp_map::const_iterator iter = _all_timers.begin();
        PRE(!(*iter).second.empty());
        datetime::timestamp next = (*iter).first;
        while (next < now) {
            ++iter;
            if (iter == _all_timers.end()) {
                // Nothing to do.  We can reach this case if all the existing
                // timers are in the past but they have not yet fired.
                return;
            }
            PRE(!(*iter).second.empty());
            next = (*iter).first;
        }

        if (next < _timer_activation || now > _timer_activation) {
            INV(next >= now);
            const datetime::delta delta = next - now;
            LD(F("Reprogramming timer; firing on %s; now is %s") % next % now);
            safe_setitimer(delta, NULL);
            _timer_activation = next;
        }
    }

public:
    /// Programs the first timer.
    ///
    /// The programming of the first timer involves setting up the SIGALRM
    /// handler and installing a timer handler for the first time, which in turn
    /// involves keeping track of the old handlers so that we can restore them.
    ///
    /// \param timer The timer being programmed.
    /// \param now The current timestamp.
    ///
    /// \throw system_error If the programming fails.
    global_state(signals::timer* timer, const datetime::timestamp& now) :
        _timer_activation(timer->when())
    {
        PRE(now < timer->when());

        signals::interrupts_inhibiter inhibiter;

        const datetime::delta delta = timer->when() - now;
        LD(F("Installing first timer; firing on %s; now is %s") %
           timer->when() % now);

        _sigalrm_programmer.reset(
            new signals::programmer(SIGALRM, sigalrm_handler));
        try {
            safe_setitimer(delta, &_old_timeval);
            _timer_activation = timer->when();
            add_to_all_timers(timer);
        } catch (...) {
            _sigalrm_programmer.reset();
            throw;
        }
    }

    /// Unprograms all timers.
    ///
    /// This clears the global system timer and unsets the SIGALRM handler.
    ~global_state(void)
    {
        signals::interrupts_inhibiter inhibiter;

        LD("Unprogramming all timers");

        if (::setitimer(ITIMER_REAL, &_old_timeval, NULL) == -1) {
            UNREACHABLE_MSG("Failed to restore original timer");
        }

        _sigalrm_programmer->unprogram();
        _sigalrm_programmer.reset();
    }

    /// Programs a new timer, possibly adjusting the global system timer.
    ///
    /// Programming any timer other than the first one only involves reloading
    /// the existing timer, not backing up the previous handler nor installing a
    /// handler for SIGALRM.
    ///
    /// \param timer The timer being programmed.
    /// \param now The current timestamp.
    ///
    /// \throw system_error If the programming fails.
    void
    program_new(signals::timer* timer, const datetime::timestamp& now)
    {
        signals::interrupts_inhibiter inhibiter;

        add_to_all_timers(timer);
        reprogram_system_timer(now, inhibiter);
    }

    /// Unprograms a timer.
    ///
    /// This removes the timer from the global state and reprograms the global
    /// system timer if necessary.
    ///
    /// \param timer The timer to unprogram.
    ///
    /// \return True if the system interval timer has been reprogrammed to
    /// another future timer; false if there are no more active timers.
    bool
    unprogram(signals::timer* timer)
    {
        signals::interrupts_inhibiter inhibiter;

        LD(F("Unprogramming timer; previously firing on %s") % timer->when());

        remove_from_all_timers(timer);
        if (_all_timers.empty()) {
            return false;
        } else {
            reprogram_system_timer(datetime::timestamp::now(), inhibiter);
            return true;
        }
    }

    /// Executes active timers.
    ///
    /// Active timers are all those that fire on or before 'now'.
    ///
    /// \param now The current time.
    void
    fire(const datetime::timestamp& now)
    {
        timers_vector to_run;
        {
            signals::interrupts_inhibiter inhibiter;
            to_run = compute_timers_to_run_and_prune_old(now, inhibiter);
            reprogram_system_timer(now, inhibiter);
        }

        for (timers_vector::iterator iter = to_run.begin();
             iter != to_run.end(); ++iter) {
            signals::detail::invoke_do_fired(*iter);
        }
    }
};


/// Unique instance of the global state.
static std::unique_ptr< global_state > globals;


/// SIGALRM handler for the timer implementation.
///
/// \param signo The signal received; must be SIGALRM.
static void
sigalrm_handler(const int signo)
{
    PRE(signo == SIGALRM);
    globals->fire(datetime::timestamp::now());
}


}  // anonymous namespace


/// Indirection to invoke the private do_fired() method of a timer.
///
/// \param timer The timer on which to run do_fired().
void
utils::signals::detail::invoke_do_fired(timer* timer)
{
    timer->do_fired();
}


/// Internal implementation for the timer.
///
/// We assume that there is a 1-1 mapping between timer objects and impl
/// objects.  If this assumption breaks, then the rest of the code in this
/// module breaks as well because we use pointers to the parent timer as the
/// identifier of the timer.
struct utils::signals::timer::impl : utils::noncopyable {
    /// Timestamp when this timer is expected to fire.
    ///
    /// Note that the timer might be processed after this timestamp, so users of
    /// this field need to check for timers that fire on or before the
    /// activation time.
    datetime::timestamp when;

    /// True until unprogram() is called.
    bool programmed;

    /// Whether this timer has fired already or not.
    ///
    /// This is updated from an interrupt context, hence why it is marked
    /// volatile.
    volatile bool fired;

    /// Constructor.
    ///
    /// \param when_ Timestamp when this timer is expected to fire.
    impl(const datetime::timestamp& when_) :
        when(when_), programmed(true), fired(false)
    {
    }

    /// Destructor.
    ~impl(void) {
    }
};


/// Constructor; programs a run-once timer.
///
/// This programs the global timer and signal handler if this is the first timer
/// being installed.  Otherwise, reprograms the global timer if this timer
/// expires earlier than all other active timers.
///
/// \param delta The time until the timer fires.
signals::timer::timer(const datetime::delta& delta)
{
    signals::interrupts_inhibiter inhibiter;

    const datetime::timestamp now = datetime::timestamp::now();
    _pimpl.reset(new impl(now + delta));
    if (globals.get() == NULL) {
        globals.reset(new global_state(this, now));
    } else {
        globals->program_new(this, now);
    }
}


/// Destructor; unprograms the timer if still programmed.
///
/// Given that this is a destructor and it can't report errors back to the
/// caller, the caller must attempt to call unprogram() on its own.  This is
/// extremely important because, otherwise, expired timers will never run!
signals::timer::~timer(void)
{
    signals::interrupts_inhibiter inhibiter;

    if (_pimpl->programmed) {
        LW("Auto-destroying still-programmed signals::timer object");
        try {
            unprogram();
        } catch (const system_error& e) {
            UNREACHABLE;
        }
    }

    if (!_pimpl->fired) {
        const datetime::timestamp now = datetime::timestamp::now();
        if (now > _pimpl->when) {
            LW("Expired timer never fired; the code never called unprogram()!");
        }
    }
}


/// Returns the time of the timer activation.
///
/// \return A timestamp that has no relation to the current time (i.e. can be in
/// the future or in the past) nor the timer's activation status.
const datetime::timestamp&
signals::timer::when(void) const
{
    return _pimpl->when;
}


/// Callback for the SIGALRM handler when this timer expires.
///
/// \warning This is executed from a signal handler context without signals
/// inhibited.  See signal(7) for acceptable system calls.
void
signals::timer::do_fired(void)
{
    PRE(!_pimpl->fired);
    _pimpl->fired = true;
    callback();
}


/// User-provided callback to run when the timer expires.
///
/// The default callback does nothing.  We record the activation of the timer
/// separately, which may be appropriate in the majority of the cases.
///
/// \warning This is executed from a signal handler context without signals
/// inhibited.  See signal(7) for acceptable system calls.
void
signals::timer::callback(void)
{
    // Intentionally left blank.
}


/// Checks whether the timer has fired already or not.
///
/// \return Returns true if the timer has fired.
bool
signals::timer::fired(void) const
{
    return _pimpl->fired;
}


/// Unprograms the timer.
///
/// \pre The timer is programmed (i.e. this can only be called once).
///
/// \post If the timer never fired asynchronously because the signal delivery
/// did not arrive on time, make sure we invoke the timer's callback here.
///
/// \throw system_error If unprogramming the timer failed.
void
signals::timer::unprogram(void)
{
    signals::interrupts_inhibiter inhibiter;

    if (!_pimpl->programmed) {
        // We cannot assert that the timer is not programmed because it might
        // have been unprogrammed asynchronously between the time we called
        // unprogram() and the time we reach this.  Simply return in this case.
        LD("Called unprogram on already-unprogrammed timer; possibly just "
           "a race");
        return;
    }

    if (!globals->unprogram(this)) {
        globals.reset();
    }
    _pimpl->programmed = false;

    // Handle the case where the timer has expired before we ever got its
    // corresponding signal.  Do so by invoking its callback now.
    if (!_pimpl->fired) {
        const datetime::timestamp now = datetime::timestamp::now();
        if (now > _pimpl->when) {
            LW(F("Firing expired timer on destruction (was to fire on %s)") %
               _pimpl->when);
            do_fired();
        }
    }
}
