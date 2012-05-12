#!./sh

func_c()
{
	/usr/bin/echo "Function C"
	sleep 1
}

func_b()
{
	/usr/bin/echo "Function B"
	sleep 1
	func_c
}

func_a()
{
	/usr/bin/echo "Function A"
	sleep 1
	func_b
}

func_a
