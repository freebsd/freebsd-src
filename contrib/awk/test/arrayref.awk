	BEGIN { # foo[10] = 0		# put this line in and it will work
		test(foo); print foo[1]
		test2(foo2); print foo2[1]
	}

	function test(foo)
	{
		test2(foo)
	}
	function test2(bar)
	{
		bar[1] = 1
	}
