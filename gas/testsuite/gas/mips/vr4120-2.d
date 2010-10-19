#objdump: -dz --prefix-addresses -m mips:4120
#as: -32 -march=vr4120 -mfix-vr4120
#name: MIPS vr4120 workarounds

.*: +file format .*mips.*

Disassembly of section .text:
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> div	zero,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> div	zero,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> divu	zero,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> divu	zero,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> ddiv	zero,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> ddiv	zero,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> ddivu	zero,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> ddivu	zero,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> dmult	a0,a1
.* <[^>]*> nop
.* <[^>]*> dmult	a2,a3
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> dmultu	a0,a1
.* <[^>]*> nop
.* <[^>]*> dmultu	a2,a3
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> dmacc	a2,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> dmult	a0,a1
.* <[^>]*> nop
.* <[^>]*> dmacc	a2,a3,t0
.* <[^>]*> or	a0,a0,a1
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> mtlo	a3
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> mtlo	a3
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> mthi	a3
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> mthi	a3
#
# vr4181a_md1:
#
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> mult	a0,a1
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> multu	a0,a1
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> dmult	a0,a1
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> dmultu	a0,a1
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> mult	a0,a1
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> multu	a0,a1
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> dmult	a0,a1
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> nop
.* <[^>]*> dmultu	a0,a1
.* <[^>]*> or	a0,a0,a1
#
# vr4181a_md4:
#
.* <[^>]*> dmult	a0,a1
.* <[^>]*> nop
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> dmultu	a0,a1
.* <[^>]*> nop
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> div	zero,a0,a1
.* <[^>]*> nop
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> divu	zero,a0,a1
.* <[^>]*> nop
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> ddiv	zero,a0,a1
.* <[^>]*> nop
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> ddivu	zero,a0,a1
.* <[^>]*> nop
.* <[^>]*> macc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> dmult	a0,a1
.* <[^>]*> nop
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> dmultu	a0,a1
.* <[^>]*> nop
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> div	zero,a0,a1
.* <[^>]*> nop
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> divu	zero,a0,a1
.* <[^>]*> nop
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> ddiv	zero,a0,a1
.* <[^>]*> nop
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#
.* <[^>]*> ddivu	zero,a0,a1
.* <[^>]*> nop
.* <[^>]*> dmacc	a0,a1,a2
.* <[^>]*> or	a0,a0,a1
#...
