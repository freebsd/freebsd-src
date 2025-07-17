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
#include <signal.h>
#include <unistd.h>
}

#include <cstddef>
#include <iostream>
#include <vector>

#include <atf-c++.hpp>

#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/containers.ipp"
#include "utils/format/macros.hpp"
#include "utils/signals/interrupts.hpp"
#include "utils/signals/programmer.hpp"

namespace datetime = utils::datetime;
namespace signals = utils::signals;


namespace {


/// A timer that inserts an element into a vector on activation.
class delayed_inserter : public signals::timer {
    /// Vector into which to insert the element.
    std::vector< int >& _destination;

    /// Element to insert into _destination on activation.
    const int _item;

    /// Timer activation callback.
    void
    callback(void)
    {
        signals::interrupts_inhibiter inhibiter;
        _destination.push_back(_item);
    }

public:
    /// Constructor.
    ///
    /// \param delta Time to the timer activation.
    /// \param destination Vector into which to insert the element.
    /// \param item Element to insert into destination on activation.
    delayed_inserter(const datetime::delta& delta,
                     std::vector< int >& destination, const int item) :
        signals::timer(delta), _destination(destination), _item(item)
    {
    }
};


/// Signal handler that does nothing.
static void
null_handler(const int /* signo */)
{
}


/// Waits for the activation of all given timers.
///
/// \param timers Pointers to all the timers to wait for.
static void
wait_timers(const std::vector< signals::timer* >& timers)
{
    std::size_t n_fired, old_n_fired = 0;
    do {
        n_fired = 0;
        for (std::vector< signals::timer* >::const_iterator
                 iter = timers.begin(); iter != timers.end(); ++iter) {
            const signals::timer* timer = *iter;
            if (timer->fired())
                ++n_fired;
        }
        if (old_n_fired < n_fired) {
            std::cout << "Waiting; " << n_fired << " timers fired so far\n";
            old_n_fired = n_fired;
        }
        ::usleep(100);
    } while (n_fired < timers.size());
}


}  // anonymous namespace


ATF_TEST_CASE(program_seconds);
ATF_TEST_CASE_HEAD(program_seconds)
{
    set_md_var("timeout", "10");
}
ATF_TEST_CASE_BODY(program_seconds)
{
    signals::timer timer(datetime::delta(1, 0));
    ATF_REQUIRE(!timer.fired());
    while (!timer.fired())
        ::usleep(1000);
}


ATF_TEST_CASE(program_useconds);
ATF_TEST_CASE_HEAD(program_useconds)
{
    set_md_var("timeout", "10");
}
ATF_TEST_CASE_BODY(program_useconds)
{
    signals::timer timer(datetime::delta(0, 500000));
    ATF_REQUIRE(!timer.fired());
    while (!timer.fired())
        ::usleep(1000);
}


ATF_TEST_CASE(multiprogram_ordered);
ATF_TEST_CASE_HEAD(multiprogram_ordered)
{
    set_md_var("timeout", "20");
}
ATF_TEST_CASE_BODY(multiprogram_ordered)
{
    static const std::size_t n_timers = 100;

    std::vector< signals::timer* > timers;
    std::vector< int > items, exp_items;

    const int initial_delay_ms = 1000000;
    for (std::size_t i = 0; i < n_timers; ++i) {
        exp_items.push_back(i);

        timers.push_back(new delayed_inserter(
            datetime::delta(0, initial_delay_ms + (i + 1) * 10000),
            items, i));
        ATF_REQUIRE(!timers[i]->fired());
    }

    wait_timers(timers);

    ATF_REQUIRE_EQ(exp_items, items);
}


ATF_TEST_CASE(multiprogram_reorder_next_activations);
ATF_TEST_CASE_HEAD(multiprogram_reorder_next_activations)
{
    set_md_var("timeout", "20");
}
ATF_TEST_CASE_BODY(multiprogram_reorder_next_activations)
{
    std::vector< signals::timer* > timers;
    std::vector< int > items;

    // First timer with an activation in the future.
    timers.push_back(new delayed_inserter(
                         datetime::delta(0, 100000), items, 1));
    ATF_REQUIRE(!timers[timers.size() - 1]->fired());

    // Timer with an activation earlier than the previous one.
    timers.push_back(new delayed_inserter(
                         datetime::delta(0, 50000), items, 2));
    ATF_REQUIRE(!timers[timers.size() - 1]->fired());

    // Timer with an activation later than all others.
    timers.push_back(new delayed_inserter(
                         datetime::delta(0, 200000), items, 3));
    ATF_REQUIRE(!timers[timers.size() - 1]->fired());

    // Timer with an activation in between.
    timers.push_back(new delayed_inserter(
                         datetime::delta(0, 150000), items, 4));
    ATF_REQUIRE(!timers[timers.size() - 1]->fired());

    wait_timers(timers);

    std::vector< int > exp_items;
    exp_items.push_back(2);
    exp_items.push_back(1);
    exp_items.push_back(4);
    exp_items.push_back(3);
    ATF_REQUIRE_EQ(exp_items, items);
}


ATF_TEST_CASE(multiprogram_and_cancel_some);
ATF_TEST_CASE_HEAD(multiprogram_and_cancel_some)
{
    set_md_var("timeout", "20");
}
ATF_TEST_CASE_BODY(multiprogram_and_cancel_some)
{
    std::vector< signals::timer* > timers;
    std::vector< int > items;

    // First timer with an activation in the future.
    timers.push_back(new delayed_inserter(
                         datetime::delta(0, 100000), items, 1));

    // Timer with an activation earlier than the previous one.
    timers.push_back(new delayed_inserter(
                         datetime::delta(0, 50000), items, 2));

    // Timer with an activation later than all others.
    timers.push_back(new delayed_inserter(
                         datetime::delta(0, 200000), items, 3));

    // Timer with an activation in between.
    timers.push_back(new delayed_inserter(
                         datetime::delta(0, 150000), items, 4));

    // Cancel the first timer to reprogram next activation.
    timers[1]->unprogram(); delete timers[1]; timers.erase(timers.begin() + 1);

    // Cancel another timer without reprogramming next activation.
    timers[2]->unprogram(); delete timers[2]; timers.erase(timers.begin() + 2);

    wait_timers(timers);

    std::vector< int > exp_items;
    exp_items.push_back(1);
    exp_items.push_back(3);
    ATF_REQUIRE_EQ(exp_items, items);
}


ATF_TEST_CASE(multiprogram_and_expire_before_activations);
ATF_TEST_CASE_HEAD(multiprogram_and_expire_before_activations)
{
    set_md_var("timeout", "20");
}
ATF_TEST_CASE_BODY(multiprogram_and_expire_before_activations)
{
    std::vector< signals::timer* > timers;
    std::vector< int > items;

    {
        signals::interrupts_inhibiter inhibiter;

        // First timer with an activation in the future.
        timers.push_back(new delayed_inserter(
                             datetime::delta(0, 100000), items, 1));
        ATF_REQUIRE(!timers[timers.size() - 1]->fired());

        // Timer with an activation earlier than the previous one.
        timers.push_back(new delayed_inserter(
                             datetime::delta(0, 50000), items, 2));
        ATF_REQUIRE(!timers[timers.size() - 1]->fired());

        ::sleep(1);

        // Timer with an activation later than all others.
        timers.push_back(new delayed_inserter(
                             datetime::delta(0, 200000), items, 3));

        ::sleep(1);
    }

    wait_timers(timers);

    std::vector< int > exp_items;
    exp_items.push_back(2);
    exp_items.push_back(1);
    exp_items.push_back(3);
    ATF_REQUIRE_EQ(exp_items, items);
}


ATF_TEST_CASE(expire_before_firing);
ATF_TEST_CASE_HEAD(expire_before_firing)
{
    set_md_var("timeout", "20");
}
ATF_TEST_CASE_BODY(expire_before_firing)
{
    std::vector< int > items;

    // The code below causes a signal to go pending.  Make sure we ignore it
    // when we unblock signals.
    signals::programmer sigalrm(SIGALRM, null_handler);

    {
        signals::interrupts_inhibiter inhibiter;

        delayed_inserter* timer = new delayed_inserter(
            datetime::delta(0, 1000), items, 1234);
        ::sleep(1);
        // Interrupts are inhibited so we never got a chance to execute the
        // timer before it was destroyed.  However, the handler should run
        // regardless at some point, possibly during deletion.
        timer->unprogram();
        delete timer;
    }

    std::vector< int > exp_items;
    exp_items.push_back(1234);
    ATF_REQUIRE_EQ(exp_items, items);
}


ATF_TEST_CASE(reprogram_from_scratch);
ATF_TEST_CASE_HEAD(reprogram_from_scratch)
{
    set_md_var("timeout", "20");
}
ATF_TEST_CASE_BODY(reprogram_from_scratch)
{
    std::vector< int > items;

    delayed_inserter* timer1 = new delayed_inserter(
        datetime::delta(0, 100000), items, 1);
    timer1->unprogram(); delete timer1;

    // All constructed timers are now dead, so the interval timer should have
    // been reprogrammed.  Let's start over.

    delayed_inserter* timer2 = new delayed_inserter(
        datetime::delta(0, 200000), items, 2);
    while (!timer2->fired())
        ::usleep(1000);
    timer2->unprogram(); delete timer2;

    std::vector< int > exp_items;
    exp_items.push_back(2);
    ATF_REQUIRE_EQ(exp_items, items);
}


ATF_TEST_CASE(unprogram);
ATF_TEST_CASE_HEAD(unprogram)
{
    set_md_var("timeout", "10");
}
ATF_TEST_CASE_BODY(unprogram)
{
    signals::timer timer(datetime::delta(0, 500000));
    timer.unprogram();
    usleep(500000);
    ATF_REQUIRE(!timer.fired());
}


ATF_TEST_CASE(infinitesimal);
ATF_TEST_CASE_HEAD(infinitesimal)
{
    set_md_var("descr", "Ensure that the ordering in which the signal, the "
               "timer and the global state are programmed is correct; do so "
               "by setting an extremely small delay for the timer hoping that "
               "it can trigger such conditions");
    set_md_var("timeout", "10");
}
ATF_TEST_CASE_BODY(infinitesimal)
{
    const std::size_t rounds = 100;
    const std::size_t exp_good = 90;

    std::size_t good = 0;
    for (std::size_t i = 0; i < rounds; i++) {
        signals::timer timer(datetime::delta(0, 1));

        // From the setitimer(2) documentation:
        //
        //     Time values smaller than the resolution of the system clock are
        //     rounded up to this resolution (typically 10 milliseconds).
        //
        // We don't know what this resolution is but we must wait for longer
        // than we programmed; do a rough guess and hope it is good.  This may
        // be obviously wrong and thus lead to mysterious test failures in some
        // systems, hence why we only expect a percentage of successes below.
        // Still, we can fail...
        ::usleep(1000);

        if (timer.fired())
            ++good;
        timer.unprogram();
    }
    std::cout << F("Ran %s tests, %s passed; threshold is %s\n")
        % rounds % good % exp_good;
    ATF_REQUIRE(good >= exp_good);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, program_seconds);
    ATF_ADD_TEST_CASE(tcs, program_useconds);
    ATF_ADD_TEST_CASE(tcs, multiprogram_ordered);
    ATF_ADD_TEST_CASE(tcs, multiprogram_reorder_next_activations);
    ATF_ADD_TEST_CASE(tcs, multiprogram_and_cancel_some);
    ATF_ADD_TEST_CASE(tcs, multiprogram_and_expire_before_activations);
    ATF_ADD_TEST_CASE(tcs, expire_before_firing);
    ATF_ADD_TEST_CASE(tcs, reprogram_from_scratch);
    ATF_ADD_TEST_CASE(tcs, unprogram);
    ATF_ADD_TEST_CASE(tcs, infinitesimal);
}
