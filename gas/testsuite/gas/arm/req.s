	.text
	.global test_dot_req_and_unreq
test_dot_req_and_unreq:

	#  Check that builtin register alias 'r0' works.
	add r0, r0, r0

	# Create an alias for r0.
	foo .req r0

	# Check that it works.
	add foo, foo, foo

	# Now remove the alias.
        .unreq foo

	# And make sure that it no longer works.
	add foo, foo, foo

	# Finally remove the builtin alias for r0.
        .unreq r0

	# And make sure that this no longer works.
	add r0, r0, r0
	
