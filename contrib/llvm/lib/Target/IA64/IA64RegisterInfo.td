
class IA64Register<string name> : Register<name> {
  let Namespace = "IA64";
}

class IA64RegisterClass<list<ValueType> types, int align, list<Register> regs> :
    RegisterClass<"IA64", types, align, regs> {
}

//
// Application registers
//

//
// Branch registers
// -   b0 (aka rp) contains the return address on entry to a function.
// -   b0, b6 & b7 are scratch (aka caller-saved).
// -   b1-b5 are preserved (aka callee-saved).
//
def B0 : IA64Register<"rp">, DwarfRegNum<[320]>;
def B1 : IA64Register<"b1">, DwarfRegNum<[321]>;
def B2 : IA64Register<"b2">, DwarfRegNum<[322]>;
def B3 : IA64Register<"b3">, DwarfRegNum<[323]>;
def B4 : IA64Register<"b4">, DwarfRegNum<[324]>;
def B5 : IA64Register<"b5">, DwarfRegNum<[325]>;
def B6 : IA64Register<"b6">, DwarfRegNum<[326]>;
def B7 : IA64Register<"b7">, DwarfRegNum<[327]>;

def BR : IA64RegisterClass<[i64], 64,
    [B6, B7, B0, B1, B2, B3, B4, B5]>;

//
// Control registers
//

//
// Floating point registers
//
def F0 : IA64Register<"f0">;
def F1 : IA64Register<"f1">;
def F2 : IA64Register<"f2">;
def F3 : IA64Register<"f3">;
def F4 : IA64Register<"f4">;
def F5 : IA64Register<"f5">;
def F8 : IA64Register<"f8">;
def F9 : IA64Register<"f9">;
def F10 : IA64Register<"f10">;
def F11 : IA64Register<"f11">;
def F12 : IA64Register<"f12">;
def F13 : IA64Register<"f13">;
def F14 : IA64Register<"f14">;
def F15 : IA64Register<"f15">;
def F16 : IA64Register<"f16">;
def F17 : IA64Register<"f17">;
def F18 : IA64Register<"f18">;
def F19 : IA64Register<"f19">;
def F20 : IA64Register<"f20">;
def F21 : IA64Register<"f21">;
def F22 : IA64Register<"f22">;
def F23 : IA64Register<"f23">;
def F24 : IA64Register<"f24">;
def F25 : IA64Register<"f25">;
def F26 : IA64Register<"f26">;
def F27 : IA64Register<"f27">;
def F28 : IA64Register<"f28">;
def F29 : IA64Register<"f29">;
def F30 : IA64Register<"f30">;
def F31 : IA64Register<"f31">;

def FR : IA64RegisterClass<[f128], 128,
    [F0, F1, F2, F3, F4, F5, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17,
     F18, F19, F20, F21, F22, F23, F24, F25, F26, F27, F28, F29, F30, F31]>;

//
// General registers
//
def R0 : IA64Register<"r0">, DwarfRegNum<[0]>;
def R1 : IA64Register<"r1">, DwarfRegNum<[1]>;
def R2 : IA64Register<"r2">, DwarfRegNum<[2]>;
def R3 : IA64Register<"r3">, DwarfRegNum<[3]>;
def R4 : IA64Register<"r4">, DwarfRegNum<[4]>;
def R5 : IA64Register<"r5">, DwarfRegNum<[5]>;
def R6 : IA64Register<"r6">, DwarfRegNum<[6]>;
def R7 : IA64Register<"r7">, DwarfRegNum<[7]>;
def R8 : IA64Register<"r8">, DwarfRegNum<[8]>;
def R9 : IA64Register<"r9">, DwarfRegNum<[9]>;
def R10 : IA64Register<"r10">, DwarfRegNum<[10]>;
def R11 : IA64Register<"r11">, DwarfRegNum<[11]>;
def R12 : IA64Register<"r12">, DwarfRegNum<[12]>;
def R13 : IA64Register<"r13">, DwarfRegNum<[13]>;
def R14 : IA64Register<"r14">, DwarfRegNum<[14]>;
def R15 : IA64Register<"r15">, DwarfRegNum<[15]>;
def R16 : IA64Register<"r16">, DwarfRegNum<[16]>;
def R17 : IA64Register<"r17">, DwarfRegNum<[17]>;
def R18 : IA64Register<"r18">, DwarfRegNum<[18]>;
def R19 : IA64Register<"r19">, DwarfRegNum<[19]>;
def R20 : IA64Register<"r20">, DwarfRegNum<[20]>;
def R21 : IA64Register<"r21">, DwarfRegNum<[21]>;
def R22 : IA64Register<"r22">, DwarfRegNum<[22]>;
def R23 : IA64Register<"r23">, DwarfRegNum<[23]>;
def R24 : IA64Register<"r24">, DwarfRegNum<[24]>;
def R25 : IA64Register<"r25">, DwarfRegNum<[25]>;
def R26 : IA64Register<"r26">, DwarfRegNum<[26]>;
def R27 : IA64Register<"r27">, DwarfRegNum<[27]>;
def R28 : IA64Register<"r28">, DwarfRegNum<[28]>;
def R29 : IA64Register<"r29">, DwarfRegNum<[29]>;
def R30 : IA64Register<"r30">, DwarfRegNum<[30]>;
def R31 : IA64Register<"r31">, DwarfRegNum<[31]>;
def R32 : IA64Register<"r32">, DwarfRegNum<[32]>;
def R33 : IA64Register<"r33">, DwarfRegNum<[33]>;
def R34 : IA64Register<"r34">, DwarfRegNum<[34]>;
def R35 : IA64Register<"r35">, DwarfRegNum<[35]>;
def R36 : IA64Register<"r36">, DwarfRegNum<[36]>;
def R37 : IA64Register<"r37">, DwarfRegNum<[37]>;
def R38 : IA64Register<"r38">, DwarfRegNum<[38]>;
def R39 : IA64Register<"r39">, DwarfRegNum<[39]>;

def GR : IA64RegisterClass<[i64], 64,
    [R14, R15, R16, R17, R18, R19, R20, R21, R22, R23, R24, R25, R26, R27,
     R28, R29, R30, R31,
     R8, R9, R10, R11,
     R2, R3,
     R32, R33, R34, R35, R36, R37, R38, R39,
     R4, R5, R6, R7,
     R1, R12, R13, R0]>;

//
// Pregicate registers
//
def P0 : IA64Register<"p0">;
def P1 : IA64Register<"p1">;
def P2 : IA64Register<"p2">;
def P3 : IA64Register<"p3">;
def P4 : IA64Register<"p4">;
def P5 : IA64Register<"p5">;
def P16 : IA64Register<"p16">;
def P17 : IA64Register<"p17">;
def P18 : IA64Register<"p18">;
def P19 : IA64Register<"p19">;
def P20 : IA64Register<"p20">;
def P21 : IA64Register<"p21">;
def P22 : IA64Register<"p22">;
def P23 : IA64Register<"p23">;
def P24 : IA64Register<"p24">;
def P25 : IA64Register<"p25">;
def P26 : IA64Register<"p26">;
def P27 : IA64Register<"p27">;
def P28 : IA64Register<"p28">;
def P29 : IA64Register<"p29">;
def P30 : IA64Register<"p30">;
def P31 : IA64Register<"p31">;
def P32 : IA64Register<"p32">;
def P33 : IA64Register<"p33">;
def P34 : IA64Register<"p34">;
def P35 : IA64Register<"p35">;
def P36 : IA64Register<"p36">;
def P37 : IA64Register<"p37">;
def P38 : IA64Register<"p38">;
def P39 : IA64Register<"p39">;
def P40 : IA64Register<"p40">;
def P41 : IA64Register<"p41">;
def P42 : IA64Register<"p42">;
def P43 : IA64Register<"p43">;
def P44 : IA64Register<"p44">;
def P45 : IA64Register<"p45">;
def P46 : IA64Register<"p46">;
def P47 : IA64Register<"p47">;
def P48 : IA64Register<"p48">;
def P49 : IA64Register<"p49">;
def P50 : IA64Register<"p50">;
def P51 : IA64Register<"p51">;
def P52 : IA64Register<"p52">;
def P53 : IA64Register<"p53">;
def P54 : IA64Register<"p54">;
def P55 : IA64Register<"p55">;
def P56 : IA64Register<"p56">;
def P57 : IA64Register<"p57">;
def P58 : IA64Register<"p58">;
def P59 : IA64Register<"p59">;
def P60 : IA64Register<"p60">;
def P61 : IA64Register<"p61">;
def P62 : IA64Register<"p62">;
def P63 : IA64Register<"p63">;

def P : IA64RegisterClass<[i1], 0,
    [P0, P1, P2, P3, P4, P5, P16, P17, P18, P19, P20, P21, P22, P23, P24, P25,
     P26, P27, P28, P29, P30, P31, P32, P33, P34, P35, P36, P37, P38, P39, P40,
     P41, P42, P43, P44, P45, P46, P47, P48, P49, P50, P51, P52, P53, P54, P55,
     P56, P57, P58, P59, P60, P61, P62, P63]>;
