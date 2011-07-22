#include "IA64.h"
#include "IA64Subtarget.h"

#include "llvm/Support/Host.h"
#include "llvm/Target/TargetRegistry.h"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "IA64GenSubtargetInfo.inc"

using namespace llvm;

IA64Subtarget::IA64Subtarget(const std::string &TT, const std::string &CPU,
        const std::string &FS) :
    IA64GenSubtargetInfo(TT, CPU, FS)
{
#if defined(__ia64__)
  std::string CPUName = sys::getHostCPUName();
#else
  std::string CPUName = "mckinley";
#endif

  // Parse features string.
  ParseSubtargetFeatures(CPUName, FS);
}
