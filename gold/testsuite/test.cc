// test.cc -- simplistic test framework for gold.

#include "gold.h"

#include <cstdio>

#include "test.h"

namespace gold_testsuite
{

// Test_framework methods.

// The current test being run.

Test_report* Test_framework::current_report;

// Run a test.

void
Test_framework::run(const char *name, bool (*pfn)(Test_report*))
{
  this->testname_ = name;
  this->current_fail_ = false;

  Test_report tr(this);
  Test_framework::current_report = &tr;

  if ((*pfn)(&tr) && !this->current_fail_)
    {
      printf("PASS: %s\n", name);
      ++this->passes_;
    }
  else
    {
      printf("FAIL: %s\n", name);
      ++this->failures_;
    }

  Test_framework::current_report = NULL;
  this->testname_ = NULL;
}

// Let a test report an error.

void
Test_framework::error(const char* message)
{
  printf("ERROR: %s: %s\n", this->testname_, message);
  this->fail();
}

// Register_test methods.

// Linked list of all registered tests.

Register_test* Register_test::all_tests;

// Register a test.

Register_test::Register_test(const char* name, bool (*pfn)(Test_report*))
  : name_(name), pfn_(pfn), next_(Register_test::all_tests)
{
  Register_test::all_tests = this;
}

// Run all registered tests.

void
Register_test::run_tests(Test_framework* tf)
{
  for (Register_test* p = Register_test::all_tests;
       p != NULL;
       p = p->next_)
    tf->run(p->name_, p->pfn_);
}

} // End namespace gold_testsuite.
