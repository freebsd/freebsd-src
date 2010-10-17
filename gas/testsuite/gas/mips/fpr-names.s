# source file to test objdump's disassembly using various styles of
# FPR names.

	.set noreorder
	.set noat

	.globl text_label .text
text_label:

	mtc1	$0, $f0
	mtc1	$0, $f1
	mtc1	$0, $f2
	mtc1	$0, $f3
	mtc1	$0, $f4
	mtc1	$0, $f5
	mtc1	$0, $f6
	mtc1	$0, $f7
	mtc1	$0, $f8
	mtc1	$0, $f9
	mtc1	$0, $f10
	mtc1	$0, $f11
	mtc1	$0, $f12
	mtc1	$0, $f13
	mtc1	$0, $f14
	mtc1	$0, $f15
	mtc1	$0, $f16
	mtc1	$0, $f17
	mtc1	$0, $f18
	mtc1	$0, $f19
	mtc1	$0, $f20
	mtc1	$0, $f21
	mtc1	$0, $f22
	mtc1	$0, $f23
	mtc1	$0, $f24
	mtc1	$0, $f25
	mtc1	$0, $f26
	mtc1	$0, $f27
	mtc1	$0, $f28
	mtc1	$0, $f29
	mtc1	$0, $f30
	mtc1	$0, $f31

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
