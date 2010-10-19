	.section .text
# R_M32R_GOTPC24
pic_gotpc:
	bl.s .+4
        ld24 r12,#_GLOBAL_OFFSET_TABLE_
	add r12,lr

# R_M32R_GOTPC_HI_ULO
# R_M32R_GOTPC_HI_SLO
# R_M32R_GOTPC_LO
pic_gotpc_slo:
	bl.s .+4
        seth r12,#shigh(_GLOBAL_OFFSET_TABLE_)
        add3 r12,r12,#low(_GLOBAL_OFFSET_TABLE_+4)
	add r12,lr

pic_gotpc_ulo:
	bl.s .+4
        seth r12,#high(_GLOBAL_OFFSET_TABLE_)
        or3 r12,r12,#low(_GLOBAL_OFFSET_TABLE_+4)
	add r12,lr

# R_M32R_GOT24
pic_got:
	.global sym
	ld24 r0,#sym@GOTOFF

# R_M32R_GOT16_HI_ULO
# R_M32R_GOT16_HI_SLO
# R_M32R_GOT16_LO
pic_got16:
	.global sym2
        seth r12,#shigh(sym2@GOT)
        add3 r12,r12,#low(sym2@GOT)
        seth r12,#high(sym2@GOTOFF)
        or3 r12,r12,#low(sym2@GOT)

# R_M32R_26_PLTREL
pic_plt:
	.global func
	bl func@PLT

# R_M32R_GOTOFF
gotoff:
	ld24 r0,#gotoff@GOTOFF

# R_M32R_GOTOFF_HI_ULO
# R_M32R_GOTOFF_HI_SLO
# R_M32R_GOTOFF_LO
        seth r0,#shigh(gotoff@GOTOFF)
        add3 r0,r0,#low(gotoff@GOTOFF)
        seth r0,#high(gotoff@GOTOFF)
        or3 r0,r0,#low(gotoff@GOTOFF)

	.end
