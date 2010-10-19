#source: empty-aligned.s
#ld: -T empty-aligned.t
#readelf: -l --wide
#xfail: "hppa64-*-*"

#...
Program Headers:
 +Type +Offset +VirtAddr +PhysAddr +FileSiz +MemSiz +Flg +Align
 +LOAD +0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ 0x[0-9a-f]+ [RWE ]+ +0x[0-9a-f]+

 Section to Segment mapping:
 +Segment Sections\.\.\.
 +00 +.text 
