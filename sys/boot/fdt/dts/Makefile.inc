# $FreeBSD$

test-dts:
.for dts in ${DTS}
	dtc -I dts -O dtb -o /dev/null ${dts}
.endfor

