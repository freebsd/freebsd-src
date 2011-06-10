#include "IA64.h"
#include "IA64InstPrinter.h"

#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

// Include the auto-generated portion of the assembly writer.
#include "IA64GenAsmWriter.inc"

void
IA64InstPrinter::printInst(const MCInst *MI, raw_ostream &O)
{
  printInstruction(MI, O);
}

void
IA64InstPrinter::printPCRelImmOperand(const MCInst *MI, unsigned OpNo,
      raw_ostream &O)
{
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isImm())
    O << Op.getImm();
  else {
    assert(Op.isExpr() && "unknown pcrel immediate operand");
    O << *Op.getExpr();
  }
}

void
IA64InstPrinter::printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O,
      const char *Modifier)
{
  assert((Modifier == 0 || Modifier[0] == 0) && "No modifiers supported");
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    O << getRegisterName(Op.getReg());
  } else if (Op.isImm()) {
    O << '#' << Op.getImm();
  } else {
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    O << '#' << *Op.getExpr();
  }
}

void IA64InstPrinter::printSrcMemOperand(const MCInst *MI, unsigned OpNo,
      raw_ostream &O, const char *Modifier)
{
  const MCOperand &Base = MI->getOperand(OpNo);
  const MCOperand &Disp = MI->getOperand(OpNo+1);

  // Print displacement first

  // If the global address expression is a part of displacement field with a
  // register base, we should not emit any prefix symbol here, e.g.
  //   mov.w &foo, r1
  // vs
  //   mov.w glb(r1), r2
  // Otherwise (!) msp430-as will silently miscompile the output :(
  if (!Base.getReg())
    O << '&';

  if (Disp.isExpr())
    O << *Disp.getExpr();
  else {
    assert(Disp.isImm() && "Expected immediate in displacement field");
    O << Disp.getImm();
  }

  // Print register base field
  if (Base.getReg())
    O << '(' << getRegisterName(Base.getReg()) << ')';
}

void
IA64InstPrinter::printCCOperand(const MCInst *MI, unsigned OpNo,
      raw_ostream &O)
{
  llvm_unreachable(__func__);
}
