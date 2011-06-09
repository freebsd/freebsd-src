#include "IA64.h"
#include "llvm/Module.h"
#include "llvm/Target/TargetRegistry.h"

using namespace llvm;

Target llvm::TheIA64Target;

extern "C" void
LLVMInitializeIA64TargetInfo()
{
  RegisterTarget<Triple::ia64, /*HasJIT=*/false>
    X(TheIA64Target, "ia64", "Itanium Processor Family");
}
