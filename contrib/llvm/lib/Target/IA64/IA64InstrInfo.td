
include "IA64InstrFormats.td"

def retflag : SDNode<"IA64ISD::RET_FLAG", SDTNone,
    [SDNPHasChain, SDNPOptInGlue]>;

def ADD : A1<0, (outs GR:$r1), (ins GR:$r2, GR:$r3),
    "add\t$r1=$r2,$r3",
    [(set GR:$r1, (add GR:$r2, GR:$r3))]>;

def BR_RET : B4<21, 4, sptk, (outs), (ins),
    "br.ret.sptk\trp",  [(retflag)]>
{
  let isReturn = 1;
  let isTerminator = 1;
  let isBarrier = 1;
}

def NOP : IA64Instruction<0, (outs), (ins), "nop", []>;

