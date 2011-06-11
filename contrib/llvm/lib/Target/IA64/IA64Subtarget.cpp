#include "IA64.h"
#include "IA64Subtarget.h"

#include "IA64GenSubtarget.inc"

using namespace llvm;

IA64Subtarget::IA64Subtarget(const std::string &TT, const std::string &FS)
{
#if defined(__ia64__)
  std::string CPU = sys::getHostCPUName();
#else
  std::string CPU = "mckinley";
#endif

  // Parse features string.
  ParseSubtargetFeatures(FS, CPU);
}
