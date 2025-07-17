// Copyright 2012 The Kyua Authors.
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

#include "utils/units.hpp"

#include <sstream>
#include <stdexcept>

#include <atf-c++.hpp>

namespace units = utils::units;


ATF_TEST_CASE_WITHOUT_HEAD(bytes__format__tb);
ATF_TEST_CASE_BODY(bytes__format__tb)
{
    using units::TB;
    using units::GB;

    ATF_REQUIRE_EQ("2.00T", units::bytes(2 * TB).format());
    ATF_REQUIRE_EQ("45.12T", units::bytes(45 * TB + 120 * GB).format());
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__format__gb);
ATF_TEST_CASE_BODY(bytes__format__gb)
{
    using units::GB;
    using units::MB;

    ATF_REQUIRE_EQ("5.00G", units::bytes(5 * GB).format());
    ATF_REQUIRE_EQ("745.96G", units::bytes(745 * GB + 980 * MB).format());
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__format__mb);
ATF_TEST_CASE_BODY(bytes__format__mb)
{
    using units::MB;
    using units::KB;

    ATF_REQUIRE_EQ("1.00M", units::bytes(1 * MB).format());
    ATF_REQUIRE_EQ("1023.50M", units::bytes(1023 * MB + 512 * KB).format());
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__format__kb);
ATF_TEST_CASE_BODY(bytes__format__kb)
{
    using units::KB;

    ATF_REQUIRE_EQ("3.00K", units::bytes(3 * KB).format());
    ATF_REQUIRE_EQ("1.33K", units::bytes(1 * KB + 340).format());
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__format__b);
ATF_TEST_CASE_BODY(bytes__format__b)
{
    ATF_REQUIRE_EQ("0", units::bytes().format());
    ATF_REQUIRE_EQ("0", units::bytes(0).format());
    ATF_REQUIRE_EQ("1023", units::bytes(1023).format());
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__parse__tb);
ATF_TEST_CASE_BODY(bytes__parse__tb)
{
    using units::TB;
    using units::GB;

    ATF_REQUIRE_EQ(0, units::bytes::parse("0T"));
    ATF_REQUIRE_EQ(units::bytes(TB), units::bytes::parse("1T"));
    ATF_REQUIRE_EQ(units::bytes(TB), units::bytes::parse("1t"));
    ATF_REQUIRE_EQ(13567973486755LL, units::bytes::parse("12.340000T"));
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__parse__gb);
ATF_TEST_CASE_BODY(bytes__parse__gb)
{
    using units::GB;
    using units::MB;

    ATF_REQUIRE_EQ(0, units::bytes::parse("0G"));
    ATF_REQUIRE_EQ(units::bytes(GB), units::bytes::parse("1G"));
    ATF_REQUIRE_EQ(units::bytes(GB), units::bytes::parse("1g"));
    ATF_REQUIRE_EQ(13249974108LL, units::bytes::parse("12.340G"));
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__parse__mb);
ATF_TEST_CASE_BODY(bytes__parse__mb)
{
    using units::MB;
    using units::KB;

    ATF_REQUIRE_EQ(0, units::bytes::parse("0M"));
    ATF_REQUIRE_EQ(units::bytes(MB), units::bytes::parse("1M"));
    ATF_REQUIRE_EQ(units::bytes(MB), units::bytes::parse("1m"));
    ATF_REQUIRE_EQ(12939427, units::bytes::parse("12.34000M"));
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__parse__kb);
ATF_TEST_CASE_BODY(bytes__parse__kb)
{
    using units::KB;

    ATF_REQUIRE_EQ(0, units::bytes::parse("0K"));
    ATF_REQUIRE_EQ(units::bytes(KB), units::bytes::parse("1K"));
    ATF_REQUIRE_EQ(units::bytes(KB), units::bytes::parse("1k"));
    ATF_REQUIRE_EQ(12636, units::bytes::parse("12.34K"));
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__parse__b);
ATF_TEST_CASE_BODY(bytes__parse__b)
{
    ATF_REQUIRE_EQ(0, units::bytes::parse("0"));
    ATF_REQUIRE_EQ(89, units::bytes::parse("89"));
    ATF_REQUIRE_EQ(1234, units::bytes::parse("1234"));
    ATF_REQUIRE_EQ(1234567890, units::bytes::parse("1234567890"));
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__parse__error);
ATF_TEST_CASE_BODY(bytes__parse__error)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error, "empty", units::bytes::parse(""));
    ATF_REQUIRE_THROW_RE(std::runtime_error, "empty", units::bytes::parse("k"));

    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid.*'.'",
                         units::bytes::parse("."));
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid.*'3.'",
                         units::bytes::parse("3."));
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid.*'.3'",
                         units::bytes::parse(".3"));

    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid.*' t'",
                         units::bytes::parse(" t"));
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid.*'.t'",
                         units::bytes::parse(".t"));
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid.*'12 t'",
                         units::bytes::parse("12 t"));
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid.*'12.t'",
                         units::bytes::parse("12.t"));
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid.*'.12t'",
                         units::bytes::parse(".12t"));
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Invalid.*'abt'",
                         units::bytes::parse("abt"));
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__istream__one_word);
ATF_TEST_CASE_BODY(bytes__istream__one_word)
{
    std::istringstream input("12M");

    units::bytes bytes;
    input >> bytes;
    ATF_REQUIRE(input.eof());
    ATF_REQUIRE_EQ(units::bytes(12 * units::MB), bytes);
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__istream__many_words);
ATF_TEST_CASE_BODY(bytes__istream__many_words)
{
    std::istringstream input("12M more");

    units::bytes bytes;
    input >> bytes;
    ATF_REQUIRE(input.good());
    ATF_REQUIRE_EQ(units::bytes(12 * units::MB), bytes);

    std::string word;
    input >> word;
    ATF_REQUIRE_EQ("more", word);
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__istream__error);
ATF_TEST_CASE_BODY(bytes__istream__error)
{
    std::istringstream input("12.M more");

    units::bytes bytes(123456789);
    input >> bytes;
    ATF_REQUIRE(input.bad());
    ATF_REQUIRE_EQ(units::bytes(123456789), bytes);
}


ATF_TEST_CASE_WITHOUT_HEAD(bytes__ostream);
ATF_TEST_CASE_BODY(bytes__ostream)
{
    std::ostringstream output;
    output << "foo " << units::bytes(5 * units::KB) << " bar";
    ATF_REQUIRE_EQ("foo 5.00K bar", output.str());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, bytes__format__tb);
    ATF_ADD_TEST_CASE(tcs, bytes__format__gb);
    ATF_ADD_TEST_CASE(tcs, bytes__format__mb);
    ATF_ADD_TEST_CASE(tcs, bytes__format__kb);
    ATF_ADD_TEST_CASE(tcs, bytes__format__b);

    ATF_ADD_TEST_CASE(tcs, bytes__parse__tb);
    ATF_ADD_TEST_CASE(tcs, bytes__parse__gb);
    ATF_ADD_TEST_CASE(tcs, bytes__parse__mb);
    ATF_ADD_TEST_CASE(tcs, bytes__parse__kb);
    ATF_ADD_TEST_CASE(tcs, bytes__parse__b);
    ATF_ADD_TEST_CASE(tcs, bytes__parse__error);

    ATF_ADD_TEST_CASE(tcs, bytes__istream__one_word);
    ATF_ADD_TEST_CASE(tcs, bytes__istream__many_words);
    ATF_ADD_TEST_CASE(tcs, bytes__istream__error);
    ATF_ADD_TEST_CASE(tcs, bytes__ostream);
}
