	.text
	.ent foo
foo:
	.frame $30, 0, $26, 0
	.prologue 1
	ret
	.end foo

	.ent bar
bar:
	.frame $30, 0, $26, 0
	.prologue 1
	ret
	.end bar
