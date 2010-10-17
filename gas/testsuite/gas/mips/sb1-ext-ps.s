# source file to test assembly of SB-1 core's paired-single
# extensions to the MIPS64 ISA.

	.set noreorder
	.set noat

	.globl text_label .text
text_label:

	div.ps		$f1, $f2, $f3
	recip.ps	$f1, $f2
	rsqrt.ps	$f1, $f2
	sqrt.ps		$f1, $f2

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
