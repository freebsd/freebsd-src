// RUN: %llvmgcc %s -S -o - -fno-math-errno | FileCheck %s
// llvm.sqrt has undefined behavior on negative inputs, so it is
// inappropriate to translate C/C++ sqrt to this.
#include <math.h>

float foo(float X) {
// CHECK: foo
// CHECK: sqrtf(float %1) nounwind readonly
  // Check that this is marked readonly when errno is ignored.
  return sqrtf(X);
}
