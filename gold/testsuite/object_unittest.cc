// object_unittest.cc -- test Object, Relobj, etc.

#include "gold.h"

#include "object.h"

#include "test.h"
#include "testfile.h"

namespace gold_testsuite
{

using namespace gold;

// Test basic Object functionality.

bool
Object_test(Test_report*)
{
  Input_file input_file("test.o", test_file_1, test_file_1_size);
  Object* object = make_elf_object("test.o", &input_file, 0,
				   test_file_1, test_file_1_size);
  CHECK(object->name() == "test.o");
  CHECK(!object->is_dynamic());
  CHECK(object->target() == target_test_pointer);
  CHECK(object->is_locked());
  object->unlock();
  CHECK(!object->is_locked());
  object->lock();
  CHECK(object->shnum() == 5);
  CHECK(object->section_name(0).empty());
  CHECK(object->section_name(1) == ".test");
  CHECK(object->section_flags(0) == 0);
  CHECK(object->section_flags(1) == elfcpp::SHF_ALLOC);
  object->unlock();
  return true;
}

Register_test object_register("Object", Object_test);

} // End namespace gold_testsuite.
