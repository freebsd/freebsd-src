# Source file used to test the abs macro.
foo:
	mtc1	$0,$f0
	cvt.d.w	$f0,$f0
        mtc1    $0,$f2
        cvt.d.w $f2,$f2
        .space	8

