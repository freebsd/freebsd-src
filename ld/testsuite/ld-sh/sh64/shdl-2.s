! Part two of test for inter-file DataLabel support.

	.mode SHmedia
	.text
part2:
	movi (datalabel foowithout + 16) & 65535, r24

	.section .rodata
	.long datalabel foo_otherwithout + 32

	.text
	movi (datalabel foo_mixwithout + 1024) & 65535, r24
	.section .rodata
	.long datalabel foo_mixwithout + 32

	.text
	movi (datalabel foo_mixwithout2 + 1024) & 65535, r24
	.section .rodata
	.long foo_mixwithout2 + 32

	.text
	.global bar
bar:
	nop

	.global barboth
barboth:
	nop

	.global barboth2
barboth2:
	nop

	.global barwithout
barwithout:
	nop

	.global bar_other
bar_other:
	nop
	.global bar_otherboth
bar_otherboth:
	nop
	.global bar_otherboth2
bar_otherboth2:
	nop
	.global bar_otherwithout
bar_otherwithout:
	nop

	.text

	.global bar_mix
bar_mix:
	nop
	.global bar_mix2
bar_mix2:
	nop
	.global bar_mixboth
bar_mixboth:
	nop
	.global bar_mixboth2
bar_mixboth2:
	nop
	.global bar_mixwithout
bar_mixwithout:
	nop
	.global bar_mixwithout2
bar_mixwithout2:
	nop

! Almost-copy of "foo" in primary file.

	.global baz
baz:
	nop
	movi (datalabel baz + 8) & 65535,r30

	.global bazboth
bazboth:
	nop
	movi (datalabel bazboth + 16) & 65535,r40
	movi (bazboth + 12) & 65535,r40

	.global bazboth2
bazboth2:
	nop
	movi (bazboth2 + 12) & 65535,r40
	movi (datalabel bazboth2 + 16) & 65535,r40

	.global bazwithout
bazwithout:
	nop
	movi (datalabel bazwithout + 24) & 65535,r30

	.global baz_other
baz_other:
	nop
	.global baz_otherboth
baz_otherboth:
	nop
	.global baz_otherboth2
baz_otherboth2:
	nop
	.global baz_otherwithout
baz_otherwithout:
	nop

	.section .rodata
	.long datalabel baz_other + 4
	.long datalabel baz_otherboth + 40
	.long baz_otherboth + 24
	.long baz_otherboth2 + 24
	.long datalabel baz_otherboth2 + 40
	.long baz_otherwithout

	.text

	.global baz_mix
baz_mix:
	nop
	movi (datalabel baz_mix + 8) & 65535,r30
	.global baz_mix2
baz_mix2:
	nop
	movi (baz_mix2 + 8) & 65535,r30
	.global baz_mixboth
baz_mixboth:
	nop
	movi (datalabel baz_mixboth + 80) & 65535,r30
	movi (baz_mixboth + 80) & 65535,r30
	.global baz_mixboth2
baz_mixboth2:
	nop
	movi (baz_mixboth2 + 64) & 65535,r30
	movi (datalabel baz_mixboth2 + 64) & 65535,r30
	.global baz_mixwithout
baz_mixwithout:
	nop
	movi (baz_mixwithout + 42) & 65535,r30
	.global baz_mixwithout2
baz_mixwithout2:
	nop
	movi (baz_mixwithout2 + 24) & 65535,r30

	.section .rodata
	.long baz_mix + 4
	.long datalabel baz_mix2 + 48
	.long datalabel baz_mixboth + 400
	.long baz_mixboth + 420
	.long baz_mixboth2 + 248
	.long datalabel baz_mixboth2 + 240
	.long baz_mixwithout

	.data
	.long datalabel dfoowithout + 44
	.long datalabel dfoo_mixwithout + 48
	.long datalabel dfoo_mixwithout2 + 84

	.global dbar
dbar:
	.long 0
	.global dbarboth
dbarboth:
	.long 0
	.global dbarboth2
dbarboth2:
	.long 0
	.global dbarwithout
dbarwithout:
	.long 0
	.global dbar_other
dbar_other:
	.long 0
	.global dbar_otherboth
dbar_otherboth:
	.long 0
	.global dbar_otherboth2
dbar_otherboth2:
	.long 0
	.global dbar_otherwithout
dbar_otherwithout:
	.long 0

	.global dbar_mix
dbar_mix:
	.long 0
	.global dbar_mix2
dbar_mix2:
	.long 0
	.global dbar_mixboth
dbar_mixboth:
	.long 0
	.global dbar_mixboth2
dbar_mixboth2:
	.long 0
	.global dbar_mixwithout
dbar_mixwithout:
	.long 0
	.global dbar_mixwithout2
dbar_mixwithout2:
	.long 0

! Almost-copy of "dfoo" in primary file.

	.data
	.global dbaz
dbaz:
	.long 0
	.long (datalabel dbaz + 8)

	.global dbazboth
dbazboth:
	.long 0
	.long (datalabel dbazboth + 16)
	.long (dbazboth + 12)

	.global dbazboth2
dbazboth2:
	.long 0
	.long (dbazboth2 + 12)
	.long (datalabel dbazboth2 + 16)

	.global dbazwithout
dbazwithout:
	.long 0
	.long (dbazwithout + 24)

	.global dbaz_other
dbaz_other:
	.long 0
	.global dbaz_otherboth
dbaz_otherboth:
	.long 0
	.global dbaz_otherboth2
dbaz_otherboth2:
	.long 0
	.global dbaz_otherwithout
dbaz_otherwithout:
	.long 0

	.section .rodata
	.long datalabel dbaz_other + 4
	.long datalabel dbaz_otherboth + 40
	.long dbaz_otherboth + 24
	.long dbaz_otherboth2 + 24
	.long datalabel dbaz_otherboth2 + 40
	.long dbaz_otherwithout

	.data

	.global dbaz_mix
dbaz_mix:
	.long 0
	.long (datalabel dbaz_mix + 8)
	.global dbaz_mix2
dbaz_mix2:
	.long 0
	.long (dbaz_mix2 + 8)
	.global dbaz_mixboth
dbaz_mixboth:
	.long 0
	.long (datalabel dbaz_mixboth + 80)
	.long (dbaz_mixboth + 80)
	.global dbaz_mixboth2
dbaz_mixboth2:
	.long 0
	.long (dbaz_mixboth2 + 64)
	.long (datalabel dbaz_mixboth2 + 64)
	.global dbaz_mixwithout
dbaz_mixwithout:
	.long 0
	.long (dbaz_mixwithout + 42)
	.global dbaz_mixwithout2
dbaz_mixwithout2:
	.long 0
	.long (dbaz_mixwithout2 + 24)

	.section .rodata
	.long dbaz_mix + 4
	.long datalabel dbaz_mix2 + 48
	.long datalabel dbaz_mixboth + 400
	.long dbaz_mixboth + 420
	.long dbaz_mixboth2 + 248
	.long datalabel dbaz_mixboth2 + 240
	.long dbaz_mixwithout
