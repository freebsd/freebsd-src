	sethi	%hi(__GOTT_BASE__), %l7
	ld	[%l7+%lo(__GOTT_BASE__)],%l7
	ld	[%l7+%lo(__GOTT_INDEX__)],%l7

	sethi	%hi(__GOTT_BASE__), %g1
	or	%g1, %lo(__GOTT_BASE__), %g1
	sethi	%hi(__GOTT_INDEX__), %g1
	or	%g1, %lo(__GOTT_INDEX__), %g1

	sethi	%hi(__GOT_BASE__), %g1
	or	%g1, %lo(__GOT_BASE__), %g1
