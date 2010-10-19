#source: arch-err-5.s
#as: --march=v0_v10 --underscore --em=criself
#objdump: -p

#...
private flags = 1: \[symbols have a _ prefix\]
#pass
