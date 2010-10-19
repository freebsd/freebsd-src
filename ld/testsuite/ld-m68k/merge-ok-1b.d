#source: merge-error-1a.s -march=isaa -mno-div -mmac
#source: merge-error-1b.s -march=isaa -mno-div -mfloat
#ld: -r
#objdump: -p
#...
private flags = 8051: \[cfv4e\] \[isa A\] \[nodiv\] \[float\] \[mac\]
