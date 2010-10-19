#source: arch-err-4.s
#as: --underscore --march=common_v10_v32 --em=criself
#objdump: -p

#...
private flags = 5: \[symbols have a _ prefix\] \[v10 and v32\]
#pass
