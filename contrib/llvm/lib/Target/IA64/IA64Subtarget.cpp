#include "IA64.h"
#include "IA64Subtarget.h"

#include "IA64GenSubtarget.inc"

using namespace llvm;

IA64Subtarget::IA64Subtarget(const std::string &TT, const std::string &FS) {
  std::string CPU = "generic";

  // Parse features string.
  ParseSubtargetFeatures(FS, CPU);
}
