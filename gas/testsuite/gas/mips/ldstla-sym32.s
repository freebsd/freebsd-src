	dla	$4,0xa800000000000000
	dla	$4,0xa800000000000000($3)
	dla	$4,0xffffffff80000000
	dla	$4,0xffffffff80000000($3)
	dla	$4,0x000000007fff7ff8
	dla	$4,0x000000007fff7ff8($3)
	dla	$4,0x000000007ffffff8
	dla	$4,0x000000007ffffff8($3)
	dla	$4,0x123456789abcdef0
	dla	$4,0x123456789abcdef0($3)

	dla	$4,small_comm
	dla	$4,small_comm($3)
	dla	$4,small_comm+3
	dla	$4,small_comm+3($3)

	dla	$4,big_comm
	dla	$4,big_comm($3)
	dla	$4,big_comm+3
	dla	$4,big_comm+3($3)

	dla	$4,small_data
	dla	$4,small_data($3)
	dla	$4,small_data+3
	dla	$4,small_data+3($3)

	dla	$4,big_data
	dla	$4,big_data($3)
	dla	$4,big_data+3
	dla	$4,big_data+3($3)

	dla	$4,extern
	dla	$4,extern($3)
	dla	$4,extern + 0x34000
	dla	$4,extern + 0x34000($3)
	dla	$4,extern - 0x34000
	dla	$4,extern - 0x34000($3)

	lw	$4,0xa800000000000000
	lw	$4,0xa800000000000000($3)
	lw	$4,0xffffffff80000000
	lw	$4,0xffffffff80000000($3)
	lw	$4,0x000000007fff7ff8
	lw	$4,0x000000007fff7ff8($3)
	lw	$4,0x000000007ffffff8
	lw	$4,0x000000007ffffff8($3)
	lw	$4,0x123456789abcdef0
	lw	$4,0x123456789abcdef0($3)

	lw	$4,small_comm
	lw	$4,small_comm($3)
	lw	$4,small_comm+3
	lw	$4,small_comm+3($3)

	lw	$4,big_comm
	lw	$4,big_comm($3)
	lw	$4,big_comm+3
	lw	$4,big_comm+3($3)

	lw	$4,small_data
	lw	$4,small_data($3)
	lw	$4,small_data+3
	lw	$4,small_data+3($3)

	lw	$4,big_data
	lw	$4,big_data($3)
	lw	$4,big_data+3
	lw	$4,big_data+3($3)

	lw	$4,extern
	lw	$4,extern($3)
	lw	$4,extern + 0x34000
	lw	$4,extern + 0x34000($3)
	lw	$4,extern - 0x34000
	lw	$4,extern - 0x34000($3)

	sw	$4,0xa800000000000000
	sw	$4,0xa800000000000000($3)
	sw	$4,0xffffffff80000000
	sw	$4,0xffffffff80000000($3)
	sw	$4,0x000000007fff7ff8
	sw	$4,0x000000007fff7ff8($3)
	sw	$4,0x000000007ffffff8
	sw	$4,0x000000007ffffff8($3)
	sw	$4,0x123456789abcdef0
	sw	$4,0x123456789abcdef0($3)

	sw	$4,small_comm
	sw	$4,small_comm($3)
	sw	$4,small_comm+3
	sw	$4,small_comm+3($3)

	sw	$4,big_comm
	sw	$4,big_comm($3)
	sw	$4,big_comm+3
	sw	$4,big_comm+3($3)

	sw	$4,small_data
	sw	$4,small_data($3)
	sw	$4,small_data+3
	sw	$4,small_data+3($3)

	sw	$4,big_data
	sw	$4,big_data($3)
	sw	$4,big_data+3
	sw	$4,big_data+3($3)

	sw	$4,extern
	sw	$4,extern($3)
	sw	$4,extern + 0x34000
	sw	$4,extern + 0x34000($3)
	sw	$4,extern - 0x34000
	sw	$4,extern - 0x34000($3)

	usw	$4,0xa800000000000000
	usw	$4,0xa800000000000000($3)
	usw	$4,0xffffffff80000000
	usw	$4,0xffffffff80000000($3)
	usw	$4,0x000000007fff7ff8
	usw	$4,0x000000007fff7ff8($3)
	usw	$4,0x000000007ffffff8
	usw	$4,0x000000007ffffff8($3)
	usw	$4,0x123456789abcdef0
	usw	$4,0x123456789abcdef0($3)

	usw	$4,small_comm
	usw	$4,small_comm($3)
	usw	$4,small_comm+3
	usw	$4,small_comm+3($3)

	usw	$4,big_comm
	usw	$4,big_comm($3)
	usw	$4,big_comm+3
	usw	$4,big_comm+3($3)

	usw	$4,small_data
	usw	$4,small_data($3)
	usw	$4,small_data+3
	usw	$4,small_data+3($3)

	usw	$4,big_data
	usw	$4,big_data($3)
	usw	$4,big_data+3
	usw	$4,big_data+3($3)

	usw	$4,extern
	usw	$4,extern($3)
	usw	$4,extern + 0x34000
	usw	$4,extern + 0x34000($3)
	usw	$4,extern - 0x34000
	usw	$4,extern - 0x34000($3)

	.set	nosym32
	dla	$4,extern
	lw	$4,extern
	sw	$4,extern
	usw	$4,extern

	.set	sym32
	dla	$4,extern
	lw	$4,extern
	sw	$4,extern
	usw	$4,extern

	.section	.sdata
	.globl	small_data
small_data:
	.fill	16

	.data
	.globl	big_data
big_data:
	.fill	16

	.comm	small_comm,8
	.comm	big_comm,16
