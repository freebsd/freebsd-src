/*
 * Operators used in the test command.
 */

#include <stdio.h>
#include "operators.h"

char *const unary_op[] = {
      "!",
      "-b",
      "-c",
      "-d",
      "-e",
      "-f",
      "-g",
      "-k",
      "-n",
      "-p",
      "-r",
      "-s",
      "-t",
      "-u",
      "-w",
      "-x",
      "-z",
      NULL
};

char *const binary_op[] = {
      "-o",
      "|",
      "-a",
      "&",
      "=",
      "!=",
      "-eq",
      "-ne",
      "-gt",
      "-lt",
      "-le",
      "-ge",
      NULL
};

char *const andor_op[] = {
      "-o",
      "|",
      "-a",
      "&",
      NULL
};

const char op_priority[] = {
      3,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      12,
      1,
      1,
      2,
      2,
      4,
      4,
      4,
      4,
      4,
      4,
      4,
      4,
};

const char op_argflag[] = {
      0,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_STRING,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_INT,
      OP_FILE,
      OP_FILE,
      OP_FILE,
      OP_STRING,
      0,
      0,
      0,
      0,
      OP_STRING,
      OP_STRING,
      OP_INT,
      OP_INT,
      OP_INT,
      OP_INT,
      OP_INT,
      OP_INT,
};
