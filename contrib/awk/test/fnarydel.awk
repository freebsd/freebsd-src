#!/usr/local/bin/gawk -f
BEGIN {
  process()
}

function process(aa,a) {
  delete aa
}

BEGIN {
	for (i = 1; i < 10; i++)
		a[i] = i;

	print "first loop"
	for (i in a)
		print a[i]

	delete a

	print "second loop"
	for (i in a)
		print a[i]

	for (i = 1; i < 10; i++)
		a[i] = i;

	print "third loop"
	for (i in a)
		print a[i]

	print "call func"
	delit(a)

	print "fourth loop"
	for (i in a)
		print a[i]

	stressit()
}

function delit(arr)
{
	delete arr
}

function stressit(	array, i)
{
	delete array
	array[4] = 4
	array[5] = 5
	delete array[5]
	print "You should just see: 4 4"
	for (i in array)
		print i, array[i]
	delete array
	print "You should see nothing between this line"
	for (i in array)
		print i, array[i]
	print "And this one"
}
