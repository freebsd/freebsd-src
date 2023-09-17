#!/bin/sh

for ext in err out rc
do
	for f in tests/*.$ext
	do
		mv $f ${f%.$ext}.exp$ext
	done
done
