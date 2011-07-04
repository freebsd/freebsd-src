include "llvm/Target/Target.td"


//
// Subtargets
// 
class IA64Impl<string name, list<SubtargetFeature> features> :
    Processor<name, NoItineraries, features>;

def FeatureLongBranch :
    SubtargetFeature<"lb", "HasLongBranch", "true",
          "Enable the long branch instruction">;

// Itanium 1
def : IA64Impl<"itanium", []>;
def : IA64Impl<"merced", []>;

// Itanium 2
def : IA64Impl<"itanium2", [FeatureLongBranch]>;
def : IA64Impl<"mckinley", [FeatureLongBranch]>;
def : IA64Impl<"madison", [FeatureLongBranch]>;
def : IA64Impl<"montecito", [FeatureLongBranch]>;
def : IA64Impl<"montvale", [FeatureLongBranch]>;

//
// Registers
//
include "IA64RegisterInfo.td"

//
// Instructions
//
include "IA64InstrInfo.td"

//
// Calling Convention
//
include "IA64CallingConv.td"

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
