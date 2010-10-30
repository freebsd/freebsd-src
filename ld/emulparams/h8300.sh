SCRIPT_NAME=h8300
OUTPUT_FORMAT="coff-h8300"
TEXT_START_ADDR=0x8000
TARGET_PAGE_SIZE=128
ARCH=h8300
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
