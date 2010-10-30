// testfile.h -- test input files   -*- C++ -*-

#ifndef GOLD_TESTSUITE_TESTFILE_H
#define GOLD_TESTSUITE_TESTFILE_H

namespace gold
{
class Target;
}

namespace gold_testsuite
{

extern gold::Target* target_test_pointer;
extern const unsigned char test_file_1[];
extern const unsigned int test_file_1_size;

}; // End namespace gold_testsuite.

#endif // !defined(GOLD_TESTSUITE_TESTFILE_H)
