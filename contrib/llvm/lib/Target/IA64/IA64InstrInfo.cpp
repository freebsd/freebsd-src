#include "IA64.h"
#include "IA64InstrInfo.h"
#include "IA64TargetMachine.h"

#include "llvm/Function.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/Support/ErrorHandling.h"

#include "IA64GenInstrInfo.inc"

using namespace llvm;

IA64InstrInfo::IA64InstrInfo(IA64TargetMachine &tm) :
    TargetInstrInfoImpl(IA64Insts, array_lengthof(IA64Insts)),
    RI(tm, *this),
    TM(tm)
{
  // nothing to do
}
