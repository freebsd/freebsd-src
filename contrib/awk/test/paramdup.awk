BEGIN { foo(0, 1, 2) }

function foo(a, b, c, b, a)
{
	print "a =", a
	print "b =", b
	print "c =", c
}
