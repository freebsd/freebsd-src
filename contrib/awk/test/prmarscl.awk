function test(a)
{
	print a[1]
}

BEGIN	{ j = 4; test(j) }
