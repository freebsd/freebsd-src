# source file to test objdump's disassembly using various styles of
# HWR (hardware register) names.

	.set noreorder
	.set noat

	.globl text_label .text
text_label:

	rdhwr	$0, $0
	rdhwr	$0, $1
	rdhwr	$0, $2
	rdhwr	$0, $3
	rdhwr	$0, $4
	rdhwr	$0, $5
	rdhwr	$0, $6
	rdhwr	$0, $7
	rdhwr	$0, $8
	rdhwr	$0, $9
	rdhwr	$0, $10
	rdhwr	$0, $11
	rdhwr	$0, $12
	rdhwr	$0, $13
	rdhwr	$0, $14
	rdhwr	$0, $15
	rdhwr	$0, $16
	rdhwr	$0, $17
	rdhwr	$0, $18
	rdhwr	$0, $19
	rdhwr	$0, $20
	rdhwr	$0, $21
	rdhwr	$0, $22
	rdhwr	$0, $23
	rdhwr	$0, $24
	rdhwr	$0, $25
	rdhwr	$0, $26
	rdhwr	$0, $27
	rdhwr	$0, $28
	rdhwr	$0, $29
	rdhwr	$0, $30
	rdhwr	$0, $31

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
      .space  8
