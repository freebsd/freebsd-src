# Source file used to test the ld macro.
	
	.set	mips1

	.data
data_label:
	.extern big_external_data_label,1000
	.extern small_external_data_label,1
	.comm big_external_common,1000
	.comm small_external_common,1
	.lcomm big_local_common,1000
	.lcomm small_local_common,1
	
	.text
	ld	$4,0
	ld	$4,1
	ld	$4,0x8000
	ld	$4,-0x8000
	ld	$4,0x10000
	ld	$4,0x1a5a5
	ld	$4,0($5)
	ld	$4,1($5)
	ld	$4,0x8000($5)
	ld	$4,-0x8000($5)
	ld	$4,0x10000($5)
	ld	$4,0x1a5a5($5)
	ld	$4,data_label
	ld	$4,big_external_data_label
	ld	$4,small_external_data_label
	ld	$4,big_external_common
	ld	$4,small_external_common
	ld	$4,big_local_common
	ld	$4,small_local_common
	ld	$4,data_label+1
	ld	$4,big_external_data_label+1
	ld	$4,small_external_data_label+1
	ld	$4,big_external_common+1
	ld	$4,small_external_common+1
	ld	$4,big_local_common+1
	ld	$4,small_local_common+1
	ld	$4,data_label+0x8000
	ld	$4,big_external_data_label+0x8000
	ld	$4,small_external_data_label+0x8000
	ld	$4,big_external_common+0x8000
	ld	$4,small_external_common+0x8000
	ld	$4,big_local_common+0x8000
	ld	$4,small_local_common+0x8000
	ld	$4,data_label-0x8000
	ld	$4,big_external_data_label-0x8000
	ld	$4,small_external_data_label-0x8000
	ld	$4,big_external_common-0x8000
	ld	$4,small_external_common-0x8000
	ld	$4,big_local_common-0x8000
	ld	$4,small_local_common-0x8000
	ld	$4,data_label+0x10000
	ld	$4,big_external_data_label+0x10000
	ld	$4,small_external_data_label+0x10000
	ld	$4,big_external_common+0x10000
	ld	$4,small_external_common+0x10000
	ld	$4,big_local_common+0x10000
	ld	$4,small_local_common+0x10000
	ld	$4,data_label+0x1a5a5
	ld	$4,big_external_data_label+0x1a5a5
	ld	$4,small_external_data_label+0x1a5a5
	ld	$4,big_external_common+0x1a5a5
	ld	$4,small_external_common+0x1a5a5
	ld	$4,big_local_common+0x1a5a5
	ld	$4,small_local_common+0x1a5a5
	ld	$4,data_label($5)
	ld	$4,big_external_data_label($5)
	ld	$4,small_external_data_label($5)
	ld	$4,big_external_common($5)
	ld	$4,small_external_common($5)
	ld	$4,big_local_common($5)
	ld	$4,small_local_common($5)
	ld	$4,data_label+1($5)
	ld	$4,big_external_data_label+1($5)
	ld	$4,small_external_data_label+1($5)
	ld	$4,big_external_common+1($5)
	ld	$4,small_external_common+1($5)
	ld	$4,big_local_common+1($5)
	ld	$4,small_local_common+1($5)
	ld	$4,data_label+0x8000($5)
	ld	$4,big_external_data_label+0x8000($5)
	ld	$4,small_external_data_label+0x8000($5)
	ld	$4,big_external_common+0x8000($5)
	ld	$4,small_external_common+0x8000($5)
	ld	$4,big_local_common+0x8000($5)
	ld	$4,small_local_common+0x8000($5)
	ld	$4,data_label-0x8000($5)
	ld	$4,big_external_data_label-0x8000($5)
	ld	$4,small_external_data_label-0x8000($5)
	ld	$4,big_external_common-0x8000($5)
	ld	$4,small_external_common-0x8000($5)
	ld	$4,big_local_common-0x8000($5)
	ld	$4,small_local_common-0x8000($5)
	ld	$4,data_label+0x10000($5)
	ld	$4,big_external_data_label+0x10000($5)
	ld	$4,small_external_data_label+0x10000($5)
	ld	$4,big_external_common+0x10000($5)
	ld	$4,small_external_common+0x10000($5)
	ld	$4,big_local_common+0x10000($5)
	ld	$4,small_local_common+0x10000($5)
	ld	$4,data_label+0x1a5a5($5)
	ld	$4,big_external_data_label+0x1a5a5($5)
	ld	$4,small_external_data_label+0x1a5a5($5)
	ld	$4,big_external_common+0x1a5a5($5)
	ld	$4,small_external_common+0x1a5a5($5)
	ld	$4,big_local_common+0x1a5a5($5)
	ld	$4,small_local_common+0x1a5a5($5)

# l.d and s.d are sort of like ld.
	l.d	$f4,0
	l.d	$f4,1
	l.d	$f4,0x8000
	l.d	$f4,-0x8000
	l.d	$f4,0($5)
	l.d	$f4,1($5)
	l.d	$f4,0x8000($5)
	l.d	$f4,-0x8000($5)
	l.d	$f4,small_external_common+0x1a5a5($5)
	# Little endian will insert a nop here.
	# We put it in explicitly so that big and little endian are similar.
	nop
	s.d	$f4,0
	s.d	$f4,1
	s.d	$f4,0x8000
	s.d	$f4,-0x8000
	s.d	$f4,0($5)
	s.d	$f4,1($5)
	s.d	$f4,0x8000($5)
	s.d	$f4,-0x8000($5)
	s.d	$f4,big_external_common+0x1a5a5($5)

# sd is handled like ld.  Sanity check it.
	sd	$4,0

# Sanity check the -mips3 versions
	.set	mips3
	ld	$4,big_local_common+0x1a5a5($5)
	sd	$4,small_local_common+0x1a5a5($5)

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
