# strftime.awk ; test the strftime code
#
# input is the output of `date', see Makefile.in
#
# The mucking about with $0 and $N is to avoid problems
# on cygwin, where the timezone field is empty and there
# are two consecutive blanks.

{
	$3 = sprintf("%02d", $3 + 0)
	print > "strftime.ok"
	$0 = strftime()
	$NF = $NF
	print > OUTPUT
}
