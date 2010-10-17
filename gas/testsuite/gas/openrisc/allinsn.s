 .data
foodata: .word 42
 .text
footext:
	.text
	.global l_j
l_j:
	l.j footext
	.text
	.global l_jal
l_jal:
	l.jal footext
	.text
	.global l_jr
l_jr:
	l.jr r0
	.text
	.global l_jalr
l_jalr:
	l.jalr r0
	.text
	.global l_bal
l_bal:
	l.bal footext
	.text
	.global l_bnf
l_bnf:
	l.bnf footext
	.text
	.global l_bf
l_bf:
	l.bf footext
	.text
	.global l_brk
l_brk:
	l.brk 0
	.text
	.global l_rfe
l_rfe:
	l.rfe r0
	.text
	.global l_sys
l_sys:
	l.sys 0
	.text
	.global l_nop
l_nop:
	l.nop
	.text
	.global l_movhi
l_movhi:
	l.movhi r0,0
	.text
	.global l_mfsr
l_mfsr:
	l.mfsr r0,r0
	.text
	.global l_mtsr
l_mtsr:
	l.mtsr r0,r0
	.text
	.global l_lw
l_lw:
	l.lw r0,0(r0)
	.text
	.global l_lbz
l_lbz:
	l.lbz r0,0(r0)
	.text
	.global l_lbs
l_lbs:
	l.lbs r0,0(r0)
	.text
	.global l_lhz
l_lhz:
	l.lhz r0,0(r0)
	.text
	.global l_lhs
l_lhs:
	l.lhs r0,0(r0)
	.text
	.global l_sw
l_sw:
	l.sw 0(r0),r0
	.text
	.global l_sb
l_sb:
	l.sb 0(r0),r0
	.text
	.global l_sh
l_sh:
	l.sh 0(r0),r0
	.text
	.global l_sll
l_sll:
	l.sll r0,r0,r0
	.text
	.global l_slli
l_slli:
	l.slli r0,r0,0
	.text
	.global l_srl
l_srl:
	l.srl r0,r0,r0
	.text
	.global l_srli
l_srli:
	l.srli r0,r0,0
	.text
	.global l_sra
l_sra:
	l.sra r0,r0,r0
	.text
	.global l_srai
l_srai:
	l.srai r0,r0,0
	.text
	.global l_ror
l_ror:
	l.ror r0,r0,r0
	.text
	.global l_rori
l_rori:
	l.rori r0,r0,0
	.text
	.global l_add
l_add:
	l.add r0,r0,r0
	.text
	.global l_addi
l_addi:
	l.addi r0,r0,0
	.text
	.global l_sub
l_sub:
	l.sub r0,r0,r0
	.text
	.global l_subi
l_subi:
	l.subi r0,r0,0
	.text
	.global l_and
l_and:
	l.and r0,r0,r0
	.text
	.global l_andi
l_andi:
	l.andi r0,r0,0
	.text
	.global l_or
l_or:
	l.or r0,r0,r0
	.text
	.global l_ori
l_ori:
	l.ori r0,r0,0
	.text
	.global l_xor
l_xor:
	l.xor r0,r0,r0
	.text
	.global l_xori
l_xori:
	l.xori r0,r0,0
	.text
	.global l_mul
l_mul:
	l.mul r0,r0,r0
	.text
	.global l_muli
l_muli:
	l.muli r0,r0,0
	.text
	.global l_div
l_div:
	l.div r0,r0,r0
	.text
	.global l_divu
l_divu:
	l.divu r0,r0,r0
	.text
	.global l_sfgts
l_sfgts:
	l.sfgts r0,r0
	.text
	.global l_sfgtu
l_sfgtu:
	l.sfgtu r0,r0
	.text
	.global l_sfges
l_sfges:
	l.sfges r0,r0
	.text
	.global l_sfgeu
l_sfgeu:
	l.sfgeu r0,r0
	.text
	.global l_sflts
l_sflts:
	l.sflts r0,r0
	.text
	.global l_sfltu
l_sfltu:
	l.sfltu r0,r0
	.text
	.global l_sfles
l_sfles:
	l.sfles r0,r0
	.text
	.global l_sfleu
l_sfleu:
	l.sfleu r0,r0
	.text
	.global l_sfgtsi
l_sfgtsi:
	l.sfgtsi r0,0
	.text
	.global l_sfgtui
l_sfgtui:
	l.sfgtui r0,0
	.text
	.global l_sfgesi
l_sfgesi:
	l.sfgesi r0,0
	.text
	.global l_sfgeui
l_sfgeui:
	l.sfgeui r0,0
	.text
	.global l_sfltsi
l_sfltsi:
	l.sfltsi r0,0
	.text
	.global l_sfltui
l_sfltui:
	l.sfltui r0,0
	.text
	.global l_sflesi
l_sflesi:
	l.sflesi r0,0
	.text
	.global l_sfleui
l_sfleui:
	l.sfleui r0,0
	.text
	.global l_sfeq
l_sfeq:
	l.sfeq r0,r0
	.text
	.global l_sfeqi
l_sfeqi:
	l.sfeqi r0,0
	.text
	.global l_sfne
l_sfne:
	l.sfne r0,r0
	.text
	.global l_sfnei
l_sfnei:
	l.sfnei r0,0
