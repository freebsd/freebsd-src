# test the gnu regexp ops

BEGIN {
	if ("a rat is here" ~ /\yrat/)	print "test  1 ok (\\y)"
	else				print "test  1 failed (\\y)"
	if ("a rat is here" ~ /rat\y/)	print "test  2 ok (\\y)"
	else				print "test  2 failed (\\y)"
	if ("what a brat" !~ /\yrat/)	print "test  3 ok (\\y)"
	else				print "test  3 failed (\\y)"

	if ("in the crate" ~ /\Brat/)	print "test  4 ok (\\B)"
	else				print "test  4 failed (\\B)"
	if ("a rat" !~ /\Brat/)	print "test  5 ok (\\B)"
	else				print "test  5 failed (\\B)"

	if ("a word" ~ /\<word/)	print "test  6 ok (\\<)"
	else				print "test  6 failed (\\<)"
	if ("foreword" !~ /\<word/)	print "test  7 ok (\\<)"
	else				print "test  7 failed (\\<)"

	if ("a word" ~ /word\>/)	print "test  8 ok (\\>)"
	else				print "test  8 failed (\\\\>)"
	if ("wordy" !~ /word\>/)	print "test  9 ok (\\>)"
	else				print "test  9 failed (\\>)"

	if ("a" ~ /\w/)		print "test 10 ok (\\w)"
	else				print "test 10 failed (\\\\w)"
	if ("+" !~ /\w/)		print "test 11 ok (\\w)"
	else				print "test 11 failed (\\w)"

	if ("a" !~ /\W/)		print "test 12 ok (\\W)"
	else				print "test 12 failed (\\W)"
	if ("+" ~ /\W/)		print "test 13 ok (\\W)"
	else				print "test 13 failed (\\W)"

	if ("a" ~ /\`a/)		print "test 14 ok (\\`)"
	else				print "test 14 failed (\\`)"
	if ("b" !~ /\`a/)		print "test 15 ok (\\`)"
	else				print "test 15 failed (\\`)"

	if ("a" ~ /a\'/)		print "test 16 ok (\\')"
	else				print "test 16 failed (\\')"
	if ("b" !~ /a\'/)		print "test 17 ok (\\')"
	else				print "test 17 failed (\\')"
}
