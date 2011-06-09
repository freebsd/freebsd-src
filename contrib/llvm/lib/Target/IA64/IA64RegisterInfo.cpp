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
  llvm_unreachable(__func__);
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
