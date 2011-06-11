#include "IA64.h"
#include "IA64RegisterInfo.h"
#include "IA64TargetMachine.h"

#include "llvm/Function.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

// FIXME: Provide proper call frame setup / destroy opcodes.
IA64RegisterInfo::IA64RegisterInfo(IA64TargetMachine &tm,
	const TargetInstrInfo &tii) :
    IA64GenRegisterInfo(IA64::NOP, IA64::NOP),
    TM(tm),
    TII(tii)
{
  StackAlign = TM.getFrameLowering()->getStackAlignment();
}

const unsigned *
IA64RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const
{
  llvm_unreachable(__func__);
}

BitVector
IA64RegisterInfo::getReservedRegs(const MachineFunction &MF) const
{
  BitVector rsvd(getNumRegs());

  rsvd.set(IA64::R0); // constant, hardwired 0
  rsvd.set(IA64::R1); // global data pointer (aka gp)
  rsvd.set(IA64::R12); // (memory) stack pointer (aka sp)
  rsvd.set(IA64::R13); // thread pointer (aka tp)

  rsvd.set(IA64::F0); // constant, hardwired 0.0
  rsvd.set(IA64::F1); // constant, hardwired 1.0

  rsvd.set(IA64::P0); // constant, hardwired 1 (true)

  return rsvd;
}

void
IA64RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator MI, int SPAdj,
    RegScavenger *RS) const
{
  llvm_unreachable(__func__);
}

int
IA64RegisterInfo::getDwarfRegNum(unsigned RegNum, bool isEH) const
{
  return IA64GenRegisterInfo::getDwarfRegNumFull(RegNum, 0);
}

unsigned
IA64RegisterInfo::getFrameRegister(const MachineFunction &MF) const
{
  llvm_unreachable(__func__);
}

unsigned
IA64RegisterInfo::getRARegister() const
{
  llvm_unreachable(__func__);
}

#include "IA64GenRegisterInfo.inc"
