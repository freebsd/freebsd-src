// testmain.cc -- main function for simplisitic gold test framework.

#include "gold.h"

#include "test.h"

using namespace gold_testsuite;

int
main(int, char** argv)
{
  gold::program_name = argv[0];

  Test_framework tf;
  Register_test::run_tests(&tf);

  exit(tf.failures());
}
