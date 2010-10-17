ENTRY(_start)
SECTIONS
{
  /* Sections to be discarded */
  /DISCARD/ : {
	*(.data.exit)
	*(.text.exit)
	}
}
