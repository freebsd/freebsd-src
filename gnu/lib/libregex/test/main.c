/* Main routine for running various tests.  Meant only to be linked with
   all the auxiliary test source files, with `test' undefined.  */

#include "test.h"

test_type t = all_test;


/* Use this to run the tests we've thought of.  */

int
main ()
{
  switch (t)
  {
  case all_test:
    test_regress ();
    test_others ();
    test_posix_basic ();
    test_posix_extended ();
    test_posix_interface ();
    break;

  case other_test:
    test_others ();
    break;

  case posix_basic_test:
    test_posix_basic ();
    break;

  case posix_extended_test:
    test_posix_extended ();
    break;

  case posix_interface_test:
    test_posix_interface ();
    break;

  case regress_test:
    test_regress ();
    break;

  default:
    fprintf (stderr, "Unknown test %d.\n", t);
  }

  return 0;
}
