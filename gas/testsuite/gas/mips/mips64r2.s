# source file to test assembly of mips64r2 instructions
# (assumes that mips32r2 instructions will be tested separately for mips64r2.)

        .set noreorder
	.set noat

	.text
text_label:

      # unprivileged CPU instructions

	# Test macro's ability to turn "dext" into "dext", "dextm" and
	# "dextu" as appropriate.  Also, add some explicit tests of the
	# actual instructions.
	dext	$2, $3, 0, 1	# dext
	dext	$2, $3, 0, 32	# dext
	dext	$2, $3, 0, 33	# dextm
	dext	$2, $3, 0, 64	# dextm
	dext	$2, $3, 31, 1	# dext
	dext	$2, $3, 31, 32	# dext
	dext	$2, $3, 31, 33	# dextm
	dext	$2, $3, 32, 1	# dextu
	dext	$2, $3, 32, 32	# dextu
	dext	$2, $3, 63, 1	# dextu
	dextm	$2, $3, 10, 44
	dextu	$2, $3, 42, 12

	# Test macro's ability to turn "dins" into "dins", "dinsm" and
	# "dinsu" as appropriate.  Also, add some explicit tests of the
	# non-macro instructions.
	dins	$2, $3, 0, 1	# dins
	dins	$2, $3, 0, 32	# dins
	dins	$2, $3, 0, 33	# dinsm
	dins	$2, $3, 0, 64	# dinsm
	dins	$2, $3, 31, 1	# dins
	dins	$2, $3, 31, 2	# dinsm
	dins	$2, $3, 31, 33	# dinsm
	dins	$2, $3, 32, 1	# dinsu
	dins	$2, $3, 32, 32	# dinsu
	dins	$2, $3, 63, 1	# dinsu
	dinsm	$2, $3, 10, 44
	dinsu	$2, $3, 42, 12

	# This file checks that in fact HW rotate will
	# be used for this arch, and checks assembly
	# of the official MIPS mnemonics.  (Note that disassembly
	# uses the traditional "dror", "dror32" and "drorv"
	# mnemonics.) Additional rotate tests are done by rol64-hw.d.
	drotl	$25, $10, 4	# dror32
	drotr	$25, $10, 4	# dror
	drotl	$25, $10, 36	# dror
	drotr	$25, $10, 36	# dror32
	drotl	$25, $10, $4	# neg / drorv
	drotr	$25, $10, $4	# drorv
	drotr32	$25, $10, 4	# dror32
	drotrv	$25, $10, $4	# drorv

	dsbh	$7
	dsbh	$8, $10

	dshd	$7
	dshd	$8, $10

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
