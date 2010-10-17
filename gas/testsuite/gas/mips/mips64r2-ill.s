# source file to test illegal mips64r2 instructions

        .set noreorder
      .set noat

      .text
text_label:

      # dext macro position/size checks

	# constraint: 0 <= pos < 64
	dext	$4, $5, -1, 1		# error (position)
	dext	$4, $5, 0, 1
	dext	$4, $5, 63, 1
	dext	$4, $5, 64, 1		# error (position)

	# constraint: 0 < size <= 64
	dext	$4, $5, 0, 0		# error (size)
	dext	$4, $5, 0, 1
	dext	$4, $5, 0, 64
	dext	$4, $5, 0, 65		# error (size)

	# constraint: 0 < (pos+size) <= 64
	dext	$4, $5, 0, 1
	dext	$4, $5, 0, 2
	dext	$4, $5, 0, 63
	dext	$4, $5, 0, 64
	dext	$4, $5, 1, 1
	dext	$4, $5, 1, 2
	dext	$4, $5, 1, 63
	dext	$4, $5, 1, 64		# error (size)
	dext	$4, $5, 63, 1
	dext	$4, $5, 63, 2		# error (size)
	dext	$4, $5, 63, 63		# error (size)
	dext	$4, $5, 63, 64		# error (size)

      # dextm instruction position/size checks

	# constraint: 0 <= pos < 32
	dextm	$4, $5, -1, 33		# error (position)
	dextm	$4, $5, 0, 33
	dextm	$4, $5, 31, 33
	dextm	$4, $5, 32, 33		# error (position)

	# constraint: 32 < size <= 64
	dextm	$4, $5, 0, 32		# error (size)
	dextm	$4, $5, 0, 33
	dextm	$4, $5, 0, 64
	dextm	$4, $5, 0, 65		# error (size)

	# constraint: 32 < (pos+size) <= 64
	dextm	$4, $5, 0, 33
	dextm	$4, $5, 0, 34
	dextm	$4, $5, 0, 63
	dextm	$4, $5, 0, 64
	dextm	$4, $5, 1, 33
	dextm	$4, $5, 1, 34
	dextm	$4, $5, 1, 63
	dextm	$4, $5, 1, 64		# error (size)
	dextm	$4, $5, 31, 33
	dextm	$4, $5, 31, 34		# error (size)
	dextm	$4, $5, 31, 63		# error (size)
	dextm	$4, $5, 31, 64		# error (size)

      # dextu instruction position/size checks

	# constraint: 32 <= pos < 64
	dextu	$4, $5, 31, 1		# error (position)
	dextu	$4, $5, 32, 1
	dextu	$4, $5, 63, 1
	dextu	$4, $5, 64, 1		# error (position)

	# constraint: 0 < size <= 32
	dextu	$4, $5, 32, 0		# error (size)
	dextu	$4, $5, 32, 1
	dextu	$4, $5, 32, 32
	dextu	$4, $5, 32, 33		# error (size)

	# constraint: 32 < (pos+size) <= 64
	dextu	$4, $5, 32, 1
	dextu	$4, $5, 32, 2
	dextu	$4, $5, 32, 31
	dextu	$4, $5, 32, 32
	dextu	$4, $5, 33, 1
	dextu	$4, $5, 33, 2
	dextu	$4, $5, 33, 31
	dextu	$4, $5, 33, 32		# error (size)
	dextu	$4, $5, 63, 1
	dextu	$4, $5, 63, 2		# error (size)
	dextu	$4, $5, 63, 31		# error (size)
	dextu	$4, $5, 63, 32		# error (size)

      # dins macro position/size checks

	# constraint: 0 <= pos < 64
	dins	$4, $5, -1, 1		# error (position)
	dins	$4, $5, 0, 1
	dins	$4, $5, 63, 1
	dins	$4, $5, 64, 1		# error (position)

	# constraint: 0 < size <= 64
	dins	$4, $5, 0, 0		# error (size)
	dins	$4, $5, 0, 1
	dins	$4, $5, 0, 64
	dins	$4, $5, 0, 65		# error (size)

	# constraint: 0 < (pos+size) <= 64
	dins	$4, $5, 0, 1
	dins	$4, $5, 0, 2
	dins	$4, $5, 0, 63
	dins	$4, $5, 0, 64
	dins	$4, $5, 1, 1
	dins	$4, $5, 1, 2
	dins	$4, $5, 1, 63
	dins	$4, $5, 1, 64		# error (size)
	dins	$4, $5, 63, 1
	dins	$4, $5, 63, 2		# error (size)
	dins	$4, $5, 63, 63		# error (size)
	dins	$4, $5, 63, 64		# error (size)

      # dinsm instruction position/size checks

	# constraint: 0 <= pos < 32
	dinsm	$4, $5, -1, 33		# error (position)
	dinsm	$4, $5, 0, 33
	dinsm	$4, $5, 31, 33
	dinsm	$4, $5, 32, 33		# error (position)

	# constraint: 2 <= size <= 64
	dinsm	$4, $5, 31, 1		# error (size)
	dinsm	$4, $5, 31, 2
	dinsm	$4, $5, 0, 64
	dinsm	$4, $5, 0, 65		# error (size)

	# constraint: 32 < (pos+size) <= 64
	dinsm	$4, $5, 0, 2		# error (size)
	dinsm	$4, $5, 0, 3		# error (size)
	dinsm	$4, $5, 0, 63
	dinsm	$4, $5, 0, 64
	dinsm	$4, $5, 1, 2		# error (size)
	dinsm	$4, $5, 1, 3		# error (size)
	dinsm	$4, $5, 1, 63
	dinsm	$4, $5, 1, 64		# error (size)
	dinsm	$4, $5, 30, 2		# error (size)
	dinsm	$4, $5, 30, 3
	dinsm	$4, $5, 30, 63		# error (size)
	dinsm	$4, $5, 30, 64		# error (size)
	dinsm	$4, $5, 31, 2
	dinsm	$4, $5, 31, 3
	dinsm	$4, $5, 31, 63		# error (size)
	dinsm	$4, $5, 31, 64		# error (size)

      # dinsu instruction position/size checks

	# constraint: 32 <= pos < 64
	dinsu	$4, $5, 31, 1		# error (position)
	dinsu	$4, $5, 32, 1
	dinsu	$4, $5, 63, 1
	dinsu	$4, $5, 64, 1		# error (position)

	# constraint: 1 <= size <= 32
	dinsu	$4, $5, 32, 0		# error (size)
	dinsu	$4, $5, 32, 1
	dinsu	$4, $5, 32, 32
	dinsu	$4, $5, 32, 33		# error (size)

	# constraint: 32 < (pos+size) <= 64
	dinsu	$4, $5, 32, 1
	dinsu	$4, $5, 32, 2
	dinsu	$4, $5, 32, 31
	dinsu	$4, $5, 32, 32
	dinsu	$4, $5, 33, 1
	dinsu	$4, $5, 33, 2
	dinsu	$4, $5, 33, 31
	dinsu	$4, $5, 33, 32		# error (size)
	dinsu	$4, $5, 62, 1
	dinsu	$4, $5, 62, 2
	dinsu	$4, $5, 62, 31		# error (size)
	dinsu	$4, $5, 62, 32		# error (size)
	dinsu	$4, $5, 63, 1
	dinsu	$4, $5, 63, 2		# error (size)
	dinsu	$4, $5, 63, 31		# error (size)
	dinsu	$4, $5, 63, 32		# error (size)

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
