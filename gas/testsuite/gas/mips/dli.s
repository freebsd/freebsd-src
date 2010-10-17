# Source file used to test the dli macro.
	
foo:	
	dli	$4,0
	dli	$4,1
	dli	$4,-1
	dli	$4,0x8000
	dli	$4,-0x8000
	dli	$4,0x10000
	dli	$4,0x1a5a5
	dli	$4,0x80001234
	dli	$4,0xffffffff
	dli	$4,0x00000000ffffffff
	dli	$4,0xffffffffffffffff
	dli	$4,0x000fffffffffffff
	dli	$4,0xffffffff80001234
	dli	$4,0xffff800012345678
	dli	$4,0x8000123456780000
	dli	$4,0xffffffffffff8765
	dli	$4,0xffffffffffff4321

	dli	$4,0xfffffffffffffff0
	dli	$4,0xffffffffffffff00
	dli	$4,0xfffffffffffff000
	dli	$4,0xffffffffffff0000
	dli	$4,0xfffffffffff00000
	dli	$4,0xffffffffff000000
	dli	$4,0xfffffffff0000000
	dli	$4,0xffffffff00000000
	dli	$4,0xfffffff000000000
	dli	$4,0xffffff0000000000
	dli	$4,0xfffff00000000000
	dli	$4,0xffff000000000000
	dli	$4,0xfff0000000000000
	dli	$4,0xff00000000000000
	dli	$4,0xf000000000000000

	dli	$4,0x0fffffffffffffff
	dli	$4,0x00ffffffffffffff
	dli	$4,0x000fffffffffffff
	dli	$4,0x0000ffffffffffff
	dli	$4,0x00000fffffffffff
	dli	$4,0x000000ffffffffff
	dli	$4,0x0000000fffffffff
	dli	$4,0x00000000ffffffff
	dli	$4,0x000000000fffffff
	dli	$4,0x0000000000ffffff
	dli	$4,0x00000000000fffff
	dli	$4,0x000000000000ffff
	dli	$4,0x0000000000000fff
	dli	$4,0x00000000000000ff
	dli	$4,0x000000000000000f

	dli	$4,0x000000000003fffc
	dli	$4,0x00003fffc0000000
	dli	$4,0x0003fffc00000000
	dli	$4,0x003fffc000000000
	dli	$4,0x003fffffffc00000
	dli	$4,0x003ffffffffc0000
	dli	$4,0x003fffffffffc000

	dli	$4,0x003ffc03ffffc000

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	nop
	nop
