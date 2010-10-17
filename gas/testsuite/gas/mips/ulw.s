# Source file used to test the ulw macro.
	
	.data
data_label:
	.extern big_external_data_label,1000
	.extern small_external_data_label,1
	.comm big_external_common,1000
	.comm small_external_common,1
	.lcomm big_local_common,1000
	.lcomm small_local_common,1
	
	.text
	ulw	$4,0
	ulw	$4,1
	ulw	$4,0x8000
	ulw	$4,-0x8000
	ulw	$4,0x10000
	ulw	$4,0x1a5a5
	ulw	$4,0($5)
	ulw	$4,1($5)
	ulw	$4,data_label
	ulw	$4,big_external_data_label
	ulw	$4,small_external_data_label
	ulw	$4,big_external_common
	ulw	$4,small_external_common
	ulw	$4,big_local_common
	ulw	$4,small_local_common
	ulw	$4,data_label+1
	ulw	$4,big_external_data_label+1
	ulw	$4,small_external_data_label+1
	ulw	$4,big_external_common+1
	ulw	$4,small_external_common+1
	ulw	$4,big_local_common+1
	ulw	$4,small_local_common+1
	ulw	$4,data_label+0x8000
	ulw	$4,big_external_data_label+0x8000
	ulw	$4,small_external_data_label+0x8000
	ulw	$4,big_external_common+0x8000
	ulw	$4,small_external_common+0x8000
	ulw	$4,big_local_common+0x8000
	ulw	$4,small_local_common+0x8000
	ulw	$4,data_label-0x8000
	ulw	$4,big_external_data_label-0x8000
	ulw	$4,small_external_data_label-0x8000
	ulw	$4,big_external_common-0x8000
	ulw	$4,small_external_common-0x8000
	ulw	$4,big_local_common-0x8000
	ulw	$4,small_local_common-0x8000
	ulw	$4,data_label+0x10000
	ulw	$4,big_external_data_label+0x10000
	ulw	$4,small_external_data_label+0x10000
	ulw	$4,big_external_common+0x10000
	ulw	$4,small_external_common+0x10000
	ulw	$4,big_local_common+0x10000
	ulw	$4,small_local_common+0x10000
	ulw	$4,data_label+0x1a5a5
	ulw	$4,big_external_data_label+0x1a5a5
	ulw	$4,small_external_data_label+0x1a5a5
	ulw	$4,big_external_common+0x1a5a5
	ulw	$4,small_external_common+0x1a5a5
	ulw	$4,big_local_common+0x1a5a5
	ulw	$4,small_local_common+0x1a5a5

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	nop
