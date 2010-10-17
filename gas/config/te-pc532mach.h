/* This file is te-pc532.h
   Written by Ian Dall <idall@eleceng.adelaide.edu.au>
   24-May-94.  */

#define TARGET_FORMAT		"a.out-pc532-mach"

#include "obj-format.h"

/* Maybe these should be more like TC_NS32532 and TC_NS32381 in case
   of conflicts. NS32381 is used in opcode/ns32k.h and that is also
   used by GDB. Need to check.  */
#define NS32532
#define NS32381
