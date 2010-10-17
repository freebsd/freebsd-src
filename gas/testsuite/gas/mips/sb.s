# Source file used to test the sb macro.
	
	.data
data_label:
	.extern big_external_data_label,1000
	.extern small_external_data_label,1
	.comm big_external_common,1000
	.comm small_external_common,1
	.lcomm big_local_common,1000
	.lcomm small_local_common,1
	
	.text
	sb	$4,0
	sb	$4,1
	sb	$4,0x8000
	sb	$4,-0x8000
	sb	$4,0x10000
	sb	$4,0x1a5a5
	sb	$4,0($5)
	sb	$4,1($5)
	sb	$4,0x8000($5)
	sb	$4,-0x8000($5)
	sb	$4,0x10000($5)
	sb	$4,0x1a5a5($5)
	sb	$4,data_label
	sb	$4,big_external_data_label
	sb	$4,small_external_data_label
	sb	$4,big_external_common
	sb	$4,small_external_common
	sb	$4,big_local_common
	sb	$4,small_local_common
	sb	$4,data_label+1
	sb	$4,big_external_data_label+1
	sb	$4,small_external_data_label+1
	sb	$4,big_external_common+1
	sb	$4,small_external_common+1
	sb	$4,big_local_common+1
	sb	$4,small_local_common+1
	sb	$4,data_label+0x8000
	sb	$4,big_external_data_label+0x8000
	sb	$4,small_external_data_label+0x8000
	sb	$4,big_external_common+0x8000
	sb	$4,small_external_common+0x8000
	sb	$4,big_local_common+0x8000
	sb	$4,small_local_common+0x8000
	sb	$4,data_label-0x8000
	sb	$4,big_external_data_label-0x8000
	sb	$4,small_external_data_label-0x8000
	sb	$4,big_external_common-0x8000
	sb	$4,small_external_common-0x8000
	sb	$4,big_local_common-0x8000
	sb	$4,small_local_common-0x8000
	sb	$4,data_label+0x10000
	sb	$4,big_external_data_label+0x10000
	sb	$4,small_external_data_label+0x10000
	sb	$4,big_external_common+0x10000
	sb	$4,small_external_common+0x10000
	sb	$4,big_local_common+0x10000
	sb	$4,small_local_common+0x10000
	sb	$4,data_label+0x1a5a5
	sb	$4,big_external_data_label+0x1a5a5
	sb	$4,small_external_data_label+0x1a5a5
	sb	$4,big_external_common+0x1a5a5
	sb	$4,small_external_common+0x1a5a5
	sb	$4,big_local_common+0x1a5a5
	sb	$4,small_local_common+0x1a5a5
	sb	$4,data_label($5)
	sb	$4,big_external_data_label($5)
	sb	$4,small_external_data_label($5)
	sb	$4,big_external_common($5)
	sb	$4,small_external_common($5)
	sb	$4,big_local_common($5)
	sb	$4,small_local_common($5)
	sb	$4,data_label+1($5)
	sb	$4,big_external_data_label+1($5)
	sb	$4,small_external_data_label+1($5)
	sb	$4,big_external_common+1($5)
	sb	$4,small_external_common+1($5)
	sb	$4,big_local_common+1($5)
	sb	$4,small_local_common+1($5)
	sb	$4,data_label+0x8000($5)
	sb	$4,big_external_data_label+0x8000($5)
	sb	$4,small_external_data_label+0x8000($5)
	sb	$4,big_external_common+0x8000($5)
	sb	$4,small_external_common+0x8000($5)
	sb	$4,big_local_common+0x8000($5)
	sb	$4,small_local_common+0x8000($5)
	sb	$4,data_label-0x8000($5)
	sb	$4,big_external_data_label-0x8000($5)
	sb	$4,small_external_data_label-0x8000($5)
	sb	$4,big_external_common-0x8000($5)
	sb	$4,small_external_common-0x8000($5)
	sb	$4,big_local_common-0x8000($5)
	sb	$4,small_local_common-0x8000($5)
	sb	$4,data_label+0x10000($5)
	sb	$4,big_external_data_label+0x10000($5)
	sb	$4,small_external_data_label+0x10000($5)
	sb	$4,big_external_common+0x10000($5)
	sb	$4,small_external_common+0x10000($5)
	sb	$4,big_local_common+0x10000($5)
	sb	$4,small_local_common+0x10000($5)
	sb	$4,data_label+0x1a5a5($5)
	sb	$4,big_external_data_label+0x1a5a5($5)
	sb	$4,small_external_data_label+0x1a5a5($5)
	sb	$4,big_external_common+0x1a5a5($5)
	sb	$4,small_external_common+0x1a5a5($5)
	sb	$4,big_local_common+0x1a5a5($5)
	sb	$4,small_local_common+0x1a5a5($5)
	
# Several macros are handled like sb.  Sanity check them.
	sd	$4,0
	sh	$4,0
	sw	$4,0
	swc0	$4,0
	swc1	$4,0
	swc2	$4,0
	swc3	$4,0
	s.s	$f4,0
	swl	$4,0
	swr	$4,0

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	nop
