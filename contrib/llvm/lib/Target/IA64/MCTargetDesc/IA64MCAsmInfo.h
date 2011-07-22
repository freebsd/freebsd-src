#ifndef LLVM_TARGET_IA64_MCASMINFO_H
#define LLVM_TARGET_IA64_MCASMINFO_H

#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCAsmInfo.h"

namespace llvm {
  class Target;

  struct IA64MCAsmInfo : public MCAsmInfo {
    explicit IA64MCAsmInfo(const Target &T, StringRef TT);
  };

} // namespace llvm

#endif // LLVM_TARGET_IA64_MCASMINFO_H
