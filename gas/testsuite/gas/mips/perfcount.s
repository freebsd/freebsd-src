# source file to test assembly of R1[20]000 performance counter instructions.

foo:
	mtps	$4, 0
	mfps	$4, 1
	mtpc	$4, 1
	mfpc	$4, 0
