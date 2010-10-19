#source: start.s
#source: exit.s
#ld: -T discard.ld
#error: `data' referenced in section `\.text' of tmpdir/dump0.o: defined in discarded section `\.data\.exit' of tmpdir/dump1.o
#objdump: -p
#pass
