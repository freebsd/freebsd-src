# Source file used to test the lb macro.
	
	.data
data_label:
	.extern big_external_data_label,1000
	.extern small_external_data_label,1
	.comm big_external_common,1000
	.comm small_external_common,1
	.lcomm big_local_common,1000
	.lcomm small_local_common,1
	
	.text
	lb	$4,0
	lb	$4,1
	lb	$4,0x8000
	lb	$4,-0x8000
	lb	$4,0x10000
	lb	$4,0x1a5a5
	lb	$4,0($5)
	lb	$4,1($5)
	lb	$4,0x8000($5)
	lb	$4,-0x8000($5)
	lb	$4,0x10000($5)
	lb	$4,0x1a5a5($5)
	lb	$4,data_label
	lb	$4,big_external_data_label
	lb	$4,small_external_data_label
	lb	$4,big_external_common
	lb	$4,small_external_common
	lb	$4,big_local_common
	lb	$4,small_local_common
	lb	$4,data_label+1
	lb	$4,big_external_data_label+1
	lb	$4,small_external_data_label+1
	lb	$4,big_external_common+1
	lb	$4,small_external_common+1
	lb	$4,big_local_common+1
	lb	$4,small_local_common+1
	lb	$4,data_label+0x8000
	lb	$4,big_external_data_label+0x8000
	lb	$4,small_external_data_label+0x8000
	lb	$4,big_external_common+0x8000
	lb	$4,small_external_common+0x8000
	lb	$4,big_local_common+0x8000
	lb	$4,small_local_common+0x8000
	lb	$4,data_label-0x8000
	lb	$4,big_external_data_label-0x8000
	lb	$4,small_external_data_label-0x8000
	lb	$4,big_external_common-0x8000
	lb	$4,small_external_common-0x8000
	lb	$4,big_local_common-0x8000
	lb	$4,small_local_common-0x8000
	lb	$4,data_label+0x10000
	lb	$4,big_external_data_label+0x10000
	lb	$4,small_external_data_label+0x10000
	lb	$4,big_external_common+0x10000
	lb	$4,small_external_common+0x10000
	lb	$4,big_local_common+0x10000
	lb	$4,small_local_common+0x10000
	lb	$4,data_label+0x1a5a5
	lb	$4,big_external_data_label+0x1a5a5
	lb	$4,small_external_data_label+0x1a5a5
	lb	$4,big_external_common+0x1a5a5
	lb	$4,small_external_common+0x1a5a5
	lb	$4,big_local_common+0x1a5a5
	lb	$4,small_local_common+0x1a5a5
	lb	$4,data_label($5)
	lb	$4,big_external_data_label($5)
	lb	$4,small_external_data_label($5)
	lb	$4,big_external_common($5)
	lb	$4,small_external_common($5)
	lb	$4,big_local_common($5)
	lb	$4,small_local_common($5)
	lb	$4,data_label+1($5)
	lb	$4,big_external_data_label+1($5)
	lb	$4,small_external_data_label+1($5)
	lb	$4,big_external_common+1($5)
	lb	$4,small_external_common+1($5)
	lb	$4,big_local_common+1($5)
	lb	$4,small_local_common+1($5)
	lb	$4,data_label+0x8000($5)
	lb	$4,big_external_data_label+0x8000($5)
	lb	$4,small_external_data_label+0x8000($5)
	lb	$4,big_external_common+0x8000($5)
	lb	$4,small_external_common+0x8000($5)
	lb	$4,big_local_common+0x8000($5)
	lb	$4,small_local_common+0x8000($5)
	lb	$4,data_label-0x8000($5)
	lb	$4,big_external_data_label-0x8000($5)
	lb	$4,small_external_data_label-0x8000($5)
	lb	$4,big_external_common-0x8000($5)
	lb	$4,small_external_common-0x8000($5)
	lb	$4,big_local_common-0x8000($5)
	lb	$4,small_local_common-0x8000($5)
	lb	$4,data_label+0x10000($5)
	lb	$4,big_external_data_label+0x10000($5)
	lb	$4,small_external_data_label+0x10000($5)
	lb	$4,big_external_common+0x10000($5)
	lb	$4,small_external_common+0x10000($5)
	lb	$4,big_local_common+0x10000($5)
	lb	$4,small_local_common+0x10000($5)
	lb	$4,data_label+0x1a5a5($5)
	lb	$4,big_external_data_label+0x1a5a5($5)
	lb	$4,small_external_data_label+0x1a5a5($5)
	lb	$4,big_external_common+0x1a5a5($5)
	lb	$4,small_external_common+0x1a5a5($5)
	lb	$4,big_local_common+0x1a5a5($5)
	lb	$4,small_local_common+0x1a5a5($5)

# Several macros are handled like lb.  Sanity check them.
	lbu	$4,0
	lh	$4,0
	lhu	$4,0
	lw	$4,0
	lwl	$4,0
	lwr	$4,0
	lwc0	$4,0
	lwc1	$4,0
	lwc2	$4,0
	lwc3	$4,0

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	nop
	nop
