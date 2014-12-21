//===-- PPCMachineFunctionInfo.cpp - Private data used for PowerPC --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "PPCMachineFunctionInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

void PPCFunctionInfo::anchor() { }

MCSymbol *PPCFunctionInfo::getPICOffsetSymbol() const {
  const MCAsmInfo *MAI = MF.getTarget().getMCAsmInfo();
  return MF.getContext().GetOrCreateSymbol(Twine(MAI->getPrivateGlobalPrefix())+
    Twine(MF.getFunctionNumber())+"$poff");
}
