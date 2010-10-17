# source file to test objdump's disassembly using various styles of
# CP0 register names.

	.set noreorder
	.set noat

	.globl text_label .text
text_label:

	mtc0	$0, $0
	mtc0	$0, $1
	mtc0	$0, $2
	mtc0	$0, $3
	mtc0	$0, $4
	mtc0	$0, $5
	mtc0	$0, $6
	mtc0	$0, $7
	mtc0	$0, $8
	mtc0	$0, $9
	mtc0	$0, $10
	mtc0	$0, $11
	mtc0	$0, $12
	mtc0	$0, $13
	mtc0	$0, $14
	mtc0	$0, $15
	mtc0	$0, $16
	mtc0	$0, $17
	mtc0	$0, $18
	mtc0	$0, $19
	mtc0	$0, $20
	mtc0	$0, $21
	mtc0	$0, $22
	mtc0	$0, $23
	mtc0	$0, $24
	mtc0	$0, $25
	mtc0	$0, $26
	mtc0	$0, $27
	mtc0	$0, $28
	mtc0	$0, $29
	mtc0	$0, $30
	mtc0	$0, $31

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
