# First create .eh_frame with the right type.
.section	.eh_frame,"a",@unwind
.long 0

# Verify that switching back into .eh_frame does not change
# its type.
.section .eh_frame
.long 1
