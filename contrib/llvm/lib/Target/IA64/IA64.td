include "llvm/Target/Target.td"

//
// Subtargets
// 
class IA64Impl<string name, list<SubtargetFeature> features> :
    Processor<name, NoItineraries, features>;

def : IA64Impl<"merced", []>;
def : IA64Impl<"mckinley", []>;

//
// Registers
//
class IA64Register<string name> : Register<name> {
  let Namespace = "IA64";
}

def F0 : IA64Register<"f0">;
def F1 : IA64Register<"f1">;

def P0 : IA64Register<"p0">;

def R0 : IA64Register<"r0">;

//
// Register classes
//
class IA64RegisterClass<list<ValueType> types, int align, list<Register> regs>
  : RegisterClass<"IA64", types, align, regs> {
}

def FloatingPoint : IA64RegisterClass<[f128], 128, [F0, F1]>;
def General : IA64RegisterClass<[i64], 64, [R0]>;
def Predicate : IA64RegisterClass<[i1], 0, [P0]>;

//
// Instructions
//
class IA64Instruction<dag outs, dag ins> : Instruction {
  let Namespace = "IA64";
  let OutOperandList = outs;
  let InOperandList = ins;
}

def NOP : IA64Instruction<(outs), (ins)>;

def IA64InstrInfo : InstrInfo;

//
// Machine code and assembly writer
//
def IA64InstPrinter : AsmWriter {
  string AsmWriterClassName  = "InstPrinter";
  bit isMCAsmWriter = 1;
}

//
// Target for the Itanium Processor Family (IPF)
//
def IA64 : Target {
  let InstructionSet = IA64InstrInfo;
  let AssemblyWriters = [IA64InstPrinter];
}
