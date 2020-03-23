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

#include "utils/auto_array.ipp"

extern "C" {
#include <sys/types.h>
}

#include <iostream>

#include <atf-c++.hpp>

#include "utils/defs.hpp"

using utils::auto_array;


namespace {


/// Mock class to capture calls to the new and delete operators.
class test_array {
public:
    /// User-settable cookie to disambiguate instances of this class.
    int m_value;

    /// The current balance of existing test_array instances.
    static ssize_t m_nblocks;

    /// Captures invalid calls to new on an array.
    ///
    /// \return Nothing; this always fails the test case.
    void*
    operator new(const size_t /* size */)
    {
        ATF_FAIL("New called but should have been new[]");
        return new int(5);
    }

    /// Obtains memory for a new instance and increments m_nblocks.
    ///
    /// \param size The amount of memory to allocate, in bytes.
    ///
    /// \return A pointer to the allocated memory.
    ///
    /// \throw std::bad_alloc If the memory cannot be allocated.
    void*
    operator new[](const size_t size)
    {
        void* mem = ::operator new(size);
        m_nblocks++;
        std::cout << "Allocated 'test_array' object " << mem << "\n";
        return mem;
    }

    /// Captures invalid calls to delete on an array.
    ///
    /// \return Nothing; this always fails the test case.
    void
    operator delete(void* /* mem */)
    {
        ATF_FAIL("Delete called but should have been delete[]");
    }

    /// Deletes a previously allocated array and decrements m_nblocks.
    ///
    /// \param mem The pointer to the memory to be deleted.
    void
    operator delete[](void* mem)
    {
        std::cout << "Releasing 'test_array' object " << mem << "\n";
        if (m_nblocks == 0)
            ATF_FAIL("Unbalanced delete[]");
        m_nblocks--;
        ::operator delete(mem);
    }
};


ssize_t test_array::m_nblocks = 0;


}  // anonymous namespace


ATF_TEST_CASE(scope);
ATF_TEST_CASE_HEAD(scope)
{
    set_md_var("descr", "Tests the automatic scope handling in the "
               "auto_array smart pointer class");
}
ATF_TEST_CASE_BODY(scope)
{
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    {
        auto_array< test_array > t(new test_array[10]);
        ATF_REQUIRE_EQ(test_array::m_nblocks, 1);
    }
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
}


ATF_TEST_CASE(copy);
ATF_TEST_CASE_HEAD(copy)
{
    set_md_var("descr", "Tests the auto_array smart pointer class' copy "
               "constructor");
}
ATF_TEST_CASE_BODY(copy)
{
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    {
        auto_array< test_array > t1(new test_array[10]);
        ATF_REQUIRE_EQ(test_array::m_nblocks, 1);

        {
            auto_array< test_array > t2(t1);
            ATF_REQUIRE_EQ(test_array::m_nblocks, 1);
        }
        ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    }
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
}


ATF_TEST_CASE(copy_ref);
ATF_TEST_CASE_HEAD(copy_ref)
{
    set_md_var("descr", "Tests the auto_array smart pointer class' copy "
               "constructor through the auxiliary ref object");
}
ATF_TEST_CASE_BODY(copy_ref)
{
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    {
        auto_array< test_array > t1(new test_array[10]);
        ATF_REQUIRE_EQ(test_array::m_nblocks, 1);

        {
            auto_array< test_array > t2 = t1;
            ATF_REQUIRE_EQ(test_array::m_nblocks, 1);
        }
        ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    }
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
}


ATF_TEST_CASE(get);
ATF_TEST_CASE_HEAD(get)
{
    set_md_var("descr", "Tests the auto_array smart pointer class' get "
               "method");
}
ATF_TEST_CASE_BODY(get)
{
    test_array* ta = new test_array[10];
    auto_array< test_array > t(ta);
    ATF_REQUIRE_EQ(t.get(), ta);
}


ATF_TEST_CASE(release);
ATF_TEST_CASE_HEAD(release)
{
    set_md_var("descr", "Tests the auto_array smart pointer class' release "
               "method");
}
ATF_TEST_CASE_BODY(release)
{
    test_array* ta1 = new test_array[10];
    {
        auto_array< test_array > t(ta1);
        ATF_REQUIRE_EQ(test_array::m_nblocks, 1);
        test_array* ta2 = t.release();
        ATF_REQUIRE_EQ(ta2, ta1);
        ATF_REQUIRE_EQ(test_array::m_nblocks, 1);
    }
    ATF_REQUIRE_EQ(test_array::m_nblocks, 1);
    delete [] ta1;
}


ATF_TEST_CASE(reset);
ATF_TEST_CASE_HEAD(reset)
{
    set_md_var("descr", "Tests the auto_array smart pointer class' reset "
               "method");
}
ATF_TEST_CASE_BODY(reset)
{
    test_array* ta1 = new test_array[10];
    test_array* ta2 = new test_array[10];
    ATF_REQUIRE_EQ(test_array::m_nblocks, 2);

    {
        auto_array< test_array > t(ta1);
        ATF_REQUIRE_EQ(test_array::m_nblocks, 2);
        t.reset(ta2);
        ATF_REQUIRE_EQ(test_array::m_nblocks, 1);
        t.reset();
        ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    }
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
}


ATF_TEST_CASE(assign);
ATF_TEST_CASE_HEAD(assign)
{
    set_md_var("descr", "Tests the auto_array smart pointer class' "
               "assignment operator");
}
ATF_TEST_CASE_BODY(assign)
{
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    {
        auto_array< test_array > t1(new test_array[10]);
        ATF_REQUIRE_EQ(test_array::m_nblocks, 1);

        {
            auto_array< test_array > t2;
            t2 = t1;
            ATF_REQUIRE_EQ(test_array::m_nblocks, 1);
        }
        ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    }
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
}


ATF_TEST_CASE(assign_ref);
ATF_TEST_CASE_HEAD(assign_ref)
{
    set_md_var("descr", "Tests the auto_array smart pointer class' "
               "assignment operator through the auxiliary ref "
               "object");
}
ATF_TEST_CASE_BODY(assign_ref)
{
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    {
        auto_array< test_array > t1(new test_array[10]);
        ATF_REQUIRE_EQ(test_array::m_nblocks, 1);

        {
            auto_array< test_array > t2;
            t2 = t1;
            ATF_REQUIRE_EQ(test_array::m_nblocks, 1);
        }
        ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
    }
    ATF_REQUIRE_EQ(test_array::m_nblocks, 0);
}


ATF_TEST_CASE(access);
ATF_TEST_CASE_HEAD(access)
{
    set_md_var("descr", "Tests the auto_array smart pointer class' access "
               "operator");
}
ATF_TEST_CASE_BODY(access)
{
    auto_array< test_array > t(new test_array[10]);

    for (int i = 0; i < 10; i++)
        t[i].m_value = i * 2;

    for (int i = 0; i < 10; i++)
        ATF_REQUIRE_EQ(t[i].m_value, i * 2);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, scope);
    ATF_ADD_TEST_CASE(tcs, copy);
    ATF_ADD_TEST_CASE(tcs, copy_ref);
    ATF_ADD_TEST_CASE(tcs, get);
    ATF_ADD_TEST_CASE(tcs, release);
    ATF_ADD_TEST_CASE(tcs, reset);
    ATF_ADD_TEST_CASE(tcs, assign);
    ATF_ADD_TEST_CASE(tcs, assign_ref);
    ATF_ADD_TEST_CASE(tcs, access);
}
