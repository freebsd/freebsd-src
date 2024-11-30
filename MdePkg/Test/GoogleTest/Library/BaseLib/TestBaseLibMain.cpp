/** @file
  Main routine for BaseLib google tests.

  Copyright (c) 2023 Pedro Falcato. All rights reserved<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <gtest/gtest.h>

int
main (
  int   argc,
  char  *argv[]
  )
{
  testing::InitGoogleTest (&argc, argv);
  return RUN_ALL_TESTS ();
}
