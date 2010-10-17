;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;bit
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	.h8300sx
	.text
_start:
    bset #0x7,r1h               ;7071
    bset #0x7,@er1             ;7d107070
    bset #0x7,@0xffffff12:8            ;7f127070
    bset #0x7,@0x1234:16           ;6a1812347070
    bset #0x7,@0x12345678:32           ;6a38123456787070

    bset r3h,r1h               ;6031
    bset r3h,@er1             ;7d106030
    bset r3h,@0xffffff12:8            ;7f126030
    bset r3h,@0x1234:16           ;6a1812346030
    bset r3h,@0x12345678:32           ;6a38123456786030

    bset/eq #0x7,@er1             ;7d107077
    bset/eq #0x7,@0xffffff12:8            ;7f127077
    bset/eq #0x7,@0x1234:16           ;6a1812347077
    bset/eq #0x7,@0x12345678:32           ;6a38123456787077

    bset/eq r3h,@er1             ;7d106037
    bset/eq r3h,@0xffffff12:8            ;7f126037
    bset/eq r3h,@0x1234:16           ;6a1812346037
    bset/eq r3h,@0x12345678:32           ;6a38123456786037

    bset/ne #0x7,@er1             ;7d107076
    bset/ne #0x7,@0xffffff12:8            ;7f127076
    bset/ne #0x7,@0x1234:16           ;6a1812347076
    bset/ne #0x7,@0x12345678:32           ;6a38123456787076

    bset/ne r3h,@er1             ;7d106036
    bset/ne r3h,@0xffffff12:8            ;7f126036
    bset/ne r3h,@0x1234:16           ;6a1812346036
    bset/ne r3h,@0x12345678:32           ;6a38123456786036

    bnot #0x7,r1h               ;7171
    bnot #0x7,@er1             ;7d107170
    bnot #0x7,@0xffffff12:8            ;7f127170
    bnot #0x7,@0x1234:16           ;6a1812347170
    bnot #0x7,@0x12345678:32           ;6a38123456787170

    bnot r3h,r1h               ;6131
    bnot r3h,@er1             ;7d106130
    bnot r3h,@0xffffff12:8            ;7f126130
    bnot r3h,@0x1234:16           ;6a1812346130
    bnot r3h,@0x12345678:32           ;6a38123456786130

    bclr #0x7,r1h               ;7271
    bclr #0x7,@er1             ;7d107270
    bclr #0x7,@0xffffff12:8            ;7f127270
    bclr #0x7,@0x1234:16           ;6a1812347270
    bclr #0x7,@0x12345678:32           ;6a38123456787270

    bclr r3h,r1h               ;6231
    bclr r3h,@er1             ;7d106230
    bclr r3h,@0xffffff12:8            ;7f126230
    bclr r3h,@0x1234:16           ;6a1812346230
    bclr r3h,@0x12345678:32           ;6a38123456786230

    bclr/eq #0x7,@er1             ;7d107277
    bclr/eq #0x7,@0xffffff12:8            ;7f127277
    bclr/eq #0x7,@0x1234:16           ;6a1812347277
    bclr/eq #0x7,@0x12345678:32           ;6a38123456787277

    bclr/eq r3h,@er1             ;7d106237
    bclr/eq r3h,@0xffffff12:8            ;7f126237
    bclr/eq r3h,@0x1234:16           ;6a1812346237
    bclr/eq r3h,@0x12345678:32           ;6a38123456786237

    bclr/ne #0x7,@er1             ;7d107276
    bclr/ne #0x7,@0xffffff12:8            ;7f127276
    bclr/ne #0x7,@0x1234:16           ;6a1812347276
    bclr/ne #0x7,@0x12345678:32           ;6a38123456787276

    bclr/ne r3h,@er1             ;7d106236
    bclr/ne r3h,@0xffffff12:8            ;7f126236
    bclr/ne r3h,@0x1234:16           ;6a1812346236
    bclr/ne r3h,@0x12345678:32           ;6a38123456786236

    btst #0x7,r1h               ;7371
    btst #0x7,@er1             ;7c107370
    btst #0x7,@0xffffff12:8            ;7e127370
    btst #0x7,@0x1234:16           ;6a1012347370
    btst #0x7,@0x12345678:32           ;6a30123456787370

    btst r3h,r1h               ;6331
    btst r3h,@er1             ;7c106330
    btst r3h,@0xffffff12:8            ;7e126330
    btst r3h,@0x1234:16           ;6a1012346330
    btst r3h,@0x12345678:32           ;6a30123456786330

    bor #0x7,r1h                ;7471
    bor #0x7,@er1              ;7c107470
    bor #0x7,@0xffffff12:8             ;7e127470
    bor #0x7,@0x1234:16            ;6a1012347470
    bor #0x7,@0x12345678:32            ;6a30123456787470

    bior #0x7,r1h               ;74f1
    bior #0x7,@er1             ;7c1074f0
    bior #0x7,@0xffffff12:8            ;7e1274f0
    bior #0x7,@0x1234:16           ;6a10123474f0
    bior #0x7,@0x12345678:32           ;6a301234567874f0

    bxor #0x7,r1h               ;7571
    bxor #0x7,@er1             ;7c107570
    bxor #0x7,@0xffffff12:8            ;7e127570
    bxor #0x7,@0x1234:16           ;6a1012347570
    bxor #0x7,@0x12345678:32           ;6a30123456787570

    bixor #0x7,r1h              ;75f1
    bixor #0x7,@er1            ;7c1075f0
    bixor #0x7,@0xffffff12:8           ;7e1275f0
    bixor #0x7,@0x1234:16          ;6a10123475f0
    bixor #0x7,@0x12345678:32          ;6a301234567875f0

    band #0x7,r1h               ;7671
    band #0x7,@er1             ;7c107670
    band #0x7,@0xffffff12:8            ;7e127670
    band #0x7,@0x1234:16           ;6a1012347670
    band #0x7,@0x12345678:32           ;6a30123456787670

    biand #0x7,r1h              ;76f1
    biand #0x7,@er1            ;7c1076f0
    biand #0x7,@0xffffff12:8           ;7e1276f0
    biand #0x7,@0x1234:16          ;6a10123476f0
    biand #0x7,@0x12345678:32          ;6a301234567876f0

    bld #0x7,r1h                ;7771
    bld #0x7,@er1              ;7c107770
    bld #0x7,@0xffffff12:8             ;7e127770
    bld #0x7,@0x1234:16            ;6a1012347770
    bld #0x7,@0x12345678:32            ;6a30123456787770

    bild #0x7,r1h               ;77f1
    bild #0x7,@er1             ;7c1077f0
    bild #0x7,@0xffffff12:8            ;7e1277f0
    bild #0x7,@0x1234:16           ;6a10123477f0
    bild #0x7,@0x12345678:32           ;6a301234567877f0

    bst #0x7,r1h                ;6771
    bst #0x7,@er1              ;7d106770
    bst #0x7,@0xffffff12:8             ;7f126770
    bst #0x7,@0x1234:16            ;6a1812346770
    bst #0x7,@0x12345678:32            ;6a38123456786770

    bstz #0x7,@er1              ;7d106777
    bstz #0x7,@0xffffff12:8             ;7f126777
    bstz #0x7,@0x1234:16            ;6a1812346777
    bstz #0x7,@0x12345678:32            ;6a38123456786777

    bist #0x7,r1h               ;67f1
    bist #0x7,@er1             ;7d1067f0
    bist #0x7,@0xffffff12:8            ;7f1267f0
    bist #0x7,@0x1234:16           ;6a18123467f0
    bist #0x7,@0x12345678:32           ;6a381234567867f0

    bistz #0x7,@er1             ;7d1067f7
    bistz #0x7,@0xffffff12:8            ;7f1267f7
    bistz #0x7,@0x1234:16           ;6a18123467f7
    bistz #0x7,@0x12345678:32           ;6a381234567867f7

    bfld #0x34:8,@er1,r3h             ;7c10f334
    bfld #0x34:8,@0xffffff12:8,r3h            ;7e12f334
    bfld #0x34:8,@0x1234:16,r3h           ;6a101234f334
    bfld #0x34:8,@0x12345678:32,r3h           ;6a3012345678f334

    bfst r3h,#0x34:8,@er1             ;7d10f334
    bfst r3h,#0x34:8,@0xffffff12:8            ;7f12f334
    bfst r3h,#0x34:8,@0x1234:16           ;6a181234f334
    bfst r3h,#0x34:8,@0x12345678:32           ;6a3812345678f334

	.end
