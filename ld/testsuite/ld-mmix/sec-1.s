 .section secname,"ax"
 TETRA 1,2,3,4,-1,-2009
 BYTE 80

 .section anothersec,"aw"
 TETRA 10,9,8,7
 BYTE 37,39,41

 .section thirdsec
 TETRA 200001,100002
 BYTE 38,40

 .section .a.fourth.section,"a"
 OCTA 8888888,808080808
