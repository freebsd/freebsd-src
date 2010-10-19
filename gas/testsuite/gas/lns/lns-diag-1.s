	.file 1 "foo.c"
	.file 0 "bar.c"
	.file 2 baz.c
	.file 1 "bar.c"

	.loc 1 1
	.loc 1 2 3
	.loc 3 1
	.loc 1 1 1 1

	.loc 1 1 basic_block
	.loc 1 1 basic_block 0
	.loc 1 1 prologue_end
	.loc 1 1 epilogue_begin

	.loc 1 1 1 is_stmt 0
	.loc 1 1 1 is_stmt 1
	.loc 1 1 1 is_stmt 2
	.loc 1 1 1 is_stmt foo

	.loc 1 1 isa 1
	.loc 1 1 isa 2
	.loc 1 1 isa -1
	.loc 1 1 isa 0

	.loc frobnitz
	.loc 1 frobnitz
	.loc 1 1 frobnitz
	.loc 1 1 1 frobnitz
