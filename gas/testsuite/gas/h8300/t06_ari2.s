;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;arith_2
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.h8300sx
	.text
_start:
    addx.b #0x12:8,r1h          ;9112
    addx.b #0x12:8,@er1         ;7d109012
    addx.b #0x12:8,@er1-        ;01766c189012

    addx.b r3h,r1h             ;0e31
    addx.b r3h,@er1           ;7d100e30
    addx.b r3h,@er1-          ;01766c180e30

    addx.b @er3,r1h           ;7c300e01
    addx.b @er3,@er1         ;0174683d0110

    addx.b @er3-,r1h         ;01766c300e01
    addx.b @er3-,@er1-       ;01766c3da110

    addx.w #0x1234:16,r1         ;015179111234
    addx.w #0x1234:16,@er1        ;7d9179101234
    addx.w #0x1234:16,@er1-       ;01566d1979101234

    addx.w r3,r1             ;01510931
    addx.w r3,@er1           ;7d910930
    addx.w r3,@er1-          ;01566d190930

    addx.w @er3,r1           ;7cb10901
    addx.w @er3,@er1         ;0154693d0110

    addx.w @er3-,r1         ;01566d310901
    addx.w @er3-,@er1-       ;01566d3da110

    addx.l #0x12345678:32,er1        ;01017a1112345678
    addx.l #0x12345678:32,@er1        ;010469197a1012345678
    addx.l #0x12345678:32,@er1-       ;01066d197a1012345678

    addx.l er3,er1           ;01010ab1
    addx.l er3,@er1           ;010469190ab0
    addx.l er3,@er1-          ;01066d190ab0

    addx.l @er3,er1           ;010469310a81
    addx.l @er3,@er1         ;0104693d0110

    addx.l @er3-,er1         ;01066d310a81
    addx.l @er3-,@er1-       ;01066d3da110

    subx.b #0x12:8,r1h          ;b112
    subx.b #0x12:8,@er1         ;7d10b012
    subx.b #0x12:8,@er1-        ;01766c18b012

    subx.b r3h,r1h             ;1e31
    subx.b r3h,@er1           ;7d101e30
    subx.b r3h,@er1-          ;01766c181e30

    subx.b @er3,r1h           ;7c301e01
    subx.b @er3,@er1         ;0174683d0130

    subx.b @er3-,r1h         ;01766c301e01
    subx.b @er3-,@er1-       ;01766c3da130

    subx.w #0x1234:16,r1         ;015179311234
    subx.w #0x1234:16,@er1        ;7d9179301234
    subx.w #0x1234:16,@er1-       ;01566d1979301234

    subx.w r3,r1             ;01511931
    subx.w r3,@er1           ;7d911930
    subx.w r3,@er1-          ;01566d191930

    subx.w @er3,r1           ;7cb11901
    subx.w @er3,@er1         ;0154693d0130

    subx.w @er3-,r1         ;01566d311901
    subx.w @er3-,@er1-       ;01566d3da130

    subx.l #0x12345678:32,er1        ;01017a3112345678
    subx.l #0x12345678:32,@er1        ;010469197a3012345678
    subx.l #0x12345678:32,@er1-       ;01066d197a3012345678

    subx.l er3,er1           ;01011ab1
    subx.l er3,@er1           ;010469191ab0
    subx.l er3,@er1-          ;01066d191ab0

    subx.l @er3,er1           ;010469311a81
    subx.l @er3,@er1         ;0104693d0130

    subx.l @er3-,er1         ;01066d311a81
    subx.l @er3-,@er1-       ;01066d3da130

    inc.b r1h                 ;0a01
    inc.w #1,r1              ;0b51
    inc.w #2,r1              ;0bd1
    inc.l #1,er1              ;0b71
    inc.l #2,er1              ;0bf1

    dec.b r1h                 ;1a01
    dec.w #1,r1              ;1b51
    dec.w #2,r1              ;1bd1
    dec.l #1,er1              ;1b71
    dec.l #2,er1              ;1bf1

    adds.l #1,er1             ;0b01
    adds.l #2,er1             ;0b81
    adds.l #4,er1             ;0b91

    subs.l #1,er1             ;1b01
    subs.l #2,er1             ;1b81
    subs.l #4,er1             ;1b91

    daa.b r1h                 ;0f01

    das.b r1h                 ;1f01

    mulxu.b #0xf:4,r1          ;01cc50f1

    mulxu.b r3h,r1            ;5031

    mulxu.w #0xf:4,er1         ;01cc52f1

    mulxu.w r3,er1           ;5231

    divxu.b #0xf:4,r1          ;01dc51f1

    divxu.b r3h,r1            ;5131

    divxu.w #0xf:4,er1         ;01dc53f1

    divxu.w r3,er1           ;5331

    mulxs.b #0xf:4,r1          ;01c450f1

    mulxs.b r3h,r1            ;01c05031

    mulxs.w #0xf:4,er1         ;01c452f1

    mulxs.w r3,er1           ;01c05231

    divxs.b #0xf:4,r1          ;01d451f1

    divxs.b r3h,r1            ;01d05131

    divxs.w #0xf:4,er1         ;01d453f1

    divxs.w r3,er1           ;01d05331

    mulu.w #0xf:4,r1           ;01ce50f1

    mulu.w r3,r1             ;01ca5031

    mulu.l #0xf:4,er1          ;01ce52f1

    mulu.l er3,er1           ;01ca5231

    mulu/u.l #0xf:4,er1          ;01cf52f1

    mulu/u.l er3,er1           ;01cb5231

    muls.w #0xf:4,r1           ;01c650f1

    muls.w r3,r1             ;01c25031

    muls.l #0xf:4,er1          ;01c652f1

    muls.l er3,er1           ;01c25231

    muls/u.l #0xf:4,er1          ;01c752f1

    muls/u.l er3,er1           ;01c35231

    divu.w #0xf:4,r1           ;01de51f1

    divu.w r3,r1             ;01da5131

    divu.l #0xf:4,er1          ;01de53f1

    divu.l er3,er1            ;01da5331

    divs.w #0xf:4,r1           ;01d651f1

    divs.w r3,r1             ;01d25131

    divs.l #0xf:4,er1          ;01d653f1

    divs.l er3,er1            ;01d25331

	.end
