#include "IA64.h"
#include "IA64InstrInfo.h"
#include "IA64TargetMachine.h"

#include "llvm/Function.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetRegistry.h"

#define GET_INSTRINFO_CTOR
#include "IA64GenInstrInfo.inc"

using namespace llvm;

IA64InstrInfo::IA64InstrInfo(IA64TargetMachine &tm) :
    IA64GenInstrInfo(IA64::NOP, IA64::NOP),
    RI(tm, *this),
    TM(tm)
{
  // nothing to do
}

void
IA64InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI, DebugLoc DL, unsigned DestReg,
    unsigned SrcReg, bool KillSrc) const
{
  bool GRDest = IA64::GRRegClass.contains(DestReg);
  bool GRSrc  = IA64::GRRegClass.contains(SrcReg);

  if (GRDest && GRSrc) {
    MachineInstrBuilder MIB = BuildMI(MBB, MI, DL, get(IA64::ADD), DestReg);
    MIB.addReg(IA64::R0);
    MIB.addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }

  llvm_unreachable(__func__);
}
