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
#include <sys/time.h>

#include <time.h>
}

#include <stdexcept>

#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"

namespace datetime = utils::datetime;

using utils::none;
using utils::optional;


namespace {


/// Fake value for the current time.
static optional< datetime::timestamp > mock_now = none;


}  // anonymous namespace


/// Creates a zero time delta.
datetime::delta::delta(void) :
    seconds(0),
    useconds(0)
{
}


/// Creates a time delta.
///
/// \param seconds_ The seconds in the delta.
/// \param useconds_ The microseconds in the delta.
///
/// \throw std::runtime_error If the input delta is negative.
datetime::delta::delta(const int64_t seconds_,
                       const unsigned long useconds_) :
    seconds(seconds_),
    useconds(useconds_)
{
    if (seconds_ < 0) {
        throw std::runtime_error(F("Negative deltas are not supported by the "
                                   "datetime::delta class; got: %s") % (*this));
    }
}


/// Converts a time expressed in microseconds to a delta.
///
/// \param useconds The amount of microseconds representing the delta.
///
/// \return A new delta object.
///
/// \throw std::runtime_error If the input delta is negative.
datetime::delta
datetime::delta::from_microseconds(const int64_t useconds)
{
    if (useconds < 0) {
        throw std::runtime_error(F("Negative deltas are not supported by the "
                                   "datetime::delta class; got: %sus") %
                                 useconds);
    }

    return delta(useconds / 1000000, useconds % 1000000);
}


/// Convers the delta to a flat representation expressed in microseconds.
///
/// \return The amount of microseconds that corresponds to this delta.
int64_t
datetime::delta::to_microseconds(void) const
{
    return seconds * 1000000 + useconds;
}


/// Checks if two time deltas are equal.
///
/// \param other The object to compare to.
///
/// \return True if the two time deltas are equals; false otherwise.
bool
datetime::delta::operator==(const datetime::delta& other) const
{
    return seconds == other.seconds && useconds == other.useconds;
}


/// Checks if two time deltas are different.
///
/// \param other The object to compare to.
///
/// \return True if the two time deltas are different; false otherwise.
bool
datetime::delta::operator!=(const datetime::delta& other) const
{
    return !(*this == other);
}


/// Checks if this time delta is shorter than another one.
///
/// \param other The object to compare to.
///
/// \return True if this time delta is shorter than other; false otherwise.
bool
datetime::delta::operator<(const datetime::delta& other) const
{
    return seconds < other.seconds ||
        (seconds == other.seconds && useconds < other.useconds);
}


/// Checks if this time delta is shorter than or equal to another one.
///
/// \param other The object to compare to.
///
/// \return True if this time delta is shorter than or equal to; false
/// otherwise.
bool
datetime::delta::operator<=(const datetime::delta& other) const
{
    return (*this) < other || (*this) == other;
}


/// Checks if this time delta is larger than another one.
///
/// \param other The object to compare to.
///
/// \return True if this time delta is larger than other; false otherwise.
bool
datetime::delta::operator>(const datetime::delta& other) const
{
    return seconds > other.seconds ||
        (seconds == other.seconds && useconds > other.useconds);
}


/// Checks if this time delta is larger than or equal to another one.
///
/// \param other The object to compare to.
///
/// \return True if this time delta is larger than or equal to; false
/// otherwise.
bool
datetime::delta::operator>=(const datetime::delta& other) const
{
    return (*this) > other || (*this) == other;
}


/// Adds a time delta to this one.
///
/// \param other The time delta to add.
///
/// \return The addition of this time delta with the other time delta.
datetime::delta
datetime::delta::operator+(const datetime::delta& other) const
{
    return delta::from_microseconds(to_microseconds() +
                                    other.to_microseconds());
}


/// Adds a time delta to this one and updates this with the result.
///
/// \param other The time delta to add.
///
/// \return The addition of this time delta with the other time delta.
datetime::delta&
datetime::delta::operator+=(const datetime::delta& other)
{
    *this = *this + other;
    return *this;
}


/// Scales this delta by a positive integral factor.
///
/// \param factor The scaling factor.
///
/// \return The scaled delta.
datetime::delta
datetime::delta::operator*(const std::size_t factor) const
{
    return delta::from_microseconds(to_microseconds() * factor);
}


/// Scales this delta by and updates this delta with the result.
///
/// \param factor The scaling factor.
///
/// \return The scaled delta as a reference to the input object.
datetime::delta&
datetime::delta::operator*=(const std::size_t factor)
{
    *this = *this * factor;
    return *this;
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
std::ostream&
datetime::operator<<(std::ostream& output, const delta& object)
{
    return (output << object.to_microseconds() << "us");
}


namespace utils {
namespace datetime {


/// Internal representation for datetime::timestamp.
struct timestamp::impl : utils::noncopyable {
    /// The raw timestamp as provided by libc.
    ::timeval data;

    /// Constructs an impl object from initialized data.
    ///
    /// \param data_ The raw timestamp to use.
    impl(const ::timeval& data_) : data(data_)
    {
    }
};


}  // namespace datetime
}  // namespace utils


/// Constructs a new timestamp.
///
/// \param pimpl_ An existing impl representation.
datetime::timestamp::timestamp(std::shared_ptr< impl > pimpl_) :
    _pimpl(pimpl_)
{
}


/// Constructs a timestamp from the amount of microseconds since the epoch.
///
/// \param value Microseconds since the epoch in UTC.  Must be positive.
///
/// \return A new timestamp.
datetime::timestamp
datetime::timestamp::from_microseconds(const int64_t value)
{
    PRE(value >= 0);
    ::timeval data;
    data.tv_sec = static_cast< time_t >(value / 1000000);
    data.tv_usec = static_cast< suseconds_t >(value % 1000000);
    return timestamp(std::shared_ptr< impl >(new impl(data)));
}


/// Constructs a timestamp based on user-friendly values.
///
/// \param year The year in the [1900,inf) range.
/// \param month The month in the [1,12] range.
/// \param day The day in the [1,30] range.
/// \param hour The hour in the [0,23] range.
/// \param minute The minute in the [0,59] range.
/// \param second The second in the [0,60] range.  Yes, that is 60, which can be
///     the case on leap seconds.
/// \param microsecond The microsecond in the [0,999999] range.
///
/// \return A new timestamp.
datetime::timestamp
datetime::timestamp::from_values(const int year, const int month,
                                 const int day, const int hour,
                                 const int minute, const int second,
                                 const int microsecond)
{
    PRE(year >= 1900);
    PRE(month >= 1 && month <= 12);
    PRE(day >= 1 && day <= 30);
    PRE(hour >= 0 && hour <= 23);
    PRE(minute >= 0 && minute <= 59);
    PRE(second >= 0 && second <= 60);
    PRE(microsecond >= 0 && microsecond <= 999999);

    // The code below is quite convoluted.  The problem is that we can't assume
    // that some fields (like tm_zone) of ::tm exist, and thus we can't blindly
    // set them from the code.  Instead of detecting their presence in the
    // configure script, we just query the current time to initialize such
    // fields and then we override the ones we are interested in.  (There might
    // be some better way to do this, but I don't know it and the documentation
    // does not shed much light into how to create your own fake date.)

    const time_t current_time = ::time(NULL);

    ::tm timedata;
    if (::gmtime_r(&current_time, &timedata) == NULL)
        UNREACHABLE;

    timedata.tm_sec = second;
    timedata.tm_min = minute;
    timedata.tm_hour = hour;
    timedata.tm_mday = day;
    timedata.tm_mon = month - 1;
    timedata.tm_year = year - 1900;
    // Ignored: timedata.tm_wday
    // Ignored: timedata.tm_yday

    ::timeval data;
    data.tv_sec = ::mktime(&timedata);
    data.tv_usec = static_cast< suseconds_t >(microsecond);
    return timestamp(std::shared_ptr< impl >(new impl(data)));
}


/// Constructs a new timestamp representing the current time in UTC.
///
/// \return A new timestamp.
datetime::timestamp
datetime::timestamp::now(void)
{
    if (mock_now)
        return mock_now.get();

    ::timeval data;
    {
        const int ret = ::gettimeofday(&data, NULL);
        INV(ret != -1);
    }

    return timestamp(std::shared_ptr< impl >(new impl(data)));
}


/// Formats a timestamp.
///
/// \param format The format string to use as consumed by strftime(3).
///
/// \return The formatted time.
std::string
datetime::timestamp::strftime(const std::string& format) const
{
    ::tm timedata;
    // This conversion to time_t is necessary because tv_sec is not guaranteed
    // to be a time_t.  For example, it isn't in NetBSD 5.x
    ::time_t epoch_seconds;
    epoch_seconds = _pimpl->data.tv_sec;
    if (::gmtime_r(&epoch_seconds, &timedata) == NULL)
        UNREACHABLE_MSG("gmtime_r(3) did not accept the value returned by "
                        "gettimeofday(2)");

    char buf[128];
    if (::strftime(buf, sizeof(buf), format.c_str(), &timedata) == 0)
        UNREACHABLE_MSG("Arbitrary-long format strings are unimplemented");
    return buf;
}


/// Formats a timestamp with the ISO 8601 standard and in UTC.
///
/// \return A string with the formatted timestamp.
std::string
datetime::timestamp::to_iso8601_in_utc(void) const
{
    return F("%s.%06sZ") % strftime("%Y-%m-%dT%H:%M:%S") % _pimpl->data.tv_usec;
}


/// Returns the number of microseconds since the epoch in UTC.
///
/// \return A number of microseconds.
int64_t
datetime::timestamp::to_microseconds(void) const
{
    return static_cast< int64_t >(_pimpl->data.tv_sec) * 1000000 +
        _pimpl->data.tv_usec;
}


/// Returns the number of seconds since the epoch in UTC.
///
/// \return A number of seconds.
int64_t
datetime::timestamp::to_seconds(void) const
{
    return static_cast< int64_t >(_pimpl->data.tv_sec);
}


/// Sets the current time for testing purposes.
void
datetime::set_mock_now(const int year, const int month,
                       const int day, const int hour,
                       const int minute, const int second,
                       const int microsecond)
{
    mock_now = timestamp::from_values(year, month, day, hour, minute, second,
                                      microsecond);
}


/// Sets the current time for testing purposes.
///
/// \param mock_now_ The mock timestamp to set the time to.
void
datetime::set_mock_now(const timestamp& mock_now_)
{
    mock_now = mock_now_;
}


/// Checks if two timestamps are equal.
///
/// \param other The object to compare to.
///
/// \return True if the two timestamps are equals; false otherwise.
bool
datetime::timestamp::operator==(const datetime::timestamp& other) const
{
    return _pimpl->data.tv_sec == other._pimpl->data.tv_sec &&
        _pimpl->data.tv_usec == other._pimpl->data.tv_usec;
}


/// Checks if two timestamps are different.
///
/// \param other The object to compare to.
///
/// \return True if the two timestamps are different; false otherwise.
bool
datetime::timestamp::operator!=(const datetime::timestamp& other) const
{
    return !(*this == other);
}


/// Checks if a timestamp is before another.
///
/// \param other The object to compare to.
///
/// \return True if this timestamp comes before other; false otherwise.
bool
datetime::timestamp::operator<(const datetime::timestamp& other) const
{
    return to_microseconds() < other.to_microseconds();
}


/// Checks if a timestamp is before or equal to another.
///
/// \param other The object to compare to.
///
/// \return True if this timestamp comes before other or is equal to it;
/// false otherwise.
bool
datetime::timestamp::operator<=(const datetime::timestamp& other) const
{
    return to_microseconds() <= other.to_microseconds();
}


/// Checks if a timestamp is after another.
///
/// \param other The object to compare to.
///
/// \return True if this timestamp comes after other; false otherwise;
bool
datetime::timestamp::operator>(const datetime::timestamp& other) const
{
    return to_microseconds() > other.to_microseconds();
}


/// Checks if a timestamp is after or equal to another.
///
/// \param other The object to compare to.
///
/// \return True if this timestamp comes after other or is equal to it;
/// false otherwise.
bool
datetime::timestamp::operator>=(const datetime::timestamp& other) const
{
    return to_microseconds() >= other.to_microseconds();
}


/// Calculates the addition of a delta to a timestamp.
///
/// \param other The delta to add.
///
/// \return A new timestamp in the future.
datetime::timestamp
datetime::timestamp::operator+(const datetime::delta& other) const
{
    return datetime::timestamp::from_microseconds(to_microseconds() +
                                                  other.to_microseconds());
}


/// Calculates the addition of a delta to this timestamp.
///
/// \param other The delta to add.
///
/// \return A reference to the modified timestamp.
datetime::timestamp&
datetime::timestamp::operator+=(const datetime::delta& other)
{
    *this = *this + other;
    return *this;
}


/// Calculates the subtraction of a delta from a timestamp.
///
/// \param other The delta to subtract.
///
/// \return A new timestamp in the past.
datetime::timestamp
datetime::timestamp::operator-(const datetime::delta& other) const
{
    return datetime::timestamp::from_microseconds(to_microseconds() -
                                                  other.to_microseconds());
}


/// Calculates the subtraction of a delta from this timestamp.
///
/// \param other The delta to subtract.
///
/// \return A reference to the modified timestamp.
datetime::timestamp&
datetime::timestamp::operator-=(const datetime::delta& other)
{
    *this = *this - other;
    return *this;
}


/// Calculates the delta between two timestamps.
///
/// \param other The subtrahend.
///
/// \return The difference between this object and the other object.
///
/// \throw std::runtime_error If the subtraction would result in a negative time
///     delta, which are currently not supported.
datetime::delta
datetime::timestamp::operator-(const datetime::timestamp& other) const
{
    /*
     * XXX-BD: gettimeofday isn't necessarily monotonic so return the
     * smallest non-zero delta if time went backwards.
     */
    if ((*this) < other)
        return datetime::delta::from_microseconds(1);
    return datetime::delta::from_microseconds(to_microseconds() -
                                              other.to_microseconds());
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
std::ostream&
datetime::operator<<(std::ostream& output, const timestamp& object)
{
    return (output << object.to_microseconds() << "us");
}
