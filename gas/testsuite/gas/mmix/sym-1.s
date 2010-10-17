# Test beginning and starting with ":"; it should be stripped off, but
# only one.  A trailing ":" is stripped off at a label only.

Main SWYM 0,4,16
	.global :scg1
	.global scg2
	.global ::scg3
	.global scg2
:scg1	SWYM 16,4,0
:scg2	SWYM 161,42,30
::scg3	SWYM 163,42,20
:scl1	SWYM 1,2,3
::scl2	SWYM 1,2,4
	.global endcg1
	.global endcg2:
endcg1:	SWYM 3,2,1
endcg2::	SWYM 3,2,1
endcl1:	SWYM 4,3,2
endcl2::	SWYM 4,3,2
