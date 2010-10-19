#source: extern.s
#ld: -T discard.ld
#error: .*data.* referenced in section `\.text' of tmpdir/dump0.o: defined in discarded section `\.data\.exit' of tmpdir/dump0.o
#objdump: -p
#pass
# The expected warning used to start with "`data' referenced..." but
# this has two problems: 1) It does not include the name of the linker
# command which will be present in the message, eg "../ld-new"
# 2) Targets which define EXTERN_FORCE_RELOC to 0 in their
# gas/config/tc-xxx.h file will convert the symbol in the reloc from
# "data" to the section symbol ".data.exit".
