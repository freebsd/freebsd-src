# Test unaligned load and store macros with PIC code.  We don't bother
# to test most cases.  The actual loads and stores are tested by the
# non-PIC test case.  We just want to check that the initial address
# is loaded correctly.

	.data
data_label:
	.extern big_external_data_label,1000
	.extern small_external_data_label,1
	.comm big_external_common,1000
	.comm small_external_common,1
	.lcomm big_local_common,1000
	.lcomm small_local_common,1
	
	.text
	ulh	$4,data_label
	ulhu	$4,big_external_data_label
	ulw	$4,small_external_data_label
	ush	$4,big_external_common
	usw	$4,small_external_common
	ulh	$4,big_local_common
	ulhu	$4,small_local_common
	ulw	$4,data_label+1
	ush	$4,big_external_data_label+1
	usw	$4,small_external_data_label+1
	ulh	$4,big_external_common+1
	ulhu	$4,small_external_common+1
	ulw	$4,big_local_common+1
	ush	$4,small_local_common+1

# Round to a 16 byte boundary, for ease in testing multiple targets.
	nop
	.ifndef	XGOT
	nop
	nop
	.endif
