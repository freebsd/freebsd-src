
class
IA64Instruction<bits<4> op, dag outs, dag ins, string as, list<dag> pat> :
    Instruction
{
  let Namespace = "IA64";
  // let Inst{40-37} = op;
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = as;
  let Pattern= pat;
}

//
// A1-A10
//

class
A1<bits<9> x, dag outs, dag ins, string as, list<dag> pat> :
    IA64Instruction<8, outs, ins, as, pat>
{
  // let Inst{35-27} = x;
}

//
// B1-B9
//

class bwh<bits<2> h>
{
  bits<2> wh = h;
}
def sptk : bwh<0>;
def spnt : bwh<1>;
def dptk : bwh<2>;
def dpnt : bwh<3>;

class
B4<bits<6> x6, bits<3> btype, bwh wh, dag outs, dag ins, string as,
      list<dag> pat> :
    IA64Instruction<0, outs, ins, as, pat>
{
  // let Inst{35} = 0;	// Branch Cache Deallocation Hint (0=none; 1=clr)
  // let Inst{34-33} = wh;
  // let Inst{32-27} = x6;
  // let Inst{12} = 0;	// Sequential Prefetch Hint (0=few; 1=many)
  // let Inst{8-6} = btype;
}

//
// F1-F16
//

//
// I1-I30
//

//
// M1-M48
//

//
// X1-X5
//

