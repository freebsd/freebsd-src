#!/bin/sh

./bsddialog --title mixedform --mixedform "Hello World!" 12 40 5 \
	Label:    1	1	Entry		1	11	18	25	0 \
	Label:    2	1	Read-Only	2	11	18	25	2 \
	Password: 3	1	Value2		3	11	18	25	1 \
	Password: 4	1	Value4		4	11	18	25	3 \
	Label5:   5	1	Value5		5	11	18	25	4 \
	2>out.txt ; cat out.txt ; rm out.txt

./bsddialog --backtitle "BSD-2-Clause Licese" --title " form " --form "Hello World!" 12 40 5 \
	Label1:	1	1	Value1		1	9	18	25 \
	Label2:	2	1	Value2		2	9	18	25 \
	Label3:	3	1	Value3		3	9	18	25 \
	Label4:	4	1	Value4		4	9	18	25 \
	Label5:	5	1	Value5		5	9	18	25 \
	2>out.txt ; cat out.txt ; rm out.txt

./bsddialog --title passwordform --passwordform "Hello World!" 12 40 5 \
	Password1:	1	1	Value1		1	12	18	25 \
	Password2:	2	1	Value2		2	12	18	25 \
	Password3:	3	1	Value3		3	12	18	25 \
	Password4:	4	1	Value4		4	12	18	25 \
	Password5:	5	1	Value5		5	12	18	25 \
	2>out.txt ; cat out.txt ; rm out.txt
