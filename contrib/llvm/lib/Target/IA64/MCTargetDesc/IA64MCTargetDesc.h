#ifndef LLVM_TARGET_IA64_MCTARGETDESC_H
#define LLVM_TARGET_IA64_MCTARGETDESC_H

namespace llvm {

  class MCSubtargetInfo;
  class Target;
  class StringRef;

  extern Target TheIA64Target;

} // namespace llvm

#define GET_REGINFO_ENUM
#include "IA64GenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "IA64GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "IA64GenSubtargetInfo.inc"

#endif // LLVM_TARGET_IA64_MCTARGETDESC_H
