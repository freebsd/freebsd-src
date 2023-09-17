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

#include "utils/optional.ipp"

#include <iostream>
#include <sstream>

#include <atf-c++.hpp>

using utils::none;
using utils::optional;


namespace {


/// Fake class to capture calls to the new and delete operators.
class test_alloc {
public:
    /// Value to disambiguate objects after construction.
    int value;

    /// Balance of alive instances of this class in dynamic memory.
    static size_t instances;

    /// Constructs a new optional object.
    ///
    /// \param value_ The value to store in this object for disambiguation.
    test_alloc(int value_) : value(value_)
    {
    }

    /// Allocates a new object and records its existence.
    ///
    /// \param size The amount of memory to allocate.
    ///
    /// \return A pointer to the allocated memory.
    ///
    /// \throw std::bad_alloc If the memory allocation fails.
    void*
    operator new(size_t size)
    {
        instances++;
        std::cout << "test_alloc::operator new called\n";
        return ::operator new(size);
    }

    /// Deallocates an existing object and unrecords its existence.
    ///
    /// \param mem The pointer to the memory to deallocate.
    void
    operator delete(void* mem)
    {
        instances--;
        std::cout << "test_alloc::operator delete called\n";
        ::operator delete(mem);
    }
};


size_t test_alloc::instances = 0;


/// Constructs and returns an optional object.
///
/// This is used by tests to validate that returning an object from within a
/// function works (i.e. the necessary constructors are available).
///
/// \tparam Type The type of the object included in the optional wrapper.
/// \param value The value to put inside the optional wrapper.
///
/// \return The constructed optional object.
template< typename Type >
optional< Type >
return_optional(const Type& value)
{
    return optional< Type >(value);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(ctors__native_type);
ATF_TEST_CASE_BODY(ctors__native_type)
{
    const optional< int > no_args;
    ATF_REQUIRE(!no_args);

    const optional< int > with_none(none);
    ATF_REQUIRE(!with_none);

    const optional< int > with_arg(3);
    ATF_REQUIRE(with_arg);
    ATF_REQUIRE_EQ(3, with_arg.get());

    const optional< int > copy_none(with_none);
    ATF_REQUIRE(!copy_none);

    const optional< int > copy_arg(with_arg);
    ATF_REQUIRE(copy_arg);
    ATF_REQUIRE_EQ(3, copy_arg.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(ctors__complex_type);
ATF_TEST_CASE_BODY(ctors__complex_type)
{
    const optional< std::string > no_args;
    ATF_REQUIRE(!no_args);

    const optional< std::string > with_none(none);
    ATF_REQUIRE(!with_none);

    const optional< std::string > with_arg("foo");
    ATF_REQUIRE(with_arg);
    ATF_REQUIRE_EQ("foo", with_arg.get());

    const optional< std::string > copy_none(with_none);
    ATF_REQUIRE(!copy_none);

    const optional< std::string > copy_arg(with_arg);
    ATF_REQUIRE(copy_arg);
    ATF_REQUIRE_EQ("foo", copy_arg.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(assign);
ATF_TEST_CASE_BODY(assign)
{
    optional< int > from_default;
    from_default = optional< int >();
    ATF_REQUIRE(!from_default);

    optional< int > from_none(3);
    from_none = none;
    ATF_REQUIRE(!from_none);

    optional< int > from_int;
    from_int = 6;
    ATF_REQUIRE_EQ(6, from_int.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(return);
ATF_TEST_CASE_BODY(return)
{
    optional< long > from_return(return_optional< long >(123));
    ATF_REQUIRE(from_return);
    ATF_REQUIRE_EQ(123, from_return.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(memory);
ATF_TEST_CASE_BODY(memory)
{
    ATF_REQUIRE_EQ(0, test_alloc::instances);
    {
        optional< test_alloc > optional1(test_alloc(3));
        ATF_REQUIRE_EQ(1, test_alloc::instances);
        ATF_REQUIRE_EQ(3, optional1.get().value);

        {
            optional< test_alloc > optional2(optional1);
            ATF_REQUIRE_EQ(2, test_alloc::instances);
            ATF_REQUIRE_EQ(3, optional2.get().value);

            optional2 = 5;
            ATF_REQUIRE_EQ(2, test_alloc::instances);
            ATF_REQUIRE_EQ(5, optional2.get().value);
            ATF_REQUIRE_EQ(3, optional1.get().value);
        }
        ATF_REQUIRE_EQ(1, test_alloc::instances);
        ATF_REQUIRE_EQ(3, optional1.get().value);
    }
    ATF_REQUIRE_EQ(0, test_alloc::instances);
}


ATF_TEST_CASE_WITHOUT_HEAD(get_default);
ATF_TEST_CASE_BODY(get_default)
{
    const std::string def_value = "hello";
    optional< std::string > optional;
    ATF_REQUIRE(&def_value == &optional.get_default(def_value));
    optional = "bye";
    ATF_REQUIRE_EQ("bye", optional.get_default(def_value));
}


ATF_TEST_CASE_WITHOUT_HEAD(make_optional);
ATF_TEST_CASE_BODY(make_optional)
{
    optional< int > opt = utils::make_optional(576);
    ATF_REQUIRE(opt);
    ATF_REQUIRE_EQ(576, opt.get());
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne);
ATF_TEST_CASE_BODY(operators_eq_and_ne)
{
    optional< int > opt1, opt2;

    opt1 = none; opt2 = none;
    ATF_REQUIRE(  opt1 == opt2);
    ATF_REQUIRE(!(opt1 != opt2));

    opt1 = utils::make_optional(5); opt2 = none;
    ATF_REQUIRE(!(opt1 == opt2));
    ATF_REQUIRE(  opt1 != opt2);

    opt1 = none; opt2 = utils::make_optional(5);
    ATF_REQUIRE(!(opt1 == opt2));
    ATF_REQUIRE(  opt1 != opt2);

    opt1 = utils::make_optional(5); opt2 = utils::make_optional(5);
    ATF_REQUIRE(  opt1 == opt2);
    ATF_REQUIRE(!(opt1 != opt2));

    opt1 = utils::make_optional(6); opt2 = utils::make_optional(5);
    ATF_REQUIRE(!(opt1 == opt2));
    ATF_REQUIRE(  opt1 != opt2);
}


ATF_TEST_CASE_WITHOUT_HEAD(output);
ATF_TEST_CASE_BODY(output)
{
    {
        std::ostringstream str;
        str << optional< int >(none);
        ATF_REQUIRE_EQ("none", str.str());
    }
    {
        std::ostringstream str;
        str << optional< int >(5);
        ATF_REQUIRE_EQ("5", str.str());
    }
    {
        std::ostringstream str;
        str << optional< std::string >("this is a text");
        ATF_REQUIRE_EQ("this is a text", str.str());
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ctors__native_type);
    ATF_ADD_TEST_CASE(tcs, ctors__complex_type);
    ATF_ADD_TEST_CASE(tcs, assign);
    ATF_ADD_TEST_CASE(tcs, return);
    ATF_ADD_TEST_CASE(tcs, memory);
    ATF_ADD_TEST_CASE(tcs, get_default);
    ATF_ADD_TEST_CASE(tcs, make_optional);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne);
    ATF_ADD_TEST_CASE(tcs, output);
}
