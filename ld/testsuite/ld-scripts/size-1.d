#source: size-1.s
#ld: -T size-1.t
#objdump: -s

.*:     file format .*

#...
Contents of section \.text:
 [0-9a-f]* (01)?000000(01)? (02)?000000(02)? .*
#...
Contents of section \.data:
 [0-9a-f]* (03)?000000(03)? (04)?000000(04)? (05)?000000(05)? 00000000 .*
 [0-9a-f]* (20)?000000(20)? (18)?000000(18)? .*
#pass
