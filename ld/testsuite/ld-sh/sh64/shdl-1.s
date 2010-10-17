! Test inter-file DataLabel support.
!
! We need to test symbols that are:
! * Global, defined in this file, with/without/both-with-without datalabel
!   references.
! * The above in combinations where the reference is/is not from within
!   the same section.  The implementation is currently indifferent to that
!   fact, but it seems likely to be something that can change.
! * Extern with/without/both-with-without datalabel-qualified references.
! * The above with reference from same *and* other file.
! * The above in combinations for where the symbol is/is not a
!   STO_SH5_ISA32-marked symbol.

! There will be omissions and overlap in combinations.  Add spotted
! omissions with complementary tests in other files.

	.text
	.mode SHmedia

! For good measure, we start with a nop to get a non-zero offset within
! the .text section.

	.global start
start:
	nop

! Referenced from the same file, same section, is ISA32, only referenced
! with datalabel qualifier.
	.global foo
foo:
	nop
	movi (datalabel foo + 8) & 65535,r30

! Referenced from same file, same section, both with and without
! datalabel qualifier, is ISA32.
	.global fooboth
fooboth:
	nop
	movi (datalabel fooboth + 16) & 65535,r40
	movi (fooboth + 12) & 65535,r40

! Same as above, but in different order.
	.global fooboth2
fooboth2:
	nop
	movi (fooboth2 + 12) & 65535,r40
	movi (datalabel fooboth2 + 16) & 65535,r40

! Referenced from this file and another, same section, is ISA32.
	.global foowithout
foowithout:
	nop
	movi (foowithout + 24) & 65535,r30

! Same as above, different section than definition.

	.global foo_other
foo_other:
	nop
	.global foo_otherboth
foo_otherboth:
	nop
	.global foo_otherboth2
foo_otherboth2:
	nop
	.global foo_otherwithout
foo_otherwithout:
	nop

	.section .rodata
	.long datalabel foo_other + 4
	.long datalabel foo_otherboth + 40
	.long foo_otherboth + 24
	.long foo_otherboth2 + 24
	.long datalabel foo_otherboth2 + 40
	.long foo_otherwithout

	.text

! Same as above, mixing references from same and other section.
	.global foo_mix
foo_mix:
	nop
	movi (datalabel foo_mix + 8) & 65535,r30
	.global foo_mix2
foo_mix2:
	nop
	movi (foo_mix2 + 8) & 65535,r30
	.global foo_mixboth
foo_mixboth:
	nop
	movi (datalabel foo_mixboth + 80) & 65535,r30
	movi (foo_mixboth + 80) & 65535,r30
	.global foo_mixboth2
foo_mixboth2:
	nop
	movi (foo_mixboth2 + 64) & 65535,r30
	movi (datalabel foo_mixboth2 + 64) & 65535,r30
	.global foo_mixwithout
foo_mixwithout:
	nop
	movi (foo_mixwithout + 42) & 65535,r30
	.global foo_mixwithout2
foo_mixwithout2:
	nop
	movi (foo_mixwithout2 + 24) & 65535,r30

	.section .rodata
	.long foo_mix + 4
	.long datalabel foo_mix2 + 48
	.long datalabel foo_mixboth + 400
	.long foo_mixboth + 420
	.long foo_mixboth2 + 248
	.long datalabel foo_mixboth2 + 240
	.long foo_mixwithout

! Same as above, referencing symbol in other file (reference only from
! this to other file).

	.text
	nop
	movi (datalabel bar + 8) & 65535,r30

	movi (datalabel barboth + 16) & 65535,r40
	movi (barboth + 12) & 65535,r40

	movi (barboth2 + 12) & 65535,r40
	movi (datalabel barboth2 + 16) & 65535,r40

	movi (barwithout + 24) & 65535,r30

	.section .rodata
	.long datalabel bar_other + 4
	.long datalabel bar_otherboth + 40
	.long bar_otherboth + 24
	.long bar_otherboth2 + 24
	.long datalabel bar_otherboth2 + 40
	.long bar_otherwithout

	.text
	movi (datalabel bar_mix + 8) & 65535,r30
	movi (bar_mix2 + 8) & 65535,r30
	movi (datalabel bar_mixboth + 80) & 65535,r30
	movi (bar_mixboth + 80) & 65535,r30
	movi (bar_mixboth2 + 64) & 65535,r30
	movi (datalabel bar_mixboth2 + 64) & 65535,r30
	movi (bar_mixwithout + 42) & 65535,r30
	movi (bar_mixwithout2 + 24) & 65535,r30

	.section .rodata
	.long bar_mix + 4
	.long datalabel bar_mix2 + 48
	.long datalabel bar_mixboth + 400
	.long bar_mixboth + 420
	.long bar_mixboth2 + 248
	.long datalabel bar_mixboth2 + 240
	.long bar_mixwithout

! Same as above, referencing symbol in other file *and* within that file.

	.text
	movi (datalabel baz + 8) & 65535,r30

	movi (datalabel bazboth + 16) & 65535,r40
	movi (bazboth + 12) & 65535,r40

	movi (bazboth2 + 12) & 65535,r40
	movi (datalabel bazboth2 + 16) & 65535,r40

	movi (bazwithout + 24) & 65535,r30

	.section .rodata
	.long datalabel baz_other + 4
	.long datalabel baz_otherboth + 40
	.long baz_otherboth + 24
	.long baz_otherboth2 + 24
	.long datalabel baz_otherboth2 + 40
	.long baz_otherwithout

	.text
	movi (datalabel baz_mix + 8) & 65535,r30
	movi (baz_mix2 + 8) & 65535,r30
	movi (datalabel baz_mixboth + 80) & 65535,r30
	movi (baz_mixboth + 80) & 65535,r30
	movi (baz_mixboth2 + 64) & 65535,r30
	movi (datalabel baz_mixboth2 + 64) & 65535,r30
	movi (baz_mixwithout + 42) & 65535,r30
	movi (baz_mixwithout2 + 24) & 65535,r30

	.section .rodata
	.long baz_mix + 4
	.long datalabel baz_mix2 + 48
	.long datalabel baz_mixboth + 400
	.long baz_mixboth + 420
	.long baz_mixboth2 + 248
	.long datalabel baz_mixboth2 + 240
	.long baz_mixwithout

! Same as all of the above, but where the symbol is not an ISA32 one.

	.data
	.global dfoo
dfoo:
	.long 0
	.long (datalabel dfoo + 8)

	.global dfooboth
dfooboth:
	.long 0
	.long (datalabel dfooboth + 16)
	.long (dfooboth + 12)

	.global dfooboth2
dfooboth2:
	.long 0
	.long (dfooboth2 + 12)
	.long (datalabel dfooboth2 + 16)

	.global dfoowithout
dfoowithout:
	.long 0
	.long (dfoowithout + 24)

	.global dfoo_other
dfoo_other:
	.long 0
	.global dfoo_otherboth
dfoo_otherboth:
	.long 0
	.global dfoo_otherboth2
dfoo_otherboth2:
	.long 0
	.global dfoo_otherwithout
dfoo_otherwithout:
	.long 0

	.section .rodata
	.long datalabel dfoo_other + 4
	.long datalabel dfoo_otherboth + 40
	.long dfoo_otherboth + 24
	.long dfoo_otherboth2 + 24
	.long datalabel dfoo_otherboth2 + 40
	.long dfoo_otherwithout

	.data

! Same as above, mixing references from same and other section.
	.global dfoo_mix
dfoo_mix:
	.long 0
	.long (datalabel dfoo_mix + 8)
	.global dfoo_mix2
dfoo_mix2:
	.long 0
	.long (dfoo_mix2 + 8)
	.global dfoo_mixboth
dfoo_mixboth:
	.long 0
	.long (datalabel dfoo_mixboth + 80)
	.long (dfoo_mixboth + 80)
	.global dfoo_mixboth2
dfoo_mixboth2:
	.long 0
	.long (dfoo_mixboth2 + 64)
	.long (datalabel dfoo_mixboth2 + 64)
	.global dfoo_mixwithout
dfoo_mixwithout:
	.long 0
	.long (dfoo_mixwithout + 42)
	.global dfoo_mixwithout2
dfoo_mixwithout2:
	.long 0
	.long (dfoo_mixwithout2 + 24)

	.section .rodata
	.long dfoo_mix + 4
	.long datalabel dfoo_mix2 + 48
	.long datalabel dfoo_mixboth + 400
	.long dfoo_mixboth + 420
	.long dfoo_mixboth2 + 248
	.long datalabel dfoo_mixboth2 + 240
	.long dfoo_mixwithout

! Same as above, referencing symbol in other file (reference only from
! this to other file).

	.text
	movi (datalabel dbarboth + 16) & 65535,r40
	movi (dbarboth + 12) & 65535,r40
	movi (dbarboth2 + 12) & 65535,r40
	movi (datalabel dbarboth2 + 16) & 65535,r40
	movi (dbarwithout + 24) & 65535,r30

	.data
	.long (datalabel dbar + 8)
	.long datalabel dbar_other + 4
	.long datalabel dbar_otherboth + 40
	.long dbar_otherboth + 24
	.long dbar_otherboth2 + 24
	.long datalabel dbar_otherboth2 + 40
	.long dbar_otherwithout

	.text
	movi (datalabel dbar_mix + 8) & 65535,r30
	movi (dbar_mix2 + 8) & 65535,r30
	movi (datalabel dbar_mixboth + 80) & 65535,r30
	movi (dbar_mixboth + 80) & 65535,r30
	movi (dbar_mixboth2 + 64) & 65535,r30
	movi (datalabel dbar_mixboth2 + 64) & 65535,r30
	movi (dbar_mixwithout + 42) & 65535,r30
	movi (dbar_mixwithout2 + 24) & 65535,r30

	.data
	.long dbar_mix + 4
	.long datalabel dbar_mix2 + 48
	.long datalabel dbar_mixboth + 400
	.long dbar_mixboth + 420
	.long dbar_mixboth2 + 248
	.long datalabel dbar_mixboth2 + 240
	.long dbar_mixwithout

! Same as above, referencing symbol in other file *and* within that file.

	.text
	movi (datalabel dbazboth + 16) & 65535,r40
	movi (dbazboth + 12) & 65535,r40

	movi (dbazboth2 + 12) & 65535,r40
	movi (datalabel dbazboth2 + 16) & 65535,r40

	movi (dbazwithout + 24) & 65535,r30

	.data
	.long (datalabel dbaz + 8)
	.long datalabel dbaz_other + 4
	.long datalabel dbaz_otherboth + 40
	.long dbaz_otherboth + 24
	.long dbaz_otherboth2 + 24
	.long datalabel dbaz_otherboth2 + 40
	.long dbaz_otherwithout

	.text
	movi (datalabel dbaz_mix + 8) & 65535,r30
	movi (dbaz_mix2 + 8) & 65535,r30
	movi (datalabel dbaz_mixboth + 80) & 65535,r30
	movi (dbaz_mixboth + 80) & 65535,r30
	movi (dbaz_mixboth2 + 64) & 65535,r30
	movi (datalabel dbaz_mixboth2 + 64) & 65535,r30
	movi (dbaz_mixwithout + 42) & 65535,r30
	movi (dbaz_mixwithout2 + 24) & 65535,r30

	.data
	.long dbaz_mix + 4
	.long datalabel dbaz_mix2 + 48
	.long datalabel dbaz_mixboth + 400
	.long dbaz_mixboth + 420
	.long dbaz_mixboth2 + 248
	.long datalabel dbaz_mixboth2 + 240
	.long dbaz_mixwithout
