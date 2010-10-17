* MRI script
sect .text = $100	; .text start address
sect .data = 1000h	; .data start address
public text_start = $100
public text_end = # continuation line
  text_start + 4
public data_start = 1000h
public data_end = data_start + 4

load tmpdir/script.o
