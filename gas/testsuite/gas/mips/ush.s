# Source file used to test the ush macro.
	
	.data
data_label:
	.extern big_external_data_label,1000
	.extern small_external_data_label,1
	.comm big_external_common,1000
	.comm small_external_common,1
	.lcomm big_local_common,1000
	.lcomm small_local_common,1
	
	.text
	ush	$4,0
	ush	$4,1
	ush	$4,0x8000
	ush	$4,-0x8000
	ush	$4,0x10000
	ush	$4,0x1a5a5
	ush	$4,0($5)
	ush	$4,1($5)
	ush	$4,data_label
	ush	$4,big_external_data_label
	ush	$4,small_external_data_label
	ush	$4,big_external_common
	ush	$4,small_external_common
	ush	$4,big_local_common
	ush	$4,small_local_common
	ush	$4,data_label+1
	ush	$4,big_external_data_label+1
	ush	$4,small_external_data_label+1
	ush	$4,big_external_common+1
	ush	$4,small_external_common+1
	ush	$4,big_local_common+1
	ush	$4,small_local_common+1
	ush	$4,data_label+0x8000
	ush	$4,big_external_data_label+0x8000
	ush	$4,small_external_data_label+0x8000
	ush	$4,big_external_common+0x8000
	ush	$4,small_external_common+0x8000
	ush	$4,big_local_common+0x8000
	ush	$4,small_local_common+0x8000
	ush	$4,data_label-0x8000
	ush	$4,big_external_data_label-0x8000
	ush	$4,small_external_data_label-0x8000
	ush	$4,big_external_common-0x8000
	ush	$4,small_external_common-0x8000
	ush	$4,big_local_common-0x8000
	ush	$4,small_local_common-0x8000
	ush	$4,data_label+0x10000
	ush	$4,big_external_data_label+0x10000
	ush	$4,small_external_data_label+0x10000
	ush	$4,big_external_common+0x10000
	ush	$4,small_external_common+0x10000
	ush	$4,big_local_common+0x10000
	ush	$4,small_local_common+0x10000
	ush	$4,data_label+0x1a5a5
	ush	$4,big_external_data_label+0x1a5a5
	ush	$4,small_external_data_label+0x1a5a5
	ush	$4,big_external_common+0x1a5a5
	ush	$4,small_external_common+0x1a5a5
	ush	$4,big_local_common+0x1a5a5
	ush	$4,small_local_common+0x1a5a5

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
