# source file to test assembly of mips32r2 instructions

        .set noreorder
	.set noat

	.text
text_label:

      # unprivileged CPU instructions

	ehb

	ext	$4, $5, 6, 8

	ins	$4, $5, 6, 8

	jalr.hb	$8
	jalr.hb $20, $9

	jr.hb	$8

	# Note, further testing of rdhwr is done in hwr-names-mips32r2.d
	rdhwr	$10, $0
	rdhwr	$11, $1
	rdhwr	$12, $2
	rdhwr	$13, $3
	rdhwr	$14, $4
	rdhwr	$15, $5

	# This file checks that in fact HW rotate will
	# be used for this arch, and checks assembly
	# of the official MIPS mnemonics.  (Note that disassembly
	# uses the traditional "ror" and "rorv" mnemonics.)
	# Additional rotate tests are done by rol-hw.d.
	rotl	$25, $10, 4
	rotr	$25, $10, 4
	rotl	$25, $10, $4
	rotr	$25, $10, $4
	rotrv	$25, $10, $4

	seb	$7
	seb	$8, $10

	seh	$7
	seh	$8, $10

	synci	0x5555($10)

	wsbh	$7
	wsbh	$8, $10

      # cp0 instructions

	di
	di	$0
	di	$10

	ei
	ei	$0
	ei	$10

	rdpgpr	$10, $25

	wrpgpr	$10, $25

      # FPU (cp1) instructions
      #
      # Even registers are supported w/ 32-bit FPU, odd
      # registers supported only for 64-bit FPU.
      # Only the 32-bit FPU instructions are tested here.
     
	mfhc1	$17, $f0
	mthc1	$17, $f0

      # cp2 instructions

	mfhc2	$17, 0x5555
	mthc2	$17, 0x5555

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
