def RetCC_IA64 : CallingConv<[
  CCIfType<[i64], CCAssignToReg<[R8]>>,

  // XXX i128 is returned in the pair {r8,r9}. Custom rule needed?

  // FP values are returned in the register format, but rounded to
  // the appropriate precision.
  CCIfType<[f32, f64, f80], CCAssignToReg<[F8]>>

  // XXX f128 is returned in the pair {f8,f9}. Custom rule needed?

  // XXX HFAs (Momogeneous FP aggregates) up to 8 elements or fields are
  // treated specially. Each element or field is assigned to a FP register
  // (in order) from the set [f8, f9, f10, f11, f12, f13, f14, f15].

  // XXX Aggregates up to 256 bits in size are assigned to registers (in
  // order) from the set [r8, r9, r10, r11].

  // Aggregates larger than 256 bits are returned in memory.
]>;

def CC_IA64 : CallingConv<[
  // XXX There's a 1-to-1 mapping between general registers and slots.
  // As such, if an argument is to be passed in a FP register, the
  // general register corresponding to that slot cannot be used anymore.
  // This isn't handled (yet).
  CCIfType<[i64], CCAssignToReg<[R32, R33, R34, R35, R36, R37, R38, R39]>>,

  // XXX i128 is assigned 2 consecutive registers/slots, aligned at the
  // next even slot.

  // FP registers are assigned in sequence. For the vararg case, this
  // works differently. This isn't handled (yet).
  CCIfNotVarArg<CCIfType<[f32, f64],
        CCAssignToReg<[F8, F9, F10, F11, F12, F13, F14, F15]>>>,

  // XXX f80 and f128 occupy 2 consecutive slots, aligned at the
  // next even slot.

  CCIfType<[i64, f32, f64], CCAssignToStack<8, 8>>,
  CCIfType<[f80, f128], CCAssignToStack<16, 16>>
]>;
