function foo()
{
	print "foo"
}

function bar(A, Z, q)
{
	print "bar"
}

function baz(C, D)
{
	print "baz"
}

BEGIN {
	A = C = D = Z = y = 1
	foo()
	bar()
	baz()
}
