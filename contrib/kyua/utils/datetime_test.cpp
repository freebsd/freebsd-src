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

#include "utils/datetime.hpp"

extern "C" {
#include <time.h>
#include <unistd.h>
}

#include <sstream>
#include <stdexcept>

#include <atf-c++.hpp>

namespace datetime = utils::datetime;


ATF_TEST_CASE_WITHOUT_HEAD(delta__defaults);
ATF_TEST_CASE_BODY(delta__defaults)
{
    const datetime::delta delta;
    ATF_REQUIRE_EQ(0, delta.seconds);
    ATF_REQUIRE_EQ(0, delta.useconds);
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__overrides);
ATF_TEST_CASE_BODY(delta__overrides)
{
    const datetime::delta delta(1, 2);
    ATF_REQUIRE_EQ(1, delta.seconds);
    ATF_REQUIRE_EQ(2, delta.useconds);

    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Negative.*not supported.*-4999997us",
        datetime::delta(-5, 3));
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__from_microseconds);
ATF_TEST_CASE_BODY(delta__from_microseconds)
{
    {
        const datetime::delta delta = datetime::delta::from_microseconds(0);
        ATF_REQUIRE_EQ(0, delta.seconds);
        ATF_REQUIRE_EQ(0, delta.useconds);
    }
    {
        const datetime::delta delta = datetime::delta::from_microseconds(
            999999);
        ATF_REQUIRE_EQ(0, delta.seconds);
        ATF_REQUIRE_EQ(999999, delta.useconds);
    }
    {
        const datetime::delta delta = datetime::delta::from_microseconds(
            1000000);
        ATF_REQUIRE_EQ(1, delta.seconds);
        ATF_REQUIRE_EQ(0, delta.useconds);
    }
    {
        const datetime::delta delta = datetime::delta::from_microseconds(
            10576293);
        ATF_REQUIRE_EQ(10, delta.seconds);
        ATF_REQUIRE_EQ(576293, delta.useconds);
    }
    {
        const datetime::delta delta = datetime::delta::from_microseconds(
            123456789123456LL);
        ATF_REQUIRE_EQ(123456789, delta.seconds);
        ATF_REQUIRE_EQ(123456, delta.useconds);
    }

    ATF_REQUIRE_THROW_RE(
        std::runtime_error, "Negative.*not supported.*-12345us",
        datetime::delta::from_microseconds(-12345));
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__to_microseconds);
ATF_TEST_CASE_BODY(delta__to_microseconds)
{
    ATF_REQUIRE_EQ(0, datetime::delta(0, 0).to_microseconds());
    ATF_REQUIRE_EQ(999999, datetime::delta(0, 999999).to_microseconds());
    ATF_REQUIRE_EQ(1000000, datetime::delta(1, 0).to_microseconds());
    ATF_REQUIRE_EQ(10576293, datetime::delta(10, 576293).to_microseconds());
    ATF_REQUIRE_EQ(11576293, datetime::delta(10, 1576293).to_microseconds());
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__equals);
ATF_TEST_CASE_BODY(delta__equals)
{
    ATF_REQUIRE(datetime::delta() == datetime::delta());
    ATF_REQUIRE(datetime::delta() == datetime::delta(0, 0));
    ATF_REQUIRE(datetime::delta(1, 2) == datetime::delta(1, 2));

    ATF_REQUIRE(!(datetime::delta() == datetime::delta(0, 1)));
    ATF_REQUIRE(!(datetime::delta() == datetime::delta(1, 0)));
    ATF_REQUIRE(!(datetime::delta(1, 2) == datetime::delta(2, 1)));
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__differs);
ATF_TEST_CASE_BODY(delta__differs)
{
    ATF_REQUIRE(!(datetime::delta() != datetime::delta()));
    ATF_REQUIRE(!(datetime::delta() != datetime::delta(0, 0)));
    ATF_REQUIRE(!(datetime::delta(1, 2) != datetime::delta(1, 2)));

    ATF_REQUIRE(datetime::delta() != datetime::delta(0, 1));
    ATF_REQUIRE(datetime::delta() != datetime::delta(1, 0));
    ATF_REQUIRE(datetime::delta(1, 2) != datetime::delta(2, 1));
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__sorting);
ATF_TEST_CASE_BODY(delta__sorting)
{
    ATF_REQUIRE(!(datetime::delta() <  datetime::delta()));
    ATF_REQUIRE(  datetime::delta() <= datetime::delta());
    ATF_REQUIRE(!(datetime::delta() >  datetime::delta()));
    ATF_REQUIRE(  datetime::delta() >= datetime::delta());

    ATF_REQUIRE(!(datetime::delta(9, 8) <  datetime::delta(9, 8)));
    ATF_REQUIRE(  datetime::delta(9, 8) <= datetime::delta(9, 8));
    ATF_REQUIRE(!(datetime::delta(9, 8) >  datetime::delta(9, 8)));
    ATF_REQUIRE(  datetime::delta(9, 8) >= datetime::delta(9, 8));

    ATF_REQUIRE(  datetime::delta(2, 5) <  datetime::delta(4, 8));
    ATF_REQUIRE(  datetime::delta(2, 5) <= datetime::delta(4, 8));
    ATF_REQUIRE(!(datetime::delta(2, 5) >  datetime::delta(4, 8)));
    ATF_REQUIRE(!(datetime::delta(2, 5) >= datetime::delta(4, 8)));

    ATF_REQUIRE(  datetime::delta(2, 5) <  datetime::delta(2, 8));
    ATF_REQUIRE(  datetime::delta(2, 5) <= datetime::delta(2, 8));
    ATF_REQUIRE(!(datetime::delta(2, 5) >  datetime::delta(2, 8)));
    ATF_REQUIRE(!(datetime::delta(2, 5) >= datetime::delta(2, 8)));

    ATF_REQUIRE(!(datetime::delta(4, 8) <  datetime::delta(2, 5)));
    ATF_REQUIRE(!(datetime::delta(4, 8) <= datetime::delta(2, 5)));
    ATF_REQUIRE(  datetime::delta(4, 8) >  datetime::delta(2, 5));
    ATF_REQUIRE(  datetime::delta(4, 8) >= datetime::delta(2, 5));

    ATF_REQUIRE(!(datetime::delta(2, 8) <  datetime::delta(2, 5)));
    ATF_REQUIRE(!(datetime::delta(2, 8) <= datetime::delta(2, 5)));
    ATF_REQUIRE(  datetime::delta(2, 8) >  datetime::delta(2, 5));
    ATF_REQUIRE(  datetime::delta(2, 8) >= datetime::delta(2, 5));
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__addition);
ATF_TEST_CASE_BODY(delta__addition)
{
    using datetime::delta;

    ATF_REQUIRE_EQ(delta(), delta() + delta());
    ATF_REQUIRE_EQ(delta(0, 10), delta() + delta(0, 10));
    ATF_REQUIRE_EQ(delta(10, 0), delta(10, 0) + delta());

    ATF_REQUIRE_EQ(delta(1, 234567), delta(0, 1234567) + delta());
    ATF_REQUIRE_EQ(delta(12, 34), delta(10, 20) + delta(2, 14));
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__addition_and_set);
ATF_TEST_CASE_BODY(delta__addition_and_set)
{
    using datetime::delta;

    {
        delta d;
        d += delta(3, 5);
        ATF_REQUIRE_EQ(delta(3, 5), d);
    }
    {
        delta d(1, 2);
        d += delta(3, 5);
        ATF_REQUIRE_EQ(delta(4, 7), d);
    }
    {
        delta d(1, 2);
        ATF_REQUIRE_EQ(delta(4, 7), (d += delta(3, 5)));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__scale);
ATF_TEST_CASE_BODY(delta__scale)
{
    using datetime::delta;

    ATF_REQUIRE_EQ(delta(), delta() * 0);
    ATF_REQUIRE_EQ(delta(), delta() * 5);

    ATF_REQUIRE_EQ(delta(0, 30), delta(0, 10) * 3);
    ATF_REQUIRE_EQ(delta(17, 500000), delta(3, 500000) * 5);
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__scale_and_set);
ATF_TEST_CASE_BODY(delta__scale_and_set)
{
    using datetime::delta;

    {
        delta d(3, 5);
        d *= 2;
        ATF_REQUIRE_EQ(delta(6, 10), d);
    }
    {
        delta d(8, 0);
        d *= 8;
        ATF_REQUIRE_EQ(delta(64, 0), d);
    }
    {
        delta d(3, 5);
        ATF_REQUIRE_EQ(delta(9, 15), (d *= 3));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__output);
ATF_TEST_CASE_BODY(delta__output)
{
    {
        std::ostringstream str;
        str << datetime::delta(15, 8791);
        ATF_REQUIRE_EQ("15008791us", str.str());
    }
    {
        std::ostringstream str;
        str << datetime::delta(12345678, 0);
        ATF_REQUIRE_EQ("12345678000000us", str.str());
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__copy);
ATF_TEST_CASE_BODY(timestamp__copy)
{
    const datetime::timestamp ts1 = datetime::timestamp::from_values(
        2011, 2, 16, 19, 15, 30, 0);
    {
        const datetime::timestamp ts2 = ts1;
        const datetime::timestamp ts3 = datetime::timestamp::from_values(
            2012, 2, 16, 19, 15, 30, 0);
        ATF_REQUIRE_EQ("2011", ts1.strftime("%Y"));
        ATF_REQUIRE_EQ("2011", ts2.strftime("%Y"));
        ATF_REQUIRE_EQ("2012", ts3.strftime("%Y"));
    }
    ATF_REQUIRE_EQ("2011", ts1.strftime("%Y"));
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__from_microseconds);
ATF_TEST_CASE_BODY(timestamp__from_microseconds)
{
    const datetime::timestamp ts = datetime::timestamp::from_microseconds(
        1328829351987654LL);
    ATF_REQUIRE_EQ("2012-02-09 23:15:51", ts.strftime("%Y-%m-%d %H:%M:%S"));
    ATF_REQUIRE_EQ(1328829351987654LL, ts.to_microseconds());
    ATF_REQUIRE_EQ(1328829351, ts.to_seconds());
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__now__mock);
ATF_TEST_CASE_BODY(timestamp__now__mock)
{
    datetime::set_mock_now(2011, 2, 21, 18, 5, 10, 0);
    ATF_REQUIRE_EQ("2011-02-21 18:05:10",
                   datetime::timestamp::now().strftime("%Y-%m-%d %H:%M:%S"));

    datetime::set_mock_now(datetime::timestamp::from_values(
                               2012, 3, 22, 19, 6, 11, 54321));
    ATF_REQUIRE_EQ("2012-03-22 19:06:11",
                   datetime::timestamp::now().strftime("%Y-%m-%d %H:%M:%S"));
    ATF_REQUIRE_EQ("2012-03-22 19:06:11",
                   datetime::timestamp::now().strftime("%Y-%m-%d %H:%M:%S"));
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__now__real);
ATF_TEST_CASE_BODY(timestamp__now__real)
{
    // This test is might fail if we happen to run at the crossing of one
    // day to the other and the two measures we pick of the current time
    // differ.  This is so unlikely that I haven't bothered to do this in any
    // other way.

    const time_t just_before = ::time(NULL);
    const datetime::timestamp now = datetime::timestamp::now();

    ::tm data;
    char buf[1024];
    ATF_REQUIRE(::gmtime_r(&just_before, &data) != 0);
    ATF_REQUIRE(::strftime(buf, sizeof(buf), "%Y-%m-%d", &data) != 0);
    ATF_REQUIRE_EQ(buf, now.strftime("%Y-%m-%d"));

    ATF_REQUIRE(now.strftime("%Z") == "GMT" || now.strftime("%Z") == "UTC");
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__now__granularity);
ATF_TEST_CASE_BODY(timestamp__now__granularity)
{
    const datetime::timestamp first = datetime::timestamp::now();
    ::usleep(1);
    const datetime::timestamp second = datetime::timestamp::now();
    ATF_REQUIRE(first.to_microseconds() != second.to_microseconds());
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__strftime);
ATF_TEST_CASE_BODY(timestamp__strftime)
{
    const datetime::timestamp ts1 = datetime::timestamp::from_values(
        2010, 12, 10, 8, 45, 50, 0);
    ATF_REQUIRE_EQ("2010-12-10", ts1.strftime("%Y-%m-%d"));
    ATF_REQUIRE_EQ("08:45:50", ts1.strftime("%H:%M:%S"));

    const datetime::timestamp ts2 = datetime::timestamp::from_values(
        2011, 2, 16, 19, 15, 30, 0);
    ATF_REQUIRE_EQ("2011-02-16T19:15:30", ts2.strftime("%Y-%m-%dT%H:%M:%S"));
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__to_iso8601_in_utc);
ATF_TEST_CASE_BODY(timestamp__to_iso8601_in_utc)
{
    const datetime::timestamp ts1 = datetime::timestamp::from_values(
        2010, 12, 10, 8, 45, 50, 0);
    ATF_REQUIRE_EQ("2010-12-10T08:45:50.000000Z", ts1.to_iso8601_in_utc());

    const datetime::timestamp ts2= datetime::timestamp::from_values(
        2016, 7, 11, 17, 51, 28, 123456);
    ATF_REQUIRE_EQ("2016-07-11T17:51:28.123456Z", ts2.to_iso8601_in_utc());
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__to_microseconds);
ATF_TEST_CASE_BODY(timestamp__to_microseconds)
{
    const datetime::timestamp ts1 = datetime::timestamp::from_values(
        2010, 12, 10, 8, 45, 50, 123456);
    ATF_REQUIRE_EQ(1291970750123456LL, ts1.to_microseconds());
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__to_seconds);
ATF_TEST_CASE_BODY(timestamp__to_seconds)
{
    const datetime::timestamp ts1 = datetime::timestamp::from_values(
        2010, 12, 10, 8, 45, 50, 123456);
    ATF_REQUIRE_EQ(1291970750, ts1.to_seconds());
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__leap_second);
ATF_TEST_CASE_BODY(timestamp__leap_second)
{
    // This is actually a test for from_values(), which is the function that
    // includes assertions to validate the input parameters.
    const datetime::timestamp ts1 = datetime::timestamp::from_values(
        2012, 6, 30, 23, 59, 60, 543);
    ATF_REQUIRE_EQ(1341100800, ts1.to_seconds());
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__equals);
ATF_TEST_CASE_BODY(timestamp__equals)
{
    ATF_REQUIRE(datetime::timestamp::from_microseconds(1291970750123456LL) ==
                datetime::timestamp::from_microseconds(1291970750123456LL));
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__differs);
ATF_TEST_CASE_BODY(timestamp__differs)
{
    ATF_REQUIRE(datetime::timestamp::from_microseconds(1291970750123456LL) !=
                datetime::timestamp::from_microseconds(1291970750123455LL));
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__sorting);
ATF_TEST_CASE_BODY(timestamp__sorting)
{
    {
        const datetime::timestamp ts1 = datetime::timestamp::from_microseconds(
            1291970750123455LL);
        const datetime::timestamp ts2 = datetime::timestamp::from_microseconds(
            1291970750123455LL);

        ATF_REQUIRE(!(ts1 < ts2));
        ATF_REQUIRE(  ts1 <= ts2);
        ATF_REQUIRE(!(ts1 > ts2));
        ATF_REQUIRE(  ts1 >= ts2);
    }
    {
        const datetime::timestamp ts1 = datetime::timestamp::from_microseconds(
            1291970750123455LL);
        const datetime::timestamp ts2 = datetime::timestamp::from_microseconds(
            1291970759123455LL);

        ATF_REQUIRE( ts1 < ts2);
        ATF_REQUIRE( ts1 <= ts2);
        ATF_REQUIRE(!(ts1 > ts2));
        ATF_REQUIRE(!(ts1 >= ts2));
    }
    {
        const datetime::timestamp ts1 = datetime::timestamp::from_microseconds(
            1291970759123455LL);
        const datetime::timestamp ts2 = datetime::timestamp::from_microseconds(
            1291970750123455LL);

        ATF_REQUIRE(!(ts1 < ts2));
        ATF_REQUIRE(!(ts1 <= ts2));
        ATF_REQUIRE(  ts1 > ts2);
        ATF_REQUIRE(  ts1 >= ts2);
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__add_delta);
ATF_TEST_CASE_BODY(timestamp__add_delta)
{
    using datetime::delta;
    using datetime::timestamp;

    ATF_REQUIRE_EQ(timestamp::from_values(2014, 12, 11, 21, 43, 30, 1234),
                   timestamp::from_values(2014, 12, 11, 21, 43, 0, 0) +
                   delta(30, 1234));
    ATF_REQUIRE_EQ(timestamp::from_values(2014, 12, 11, 22, 43, 7, 100),
                   timestamp::from_values(2014, 12, 11, 21, 43, 0, 0) +
                   delta(3602, 5000100));
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__add_delta_and_set);
ATF_TEST_CASE_BODY(timestamp__add_delta_and_set)
{
    using datetime::delta;
    using datetime::timestamp;

    {
        timestamp ts = timestamp::from_values(2014, 12, 11, 21, 43, 0, 0);
        ts += delta(30, 1234);
        ATF_REQUIRE_EQ(timestamp::from_values(2014, 12, 11, 21, 43, 30, 1234),
                       ts);
    }
    {
        timestamp ts = timestamp::from_values(2014, 12, 11, 21, 43, 0, 0);
        ATF_REQUIRE_EQ(timestamp::from_values(2014, 12, 11, 22, 43, 7, 100),
                       ts += delta(3602, 5000100));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__subtract_delta);
ATF_TEST_CASE_BODY(timestamp__subtract_delta)
{
    using datetime::delta;
    using datetime::timestamp;

    ATF_REQUIRE_EQ(timestamp::from_values(2014, 12, 11, 21, 43, 10, 4321),
                   timestamp::from_values(2014, 12, 11, 21, 43, 40, 5555) -
                   delta(30, 1234));
    ATF_REQUIRE_EQ(timestamp::from_values(2014, 12, 11, 20, 43, 1, 300),
                   timestamp::from_values(2014, 12, 11, 21, 43, 8, 400) -
                   delta(3602, 5000100));
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__subtract_delta_and_set);
ATF_TEST_CASE_BODY(timestamp__subtract_delta_and_set)
{
    using datetime::delta;
    using datetime::timestamp;

    {
        timestamp ts = timestamp::from_values(2014, 12, 11, 21, 43, 40, 5555);
        ts -= delta(30, 1234);
        ATF_REQUIRE_EQ(timestamp::from_values(2014, 12, 11, 21, 43, 10, 4321),
                       ts);
    }
    {
        timestamp ts = timestamp::from_values(2014, 12, 11, 21, 43, 8, 400);
        ATF_REQUIRE_EQ(timestamp::from_values(2014, 12, 11, 20, 43, 1, 300),
                       ts -= delta(3602, 5000100));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__subtraction);
ATF_TEST_CASE_BODY(timestamp__subtraction)
{
    const datetime::timestamp ts1 = datetime::timestamp::from_microseconds(
        1291970750123456LL);
    const datetime::timestamp ts2 = datetime::timestamp::from_microseconds(
        1291970750123468LL);
    const datetime::timestamp ts3 = datetime::timestamp::from_microseconds(
        1291970850123456LL);

    ATF_REQUIRE_EQ(datetime::delta(0, 0), ts1 - ts1);
    ATF_REQUIRE_EQ(datetime::delta(0, 12), ts2 - ts1);
    ATF_REQUIRE_EQ(datetime::delta(100, 0), ts3 - ts1);
    ATF_REQUIRE_EQ(datetime::delta(99, 999988), ts3 - ts2);

    /*
     * NOTE (ngie): behavior change for
     * https://github.com/jmmv/kyua/issues/155 .
     */
    ATF_REQUIRE_EQ(datetime::delta::from_microseconds(1), ts2 - ts3);
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__output);
ATF_TEST_CASE_BODY(timestamp__output)
{
    {
        std::ostringstream str;
        str << datetime::timestamp::from_microseconds(1291970750123456LL);
        ATF_REQUIRE_EQ("1291970750123456us", str.str());
    }
    {
        std::ostringstream str;
        str << datetime::timestamp::from_microseconds(1028309798759812LL);
        ATF_REQUIRE_EQ("1028309798759812us", str.str());
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, delta__defaults);
    ATF_ADD_TEST_CASE(tcs, delta__overrides);
    ATF_ADD_TEST_CASE(tcs, delta__from_microseconds);
    ATF_ADD_TEST_CASE(tcs, delta__to_microseconds);
    ATF_ADD_TEST_CASE(tcs, delta__equals);
    ATF_ADD_TEST_CASE(tcs, delta__differs);
    ATF_ADD_TEST_CASE(tcs, delta__sorting);
    ATF_ADD_TEST_CASE(tcs, delta__addition);
    ATF_ADD_TEST_CASE(tcs, delta__addition_and_set);
    ATF_ADD_TEST_CASE(tcs, delta__scale);
    ATF_ADD_TEST_CASE(tcs, delta__scale_and_set);
    ATF_ADD_TEST_CASE(tcs, delta__output);

    ATF_ADD_TEST_CASE(tcs, timestamp__copy);
    ATF_ADD_TEST_CASE(tcs, timestamp__from_microseconds);
    ATF_ADD_TEST_CASE(tcs, timestamp__now__mock);
    ATF_ADD_TEST_CASE(tcs, timestamp__now__real);
    ATF_ADD_TEST_CASE(tcs, timestamp__now__granularity);
    ATF_ADD_TEST_CASE(tcs, timestamp__strftime);
    ATF_ADD_TEST_CASE(tcs, timestamp__to_iso8601_in_utc);
    ATF_ADD_TEST_CASE(tcs, timestamp__to_microseconds);
    ATF_ADD_TEST_CASE(tcs, timestamp__to_seconds);
    ATF_ADD_TEST_CASE(tcs, timestamp__leap_second);
    ATF_ADD_TEST_CASE(tcs, timestamp__equals);
    ATF_ADD_TEST_CASE(tcs, timestamp__differs);
    ATF_ADD_TEST_CASE(tcs, timestamp__sorting);
    ATF_ADD_TEST_CASE(tcs, timestamp__add_delta);
    ATF_ADD_TEST_CASE(tcs, timestamp__add_delta_and_set);
    ATF_ADD_TEST_CASE(tcs, timestamp__subtract_delta);
    ATF_ADD_TEST_CASE(tcs, timestamp__subtract_delta_and_set);
    ATF_ADD_TEST_CASE(tcs, timestamp__subtraction);
    ATF_ADD_TEST_CASE(tcs, timestamp__output);
}
