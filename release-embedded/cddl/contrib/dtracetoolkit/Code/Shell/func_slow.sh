#!./sh

func_c()
{
	echo "Function C"
	i=0
	while [ $i -lt 300 ]
	do
		i=`expr $i + 1`
	done
}

func_b()
{
	echo "Function B"
	i=0
	while [ $i -lt 200 ]
	do
		i=`expr $i + 1`
	done
	func_c
}

func_a()
{
	echo "Function A"
	i=0
	while [ $i -lt 100 ]
	do
		i=`expr $i + 1`
	done
	func_b
}

func_a
