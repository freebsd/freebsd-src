#include "IA64MCInstLower.h"

#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Target/Mangler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/SmallString.h"

using namespace llvm;

MCSymbol *
IA64MCInstLower::GetGlobalAddressSymbol(const MachineOperand &MO) const
{
  llvm_unreachable(__func__);
}

MCSymbol *
IA64MCInstLower::GetExternalSymbolSymbol(const MachineOperand &MO) const
{
  llvm_unreachable(__func__);
}

MCSymbol *
IA64MCInstLower::GetJumpTableSymbol(const MachineOperand &MO) const
{
  llvm_unreachable(__func__);
}

MCSymbol *
IA64MCInstLower::GetConstantPoolIndexSymbol(const MachineOperand &MO) const
{
  llvm_unreachable(__func__);
}

MCSymbol *
IA64MCInstLower::GetBlockAddressSymbol(const MachineOperand &MO) const
{
  llvm_unreachable(__func__);
}

MCOperand
IA64MCInstLower::LowerSymbolOperand(const MachineOperand &MO, MCSymbol *Sym)
      const
{
  llvm_unreachable(__func__);
}

void
IA64MCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) const
{

  OutMI.setOpcode(MI->getOpcode());

  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);

    MCOperand MCOp;
    switch (MO.getType()) {
    case MachineOperand::MO_Register:
      if (MO.isImplicit())
        continue;
      MCOp = MCOperand::CreateReg(MO.getReg());
      break;
    default:
      MI->dump();
      llvm_unreachable(__func__);
    }

    OutMI.addOperand(MCOp);
  }
}
