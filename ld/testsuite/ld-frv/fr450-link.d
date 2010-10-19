#source: fr450-linka.s -mcpu=fr400
#source: fr450-linkb.s -mcpu=fr405
#source: fr450-linkc.s -mcpu=fr450
#source: fr450-linkb.s -mcpu=fr405
#source: fr450-linka.s -mcpu=fr400
#ld: -r
#objdump: -p

.*:     file format elf32-frv(|fdpic)
private flags = 0x800[08]000: -mcpu=fr450(| -mfdpic)

