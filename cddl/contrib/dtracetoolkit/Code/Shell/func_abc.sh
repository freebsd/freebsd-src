#!./sh

func_c()
{
	echo "Function C"
	sleep 1
}

func_b()
{
	echo "Function B"
	sleep 1
	func_c
}

func_a()
{
	echo "Function A"
	sleep 1
	func_b
}

func_a
