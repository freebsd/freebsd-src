BEGIN {
	str="\342\200\257"
	print length(str)
	match(str,/^/)
	print RSTART, RLENGTH	
	match(str,/.+/)
	print RSTART, RLENGTH
	match(str,/$/)
	print RSTART, RLENGTH
}
