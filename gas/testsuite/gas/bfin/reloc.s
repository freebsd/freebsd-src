	.extern load_extern1;
	.extern call_data1;
	.data
load_data1:	.word 4567;
load_data2:	.word 8901;
load_data3:	.word 1243;
load_data4:	.byte load_extern1;
	.text
	jump exit;
	call _call_data1;
	r5 = load_data1;
	jump exit-4;
	call call_data1+8;
	r5.H = load_data2;
	r7.L = load_data3;
	r1.h = load_extern1;

exit:

