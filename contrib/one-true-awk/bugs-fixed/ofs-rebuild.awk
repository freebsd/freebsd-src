# The bug here is that nawk should use the value of OFS that
# was current when $0 became invalid to rebuild the record.

BEGIN {
	OFS = ":"
	$0 = "a b c d e f g"
	$3 = "3333"
	# Conceptually, $0 should now be "a:b:3333:d:e:f:g"

	# Change OFS after (conceptually) rebuilding the record
	OFS = "<>"

	# Unmodified nawk prints "a<>b<>3333<>d<>e<>f<>g" because
	# it delays rebuilding $0 until it's needed, and then it uses
	# the current value of OFS. Oops.
	print
}
