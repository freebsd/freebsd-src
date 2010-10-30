. ${srcdir}/emulparams/h8300elf.sh
ARCH="h8300:h8300sx"
STACK_ADDR=0x2fefc
TINY_READONLY_SECTION=".tinyrodata :
  {
	*(.tinyrodata)
  } =0"
TINY_DATA_SECTION=".tinydata	0xff8000 :
  {
	*(.tinydata)
	${RELOCATING+ _tinydata = .; }
  }"
TINY_BSS_SECTION=".tinybss	: AT (_tinydata)
  {
	*(.tinybss)
  }"
