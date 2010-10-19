	.text
l:
/* a# test references after weakref. */
	.weakref Wwa1, wa1
	.long Wwa1

	.weakref Wua2, ua2
	.long ua2

	.weakref Wua3, ua3
	.long Wua3
	.long ua3

	.weakref Wua4, ua4
	.long ua4
	.long Wua4

	.weakref Wna5, na5

/* b# test references before weakref.  */
	.long Wwb1
	.weakref Wwb1, wb1

	.long ub2
	.weakref Wub2, ub2

	.long Wub3
	.long ub3
	.weakref Wub3, ub3

	.long ub4
	.long Wub4
	.weakref Wub4, ub4

/* c# test combinations of references before and after weakref.  */
	.long Wwc1
	.weakref Wwc1, wc1
	.long Wwc1

	.long uc2
	.weakref Wuc2, uc2
	.long uc2

	.long Wuc3
	.long uc3
	.weakref Wuc3, uc3
	.long Wuc3
	.long uc3

	.long uc4
	.long Wuc4
	.weakref Wuc4, uc4
	.long uc4
	.long Wuc4

	.long Wuc5
	.long uc5
	.weakref Wuc5, uc5
	.long uc5
	.long Wuc5

	.long uc6
	.long Wuc6
	.weakref Wuc6, uc6
	.long uc6
	.long Wuc6

	.long uc7
	.weakref Wuc7, uc7
	.long Wuc7

	.long Wuc8
	.weakref Wuc8, uc8
	.long uc8

	.long Wuc9
	.weakref Wuc9, uc9
	.long Wuc9
	.long uc9

/* w# test that explicitly weak target don't lose the weak status */
	.weakref Www1, ww1
	.weak ww1
	.long ww1

	.weak ww2
	.weakref Www2, ww2
	.long ww2

	.weak ww3
	.long ww3
	.weakref Www3, ww3
	.long ww3

	.long ww4
	.weakref Www4, ww4
	.weak ww4
	.long ww4

	.long ww5
	.weakref Www5, ww5
	.long ww5
	.weak ww5

	.weakref Www6, ww6
	.weak ww6
	.long Www6

	.weak ww7
	.weakref Www7, ww7
	.long Www7

	.weak ww8
	.long Www8
	.weakref Www8, ww8
	.long Www8

	.long Www9
	.weakref Www9, ww9
	.weak ww9
	.long Www9

	.long Www10
	.weakref Www10, ww10
	.long Www10
	.weak ww10

/* m# test multiple weakrefs */
	.weakref Wnm4a, nm4
	.weakref Wnm4b, nm4

	.weakref Wum5a, um5
	.weakref Wum5b, um5
	.long um5

	.weakref Wwm6a, wm6
	.weakref Wwm6b, wm6
	.long Wwm6a

	.weakref Wwm7a, wm7
	.weakref Wwm7b, wm7
	.long Wwm7b

	.weakref Wwm8a, wm8
	.long Wwm8b
	.weakref Wwm8b, wm8

/* h# test weakref chain */
	.weakref Wnh1a, nh1
	.weakref Wnh1b, Wnh1a
	.weakref Wnh1c, Wnh1b

	.weakref Wwh2a, wh2
	.weakref Wwh2b, Wwh2a
	.long Wwh2b

	.weakref Wwh3a, wh3
	.weakref Wwh3b, Wwh3a
	.long Wwh3a

	.weakref Wwh4b, Wwh4a
	.weakref Wwh4a, wh4
	.long Wwh4b

	.long Wwh5b
	.weakref Wwh5a, wh5
	.weakref Wwh5b, Wwh5a

	.long Wwh6b
	.weakref Wwh6b, Wwh6a
	.weakref Wwh6a, wh6

	.weakref Wwh7b, Wwh7a
	.long Wwh7b
	.weakref Wwh7a, wh7

	.long Wuh8c
	.weakref Wuh8a, uh8
	.weakref Wuh8b, Wuh8a
	.weakref Wuh8c, Wuh8b
	.long uh8

	.long Wuh9c
	.weakref Wuh9c, Wuh9b
	.weakref Wuh9b, Wuh9a
	.weakref Wuh9a, uh9
	.long uh9

/* d# target symbol definitions */
	.weakref Wld1, ld1
	.long Wld1
	ld1 = l

	.weakref Wld2, ld2
	.long Wld2
ld2:

ld3:
	.weakref Wld3, ld3
	.long Wld3

ld4:
	.long Wld4
	.weakref Wld4, ld4

	.global ud5
	.weakref Wud5, ud5
	.long Wud5

	.global gd6
	.weakref Wgd6, gd6
	.long Wgd6
gd6:

	.weakref Wgd7, gd7
	.long Wgd7
	.global gd7
gd7:

	.long Wld8c
	.weakref Wld8a, ld8
	.weakref Wld8b, Wld8a
	.weakref Wld8c, Wld8b
	.long ld8
ld8:

	.long Wld9c
	.weakref Wld9c, Wld9b
	.weakref Wld9b, Wld9a
	.weakref Wld9a, ld9
	.long ld9
ld9:
