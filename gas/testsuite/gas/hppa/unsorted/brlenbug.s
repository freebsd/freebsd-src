	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:
	.data

	.align 4
done___2
	.word 0
	.IMPORT memset,CODE
	.EXPORT re_syntax_options,DATA
	.align 4
re_syntax_options
	.word 0

	.align 4
re_error_msg
	.word 0
	.word L$C0000
	.word L$C0001
	.word L$C0002
	.word L$C0003
	.word L$C0004
	.word L$C0005
	.word L$C0006
	.word L$C0007
	.word L$C0008
	.word L$C0009
	.word L$C0010
	.word L$C0011
	.word L$C0012
	.word L$C0013
	.word L$C0014
	.word L$C0015
	.code

	.align 4
L$C0015
	.STRING "Unmatched ) or \\)\x00"
	.align 4
L$C0014
	.STRING "Regular expression too big\x00"
	.align 4
L$C0013
	.STRING "Premature end of regular expression\x00"
	.align 4
L$C0012
	.STRING "Invalid preceding regular expression\x00"
	.align 4
L$C0011
	.STRING "Memory exhausted\x00"
	.align 4
L$C0010
	.STRING "Invalid range end\x00"
	.align 4
L$C0009
	.STRING "Invalid content of \\{\\}\x00"
	.align 4
L$C0008
	.STRING "Unmatched \\{\x00"
	.align 4
L$C0007
	.STRING "Unmatched ( or \\(\x00"
	.align 4
L$C0006
	.STRING "Unmatched [ or [^\x00"
	.align 4
L$C0005
	.STRING "Invalid back reference\x00"
	.align 4
L$C0004
	.STRING "Trailing backslash\x00"
	.align 4
L$C0003
	.STRING "Invalid character class name\x00"
	.align 4
L$C0002
	.STRING "Invalid collation character\x00"
	.align 4
L$C0001
	.STRING "Invalid regular expression\x00"
	.align 4
L$C0000
	.STRING "No match\x00"
	.EXPORT re_max_failures,DATA
	.data

	.align 4
re_max_failures
	.word 2000
	.IMPORT malloc,CODE
	.IMPORT realloc,CODE
	.IMPORT free,CODE
	.IMPORT strcmp,CODE
	.code

	.align 4
L$C0016
	.STRING "alnum\x00"
	.align 4
L$C0017
	.STRING "alpha\x00"
	.align 4
L$C0018
	.STRING "blank\x00"
	.align 4
L$C0019
	.STRING "cntrl\x00"
	.align 4
L$C0020
	.STRING "digit\x00"
	.align 4
L$C0021
	.STRING "graph\x00"
	.align 4
L$C0022
	.STRING "lower\x00"
	.align 4
L$C0023
	.STRING "print\x00"
	.align 4
L$C0024
	.STRING "punct\x00"
	.align 4
L$C0025
	.STRING "space\x00"
	.align 4
L$C0026
	.STRING "upper\x00"
	.align 4
L$C0027
	.STRING "xdigit\x00"
	.IMPORT __alnum,DATA
	.IMPORT __ctype2,DATA
	.IMPORT __ctype,DATA
	.IMPORT at_begline_loc_p,CODE
	.IMPORT at_endline_loc_p,CODE
	.IMPORT store_op1,CODE
	.IMPORT insert_op1,CODE
	.IMPORT store_op2,CODE
	.IMPORT insert_op2,CODE
	.IMPORT compile_range,CODE
	.IMPORT group_in_compile_stack,CODE
	.code

	.align 4
regex_compile
	.PROC
	.CALLINFO FRAME=320,CALLS,SAVE_RP,ENTRY_GR=18
	.ENTRY
	stw %r2,-20(%r30) ;# 8989 reload_outsi+2/6
	ldo 320(%r30),%r30 ;# 8991 addsi3/2
	stw %r18,-168(%r30) ;# 8993 reload_outsi+2/6
	stw %r17,-164(%r30) ;# 8995 reload_outsi+2/6
	stw %r16,-160(%r30) ;# 8997 reload_outsi+2/6
	stw %r15,-156(%r30) ;# 8999 reload_outsi+2/6
	stw %r14,-152(%r30) ;# 9001 reload_outsi+2/6
	stw %r13,-148(%r30) ;# 9003 reload_outsi+2/6
	stw %r12,-144(%r30) ;# 9005 reload_outsi+2/6
	stw %r11,-140(%r30) ;# 9007 reload_outsi+2/6
	stw %r10,-136(%r30) ;# 9009 reload_outsi+2/6
	stw %r9,-132(%r30) ;# 9011 reload_outsi+2/6
	stw %r8,-128(%r30) ;# 9013 reload_outsi+2/6
	stw %r7,-124(%r30) ;# 9015 reload_outsi+2/6
	stw %r6,-120(%r30) ;# 9017 reload_outsi+2/6
	stw %r5,-116(%r30) ;# 9019 reload_outsi+2/6
	stw %r4,-112(%r30) ;# 9021 reload_outsi+2/6
	stw %r3,-108(%r30) ;# 9023 reload_outsi+2/6
	stw %r26,-276(%r30) ;# 4 reload_outsi+2/6
	ldi 0,%r9 ;# 25 reload_outsi+2/2
	ldi 0,%r8 ;# 28 reload_outsi+2/2
	stw %r0,-260(%r30) ;# 34 reload_outsi+2/6
	ldi 0,%r10 ;# 31 reload_outsi+2/2
	ldi 640,%r26 ;# 37 reload_outsi+2/2
	ldw -276(%r30),%r1 ;# 8774 reload_outsi+2/5
	copy %r24,%r15 ;# 8 reload_outsi+2/1
	stw %r1,-296(%r30) ;# 2325 reload_outsi+2/6
	copy %r23,%r5 ;# 10 reload_outsi+2/1
	addl %r1,%r25,%r16 ;# 19 addsi3/1
	.CALL ARGW0=GR
	bl malloc,%r2 ;# 39 call_value_internal_symref
	ldw 20(%r5),%r14 ;# 22 reload_outsi+2/5
	comib,<> 0,%r28,L$0021 ;# 48 bleu+1
	stw %r28,-312(%r30) ;# 43 reload_outsi+2/6
L$0953
	bl L$0867,%r0 ;# 53 jump
	ldi 12,%r28 ;# 51 reload_outsi+2/2
L$0021
	ldi 32,%r19 ;# 58 reload_outsi+2/2
	stw %r19,-308(%r30) ;# 59 reload_outsi+2/6
	stw %r0,-304(%r30) ;# 62 reload_outsi+2/6
	stw %r15,12(%r5) ;# 65 reload_outsi+2/6
	stw %r0,8(%r5) ;# 85 reload_outsi+2/6
	stw %r0,24(%r5) ;# 88 reload_outsi+2/6
	addil LR'done___2-$global$,%r27 ;# 92 pic2_lo_sum+1
	ldw 28(%r5),%r19 ;# 68 reload_outsi+2/5
	ldw RR'done___2-$global$(%r1),%r20 ;# 94 reload_outsi+2/5
	depi 0,3,1,%r19 ;# 69 andsi3/2
	depi 0,6,2,%r19 ;# 80 andsi3/2
	comib,<> 0,%r20,L$0022 ;# 95 bleu+1
	stw %r19,28(%r5) ;# 82 reload_outsi+2/6
	addil LR're_syntax_table-$global$,%r27 ;# 99 pic2_lo_sum+1
	ldo RR're_syntax_table-$global$(%r1),%r4 ;# 100 movhi-2
	copy %r4,%r26 ;# 101 reload_outsi+2/1
	ldi 0,%r25 ;# 102 reload_outsi+2/2
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memset,%r2 ;# 104 call_value_internal_symref
	ldi 256,%r24 ;# 103 reload_outsi+2/2
	ldi 1,%r20 ;# 8732 movqi+1/2
	ldo 97(%r4),%r19 ;# 8736 addsi3/2
	ldo 122(%r4),%r4 ;# 8738 addsi3/2
	stbs,ma %r20,1(%r19) ;# 115 movqi+1/6
L$1155
	comb,>=,n %r4,%r19,L$1155 ;# 121 bleu+1
	stbs,ma %r20,1(%r19) ;# 115 movqi+1/6
	ldi 1,%r21 ;# 8717 movqi+1/2
	addil LR're_syntax_table-$global$,%r27 ;# 8712 pic2_lo_sum+1
	ldo RR're_syntax_table-$global$(%r1),%r19 ;# 8715 movhi-2
	ldo 65(%r19),%r20 ;# 8721 addsi3/2
	ldo 90(%r19),%r19 ;# 8723 addsi3/2
	stbs,ma %r21,1(%r20) ;# 138 movqi+1/6
L$1156
	comb,>=,n %r19,%r20,L$1156 ;# 144 bleu+1
	stbs,ma %r21,1(%r20) ;# 138 movqi+1/6
	ldi 48,%r20 ;# 151 reload_outsi+2/2
	ldi 57,%r22 ;# 7976 reload_outsi+2/2
	ldi 1,%r21 ;# 8707 movqi+1/2
	addil LR're_syntax_table-$global$+48,%r27 ;# 8705 pic2_lo_sum+1
	ldo RR're_syntax_table-$global$+48(%r1),%r19 ;# 8711 movhi-2
L$0037
	ldo 1(%r20),%r20 ;# 164 addsi3/2
	comb,>= %r22,%r20,L$0037 ;# 167 bleu+1
	stbs,ma %r21,1(%r19) ;# 161 movqi+1/6
	addil LR're_syntax_table-$global$,%r27 ;# 174 pic2_lo_sum+1
	ldo RR're_syntax_table-$global$(%r1),%r19 ;# 175 movhi-2
	ldi 1,%r20 ;# 176 movqi+1/2
	stb %r20,95(%r19) ;# 177 movqi+1/6
	addil LR'done___2-$global$,%r27 ;# 178 pic2_lo_sum+1
	ldi 1,%r19 ;# 180 reload_outsi+2/2
	stw %r19,RR'done___2-$global$(%r1) ;# 181 reload_outsi+2/6
L$0022
	ldw 4(%r5),%r19 ;# 187 reload_outsi+2/5
	comib,<>,n 0,%r19,L$0039 ;# 189 bleu+1
	ldw 0(%r5),%r26 ;# 193 reload_outsi+2/5
	comib,=,n 0,%r26,L$0040 ;# 195 bleu+1
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 205 call_value_internal_symref
	ldi 32,%r25 ;# 203 reload_outsi+2/2
	bl L$1157,%r0 ;# 211 jump
	stw %r28,0(%r5) ;# 223 reload_outsi+2/6
L$0040
	.CALL ARGW0=GR
	bl malloc,%r2 ;# 219 call_value_internal_symref
	ldi 32,%r26 ;# 217 reload_outsi+2/2
	stw %r28,0(%r5) ;# 223 reload_outsi+2/6
L$1157
	ldw 0(%r5),%r19 ;# 228 reload_outsi+2/5
	comib,<> 0,%r19,L$0042 ;# 230 bleu+1
	ldi 32,%r19 ;# 243 reload_outsi+2/2
	.CALL ARGW0=GR
	bl free,%r2 ;# 234 call_internal_symref
	ldw -312(%r30),%r26 ;# 232 reload_outsi+2/5
	bl L$0867,%r0 ;# 238 jump
	ldi 12,%r28 ;# 51 reload_outsi+2/2
L$0042
	stw %r19,4(%r5) ;# 244 reload_outsi+2/6
L$0039
	ldw 0(%r5),%r6 ;# 249 reload_outsi+2/5
	ldw -296(%r30),%r19 ;# 7981 reload_outsi+2/5
	comclr,<> %r16,%r19,%r0 ;# 7982 bleu+1
	bl L$0044,%r0
	copy %r6,%r12 ;# 253 reload_outsi+2/1
	ldw -296(%r30),%r19 ;# 2334 reload_outsi+2/5
L$1178
	ldbs,ma 1(%r19),%r7 ;# 277 zero_extendqisi2/2
	comib,= 0,%r14,L$0047 ;# 282 bleu+1
	stw %r19,-296(%r30) ;# 2337 reload_outsi+2/6
	addl %r14,%r7,%r19 ;# 283 addsi3/1
	ldb 0(%r19),%r7 ;# 286 zero_extendqisi2/2
L$0047
	ldo -10(%r7),%r19 ;# 7895 addsi3/2
	addi,uv -115,%r19,%r0 ;# 7896 casesi0
	blr,n %r19,%r0
	b,n L$0076
L$0863
	bl L$0376,%r0
	nop ;# 9092 switch_jump
L$0954
	bl L$0076,%r0
	nop ;# 9095 switch_jump
L$0955
	bl L$0076,%r0
	nop ;# 9098 switch_jump
L$0956
	bl L$0076,%r0
	nop ;# 9101 switch_jump
L$0957
	bl L$0076,%r0
	nop ;# 9104 switch_jump
L$0958
	bl L$0076,%r0
	nop ;# 9107 switch_jump
L$0959
	bl L$0076,%r0
	nop ;# 9110 switch_jump
L$0960
	bl L$0076,%r0
	nop ;# 9113 switch_jump
L$0961
	bl L$0076,%r0
	nop ;# 9116 switch_jump
L$0962
	bl L$0076,%r0
	nop ;# 9119 switch_jump
L$0963
	bl L$0076,%r0
	nop ;# 9122 switch_jump
L$0964
	bl L$0076,%r0
	nop ;# 9125 switch_jump
L$0965
	bl L$0076,%r0
	nop ;# 9128 switch_jump
L$0966
	bl L$0076,%r0
	nop ;# 9131 switch_jump
L$0967
	bl L$0076,%r0
	nop ;# 9134 switch_jump
L$0968
	bl L$0076,%r0
	nop ;# 9137 switch_jump
L$0969
	bl L$0076,%r0
	nop ;# 9140 switch_jump
L$0970
	bl L$0076,%r0
	nop ;# 9143 switch_jump
L$0971
	bl L$0076,%r0
	nop ;# 9146 switch_jump
L$0972
	bl L$0076,%r0
	nop ;# 9149 switch_jump
L$0973
	bl L$0076,%r0
	nop ;# 9152 switch_jump
L$0974
	bl L$0076,%r0
	nop ;# 9155 switch_jump
L$0975
	bl L$0076,%r0
	nop ;# 9158 switch_jump
L$0976
	bl L$0076,%r0
	nop ;# 9161 switch_jump
L$0977
	bl L$0076,%r0
	nop ;# 9164 switch_jump
L$0978
	bl L$0076,%r0
	nop ;# 9167 switch_jump
L$0979
	bl L$0077,%r0 ;# 9170 switch_jump
	ldw -296(%r30),%r26 ;# 2349 reload_outsi+2/5
L$0980
	bl L$0076,%r0
	nop ;# 9173 switch_jump
L$0981
	bl L$0076,%r0
	nop ;# 9176 switch_jump
L$0982
	bl L$0076,%r0
	nop ;# 9179 switch_jump
L$0983
	bl L$0368,%r0
	nop ;# 9182 switch_jump
L$0984
	bl L$0372,%r0
	nop ;# 9185 switch_jump
L$0985
	bl L$0104,%r0
	nop ;# 9188 switch_jump
L$0986
	bl L$1158,%r0 ;# 9191 switch_jump
	ldi 1026,%r19 ;# 662 reload_outsi+2/2
L$0987
	bl L$0076,%r0
	nop ;# 9194 switch_jump
L$0988
	bl L$0076,%r0
	nop ;# 9197 switch_jump
L$0989
	bl L$0196,%r0 ;# 9200 switch_jump
	ldw 0(%r5),%r4 ;# 8027 reload_outsi+2/5
L$0990
	bl L$0076,%r0
	nop ;# 9203 switch_jump
L$0991
	bl L$0076,%r0
	nop ;# 9206 switch_jump
L$0992
	bl L$0076,%r0
	nop ;# 9209 switch_jump
L$0993
	bl L$0076,%r0
	nop ;# 9212 switch_jump
L$0994
	bl L$0076,%r0
	nop ;# 9215 switch_jump
L$0995
	bl L$0076,%r0
	nop ;# 9218 switch_jump
L$0996
	bl L$0076,%r0
	nop ;# 9221 switch_jump
L$0997
	bl L$0076,%r0
	nop ;# 9224 switch_jump
L$0998
	bl L$0076,%r0
	nop ;# 9227 switch_jump
L$0999
	bl L$0076,%r0
	nop ;# 9230 switch_jump
L$1000
	bl L$0076,%r0
	nop ;# 9233 switch_jump
L$1001
	bl L$0076,%r0
	nop ;# 9236 switch_jump
L$1002
	bl L$0076,%r0
	nop ;# 9239 switch_jump
L$1003
	bl L$0076,%r0
	nop ;# 9242 switch_jump
L$1004
	bl L$0076,%r0
	nop ;# 9245 switch_jump
L$1005
	bl L$0076,%r0
	nop ;# 9248 switch_jump
L$1006
	bl L$0101,%r0 ;# 9251 switch_jump
	ldi 1026,%r19 ;# 662 reload_outsi+2/2
L$1007
	bl L$0076,%r0
	nop ;# 9254 switch_jump
L$1008
	bl L$0076,%r0
	nop ;# 9257 switch_jump
L$1009
	bl L$0076,%r0
	nop ;# 9260 switch_jump
L$1010
	bl L$0076,%r0
	nop ;# 9263 switch_jump
L$1011
	bl L$0076,%r0
	nop ;# 9266 switch_jump
L$1012
	bl L$0076,%r0
	nop ;# 9269 switch_jump
L$1013
	bl L$0076,%r0
	nop ;# 9272 switch_jump
L$1014
	bl L$0076,%r0
	nop ;# 9275 switch_jump
L$1015
	bl L$0076,%r0
	nop ;# 9278 switch_jump
L$1016
	bl L$0076,%r0
	nop ;# 9281 switch_jump
L$1017
	bl L$0076,%r0
	nop ;# 9284 switch_jump
L$1018
	bl L$0076,%r0
	nop ;# 9287 switch_jump
L$1019
	bl L$0076,%r0
	nop ;# 9290 switch_jump
L$1020
	bl L$0076,%r0
	nop ;# 9293 switch_jump
L$1021
	bl L$0076,%r0
	nop ;# 9296 switch_jump
L$1022
	bl L$0076,%r0
	nop ;# 9299 switch_jump
L$1023
	bl L$0076,%r0
	nop ;# 9302 switch_jump
L$1024
	bl L$0076,%r0
	nop ;# 9305 switch_jump
L$1025
	bl L$0076,%r0
	nop ;# 9308 switch_jump
L$1026
	bl L$0076,%r0
	nop ;# 9311 switch_jump
L$1027
	bl L$0076,%r0
	nop ;# 9314 switch_jump
L$1028
	bl L$0076,%r0
	nop ;# 9317 switch_jump
L$1029
	bl L$0076,%r0
	nop ;# 9320 switch_jump
L$1030
	bl L$0076,%r0
	nop ;# 9323 switch_jump
L$1031
	bl L$0076,%r0
	nop ;# 9326 switch_jump
L$1032
	bl L$0076,%r0
	nop ;# 9329 switch_jump
L$1033
	bl L$0076,%r0
	nop ;# 9332 switch_jump
L$1034
	bl L$0216,%r0 ;# 9335 switch_jump
	ldw -296(%r30),%r19 ;# 2418 reload_outsi+2/5
L$1035
	bl L$0387,%r0 ;# 9338 switch_jump
	ldw -296(%r30),%r19 ;# 3797 reload_outsi+2/5
L$1036
	bl L$0076,%r0
	nop ;# 9341 switch_jump
L$1037
	bl L$0053,%r0 ;# 9344 switch_jump
	ldw -276(%r30),%r1 ;# 8777 reload_outsi+2/5
L$1038
	bl L$0076,%r0
	nop ;# 9347 switch_jump
L$1039
	bl L$0076,%r0
	nop ;# 9350 switch_jump
L$1040
	bl L$0076,%r0
	nop ;# 9353 switch_jump
L$1041
	bl L$0076,%r0
	nop ;# 9356 switch_jump
L$1042
	bl L$0076,%r0
	nop ;# 9359 switch_jump
L$1043
	bl L$0076,%r0
	nop ;# 9362 switch_jump
L$1044
	bl L$0076,%r0
	nop ;# 9365 switch_jump
L$1045
	bl L$0076,%r0
	nop ;# 9368 switch_jump
L$1046
	bl L$0076,%r0
	nop ;# 9371 switch_jump
L$1047
	bl L$0076,%r0
	nop ;# 9374 switch_jump
L$1048
	bl L$0076,%r0
	nop ;# 9377 switch_jump
L$1049
	bl L$0076,%r0
	nop ;# 9380 switch_jump
L$1050
	bl L$0076,%r0
	nop ;# 9383 switch_jump
L$1051
	bl L$0076,%r0
	nop ;# 9386 switch_jump
L$1052
	bl L$0076,%r0
	nop ;# 9389 switch_jump
L$1053
	bl L$0076,%r0
	nop ;# 9392 switch_jump
L$1054
	bl L$0076,%r0
	nop ;# 9395 switch_jump
L$1055
	bl L$0076,%r0
	nop ;# 9398 switch_jump
L$1056
	bl L$0076,%r0
	nop ;# 9401 switch_jump
L$1057
	bl L$0076,%r0
	nop ;# 9404 switch_jump
L$1058
	bl L$0076,%r0
	nop ;# 9407 switch_jump
L$1059
	bl L$0076,%r0
	nop ;# 9410 switch_jump
L$1060
	bl L$0076,%r0
	nop ;# 9413 switch_jump
L$1061
	bl L$0076,%r0
	nop ;# 9416 switch_jump
L$1062
	bl L$0076,%r0
	nop ;# 9419 switch_jump
L$1063
	bl L$0076,%r0
	nop ;# 9422 switch_jump
L$1064
	bl L$0076,%r0
	nop ;# 9425 switch_jump
L$1065
	bl L$0076,%r0
	nop ;# 9428 switch_jump
L$1066
	bl L$0383,%r0 ;# 9431 switch_jump
	ldi 4608,%r20 ;# 3778 reload_outsi+2/2
L$1067
	bl L$0380,%r0
	nop ;# 9434 switch_jump
L$1068
	bl,n L$0076,%r0 ;# 7899 jump
L$0053
	ldw -296(%r30),%r25 ;# 2343 reload_outsi+2/5
	ldo 1(%r1),%r19 ;# 306 addsi3/2
	comb,=,n %r19,%r25,L$0055 ;# 308 bleu+1
	bb,< %r15,28,L$0055 ;# 313 bleu+3
	ldw -276(%r30),%r26 ;# 315 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl at_begline_loc_p,%r2 ;# 321 call_value_internal_symref
	copy %r15,%r24 ;# 319 reload_outsi+2/1
	extrs %r28,31,8,%r28 ;# 324 extendqisi2
	comiclr,<> 0,%r28,%r0 ;# 326 bleu+1
	bl,n L$0076,%r0
L$0055
	ldw 0(%r5),%r4 ;# 7986 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 7989 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 7987 subsi3/1
	ldo 1(%r19),%r19 ;# 7988 addsi3/2
	comb,>>=,n %r20,%r19,L$0060 ;# 7990 bleu+1
	ldil L'65536,%r3 ;# 8701 reload_outsi+2/3
L$0061
	comclr,<> %r3,%r20,%r0 ;# 357 bleu+1
	bl L$0944,%r0
	zdep %r20,30,31,%r19 ;# 367 ashlsi3+1
	comb,>>= %r3,%r19,L$0066 ;# 375 bleu+1
	stw %r19,4(%r5) ;# 369 reload_outsi+2/6
	stw %r3,4(%r5) ;# 378 reload_outsi+2/6
L$0066
	ldw 0(%r5),%r26 ;# 385 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 389 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 387 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 397 bleu+1
	stw %r28,0(%r5) ;# 393 reload_outsi+2/6
	comb,= %r28,%r4,L$0059 ;# 407 bleu+1
	sub %r6,%r4,%r19 ;# 409 subsi3/1
	addl %r28,%r19,%r6 ;# 412 addsi3/1
	sub %r12,%r4,%r19 ;# 413 subsi3/1
	comib,= 0,%r10,L$0069 ;# 418 bleu+1
	addl %r28,%r19,%r12 ;# 416 addsi3/1
	sub %r10,%r4,%r19 ;# 419 subsi3/1
	addl %r28,%r19,%r10 ;# 422 addsi3/1
L$0069
	comib,= 0,%r8,L$0070 ;# 425 bleu+1
	sub %r8,%r4,%r19 ;# 426 subsi3/1
	addl %r28,%r19,%r8 ;# 429 addsi3/1
L$0070
	comib,= 0,%r9,L$0059 ;# 432 bleu+1
	sub %r9,%r4,%r19 ;# 433 subsi3/1
	addl %r28,%r19,%r9 ;# 436 addsi3/1
L$0059
	ldw 0(%r5),%r4 ;# 337 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 341 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 338 subsi3/1
	ldo 1(%r19),%r19 ;# 339 addsi3/2
	comb,<< %r20,%r19,L$0061
	nop ;# 343 bleu+1
L$0060
	ldi 8,%r19 ;# 458 movqi+1/2
	bl L$0043,%r0 ;# 479 jump
	stbs,ma %r19,1(%r6) ;# 459 movqi+1/6
L$0077
	comb,=,n %r16,%r26,L$0079 ;# 485 bleu+1
	bb,< %r15,28,L$0079 ;# 490 bleu+3
	copy %r16,%r25 ;# 494 reload_outsi+2/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl at_endline_loc_p,%r2 ;# 498 call_value_internal_symref
	copy %r15,%r24 ;# 496 reload_outsi+2/1
	extrs %r28,31,8,%r28 ;# 501 extendqisi2
	comiclr,<> 0,%r28,%r0 ;# 503 bleu+1
	bl,n L$0076,%r0
L$0079
	ldw 0(%r5),%r4 ;# 7994 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 7997 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 7995 subsi3/1
	ldo 1(%r19),%r19 ;# 7996 addsi3/2
	comb,>>=,n %r20,%r19,L$0084 ;# 7998 bleu+1
	ldil L'65536,%r3 ;# 8699 reload_outsi+2/3
L$0085
	comclr,<> %r3,%r20,%r0 ;# 534 bleu+1
	bl L$0944,%r0
	zdep %r20,30,31,%r19 ;# 544 ashlsi3+1
	comb,>>= %r3,%r19,L$0090 ;# 552 bleu+1
	stw %r19,4(%r5) ;# 546 reload_outsi+2/6
	stw %r3,4(%r5) ;# 555 reload_outsi+2/6
L$0090
	ldw 0(%r5),%r26 ;# 562 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 566 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 564 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 574 bleu+1
	stw %r28,0(%r5) ;# 570 reload_outsi+2/6
	comb,= %r28,%r4,L$0083 ;# 584 bleu+1
	sub %r6,%r4,%r19 ;# 586 subsi3/1
	addl %r28,%r19,%r6 ;# 589 addsi3/1
	sub %r12,%r4,%r19 ;# 590 subsi3/1
	comib,= 0,%r10,L$0093 ;# 595 bleu+1
	addl %r28,%r19,%r12 ;# 593 addsi3/1
	sub %r10,%r4,%r19 ;# 596 subsi3/1
	addl %r28,%r19,%r10 ;# 599 addsi3/1
L$0093
	comib,= 0,%r8,L$0094 ;# 602 bleu+1
	sub %r8,%r4,%r19 ;# 603 subsi3/1
	addl %r28,%r19,%r8 ;# 606 addsi3/1
L$0094
	comib,= 0,%r9,L$0083 ;# 609 bleu+1
	sub %r9,%r4,%r19 ;# 610 subsi3/1
	addl %r28,%r19,%r9 ;# 613 addsi3/1
L$0083
	ldw 0(%r5),%r4 ;# 514 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 518 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 515 subsi3/1
	ldo 1(%r19),%r19 ;# 516 addsi3/2
	comb,<< %r20,%r19,L$0085
	nop ;# 520 bleu+1
L$0084
	ldi 9,%r19 ;# 635 movqi+1/2
	bl L$0043,%r0 ;# 656 jump
	stbs,ma %r19,1(%r6) ;# 636 movqi+1/6
L$0877
	bl L$0110,%r0 ;# 897 jump
	stw %r21,-296(%r30) ;# 2391 reload_outsi+2/6
L$0101
L$1158
	and %r15,%r19,%r19 ;# 663 andsi3/1
	comiclr,= 0,%r19,%r0 ;# 665 bleu+1
	bl,n L$0076,%r0
L$0104
	comib,<> 0,%r8,L$0105 ;# 674 bleu+1
	ldi 0,%r13 ;# 711 reload_outsi+2/2
	extrs,>= %r15,26,1,%r0 ;# 681 bleu+3
	extrs,< %r15,27,1,%r0 ;# 700 movsi-4
	nop
	bl,n L$0076,%r0
L$0105
	ldi 0,%r11 ;# 714 reload_outsi+2/2
	ldi 0,%r22 ;# 716 reload_outsi+2/2
	ldi 43,%r24 ;# 8688 reload_outsi+2/2
	ldi 63,%r23 ;# 8690 reload_outsi+2/2
	ldi 42,%r28 ;# 8692 reload_outsi+2/2
	ldi 2,%r19 ;# 8694 reload_outsi+2/2
	and %r15,%r19,%r25 ;# 8695 andsi3/1
	ldi 92,%r26 ;# 8697 reload_outsi+2/2
L$0109
	comb,= %r24,%r7,L$0112 ;# 727 bleu+1
	copy %r11,%r19 ;# 8780 reload_outsi+2/1
	depi -1,31,1,%r19 ;# 729 iorsi3+1/2
	bl L$0113,%r0 ;# 731 jump
	extrs %r19,31,8,%r19 ;# 730 extendqisi2
L$0112
	extrs %r11,31,8,%r19 ;# 734 extendqisi2
L$0113
L$1159
	comb,= %r23,%r7,L$0114 ;# 744 bleu+1
	copy %r19,%r11 ;# 737 reload_outsi+2/1
	copy %r22,%r19 ;# 8783 reload_outsi+2/1
	depi -1,31,1,%r19 ;# 746 iorsi3+1/2
	bl L$0115,%r0 ;# 748 jump
	extrs %r19,31,8,%r19 ;# 747 extendqisi2
L$0114
	extrs %r22,31,8,%r19 ;# 751 extendqisi2
L$0115
	ldw -296(%r30),%r21 ;# 2355 reload_outsi+2/5
	comb,= %r16,%r21,L$0110 ;# 757 bleu+1
	copy %r19,%r22 ;# 754 reload_outsi+2/1
	copy %r21,%r20 ;# 8743 reload_outsi+2/1
	ldbs,ma 1(%r20),%r7 ;# 776 zero_extendqisi2/2
	comib,= 0,%r14,L$0118 ;# 781 bleu+1
	stw %r20,-296(%r30) ;# 2364 reload_outsi+2/6
	addl %r14,%r7,%r19 ;# 782 addsi3/1
	ldb 0(%r19),%r7 ;# 785 zero_extendqisi2/2
L$0118
	comb,= %r28,%r7,L$0109
	nop ;# 802 bleu+1
	comib,<>,n 0,%r25,L$0869 ;# 807 bleu+1
	comb,= %r24,%r7,L$1159 ;# 811 bleu+1
	extrs %r11,31,8,%r19 ;# 734 extendqisi2
	comb,= %r23,%r7,L$0109 ;# 815 bleu+1
	ldw -296(%r30),%r19 ;# 2400 reload_outsi+2/5
	bl,n L$1160,%r0 ;# 827 jump
L$0869
	comb,<> %r26,%r7,L$0126 ;# 831 bleu+1
	ldw -296(%r30),%r19 ;# 2400 reload_outsi+2/5
	comclr,<> %r16,%r20,%r0 ;# 835 bleu+1
	bl L$0903,%r0
	ldo 1(%r20),%r19 ;# 863 addsi3/2
	ldb 1(%r21),%r3 ;# 860 zero_extendqisi2/2
	comib,= 0,%r14,L$0129 ;# 865 bleu+1
	stw %r19,-296(%r30) ;# 2379 reload_outsi+2/6
	addl %r14,%r3,%r19 ;# 866 addsi3/1
	ldb 0(%r19),%r3 ;# 869 zero_extendqisi2/2
L$0129
	comb,= %r24,%r3,L$0109 ;# 886 bleu+1
	copy %r3,%r7 ;# 903 reload_outsi+2/1
	comb,<> %r23,%r3,L$0877
	nop ;# 890 bleu+1
	bl,n L$0109,%r0 ;# 905 jump
L$0126
L$1160
	ldo -1(%r19),%r19 ;# 910 addsi3/2
	stw %r19,-296(%r30) ;# 2397 reload_outsi+2/6
L$0110
	comiclr,<> 0,%r8,%r0 ;# 927 bleu+1
	bl L$1161,%r0
	ldw -296(%r30),%r19 ;# 2328 reload_outsi+2/5
	comib,=,n 0,%r22,L$0137 ;# 934 bleu+1
	ldw 0(%r5),%r3 ;# 8002 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8005 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 8003 subsi3/1
	ldo 3(%r19),%r19 ;# 8004 addsi3/2
	comb,>>=,n %r20,%r19,L$0139 ;# 8006 bleu+1
	ldil L'65536,%r4 ;# 8686 reload_outsi+2/3
L$0140
	comclr,<> %r4,%r20,%r0 ;# 961 bleu+1
	bl L$0944,%r0
	zdep %r20,30,31,%r19 ;# 971 ashlsi3+1
	comb,>>= %r4,%r19,L$0145 ;# 979 bleu+1
	stw %r19,4(%r5) ;# 973 reload_outsi+2/6
	stw %r4,4(%r5) ;# 982 reload_outsi+2/6
L$0145
	ldw 0(%r5),%r26 ;# 989 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 993 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 991 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 1001 bleu+1
	stw %r28,0(%r5) ;# 997 reload_outsi+2/6
	comb,= %r28,%r3,L$0138 ;# 1011 bleu+1
	sub %r6,%r3,%r19 ;# 1013 subsi3/1
	addl %r28,%r19,%r6 ;# 1016 addsi3/1
	sub %r12,%r3,%r19 ;# 1017 subsi3/1
	comib,= 0,%r10,L$0148 ;# 1022 bleu+1
	addl %r28,%r19,%r12 ;# 1020 addsi3/1
	sub %r10,%r3,%r19 ;# 1023 subsi3/1
	addl %r28,%r19,%r10 ;# 1026 addsi3/1
L$0148
	comib,= 0,%r8,L$0149 ;# 1029 bleu+1
	sub %r8,%r3,%r19 ;# 1030 subsi3/1
	addl %r28,%r19,%r8 ;# 1033 addsi3/1
L$0149
	comib,= 0,%r9,L$0138 ;# 1036 bleu+1
	sub %r9,%r3,%r19 ;# 1037 subsi3/1
	addl %r28,%r19,%r9 ;# 1040 addsi3/1
L$0138
	ldw 0(%r5),%r3 ;# 941 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 945 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 942 subsi3/1
	ldo 3(%r19),%r19 ;# 943 addsi3/2
	comb,<< %r20,%r19,L$0140
	nop ;# 947 bleu+1
L$0139
	comib,= 0,%r14,L$0154 ;# 1063 bleu+1
	ldw -296(%r30),%r19 ;# 2403 reload_outsi+2/5
	ldb -2(%r19),%r19 ;# 1066 zero_extendqisi2/2
	addl %r14,%r19,%r19 ;# 1067 addsi3/1
	bl L$0947,%r0 ;# 1071 jump
	ldb 0(%r19),%r19 ;# 1069 movqi+1/5
L$0154
	ldb -2(%r19),%r19 ;# 1075 movqi+1/5
L$0947
	comib,= 0,%r14,L$0156 ;# 1079 bleu+1
	extrs %r19,31,8,%r20 ;# 1076 extendqisi2
	ldb 46(%r14),%r19 ;# 1081 movqi+1/5
	extrs %r19,31,8,%r19 ;# 1082 extendqisi2
	comb,= %r19,%r20,L$0157 ;# 1084 bleu+1
	ldi 17,%r26 ;# 1159 reload_outsi+2/2
	bl,n L$1162,%r0 ;# 1085 jump
L$0156
	ldi 46,%r19 ;# 1089 reload_outsi+2/2
	comb,<> %r19,%r20,L$1162 ;# 1091 bleu+1
	ldi 17,%r26 ;# 1159 reload_outsi+2/2
L$0157
	comib,= 0,%r11,L$0153 ;# 1096 bleu+1
	ldw -296(%r30),%r19 ;# 2409 reload_outsi+2/5
	comb,<<= %r16,%r19,L$1162 ;# 1098 bleu+1
	ldi 17,%r26 ;# 1159 reload_outsi+2/2
	comib,=,n 0,%r14,L$0158 ;# 1100 bleu+1
	ldb 0(%r19),%r19 ;# 1103 zero_extendqisi2/2
	addl %r14,%r19,%r19 ;# 1104 addsi3/1
L$0158
	ldb 0(%r19),%r19 ;# 1112 movqi+1/5
	comib,= 0,%r14,L$0160 ;# 1116 bleu+1
	extrs %r19,31,8,%r20 ;# 1113 extendqisi2
	ldb 10(%r14),%r19 ;# 1118 movqi+1/5
	extrs %r19,31,8,%r19 ;# 1119 extendqisi2
	comb,= %r19,%r20,L$0161 ;# 1121 bleu+1
	ldi 17,%r26 ;# 1159 reload_outsi+2/2
	bl,n L$1162,%r0 ;# 1122 jump
L$0160
	comib,<> 10,%r20,L$1162 ;# 1126 bleu+1
	ldi 17,%r26 ;# 1159 reload_outsi+2/2
L$0161
	bb,< %r15,25,L$1162 ;# 1134 bleu+3
	ldi 17,%r26 ;# 1159 reload_outsi+2/2
	ldi 12,%r26 ;# 1140 reload_outsi+2/2
	copy %r6,%r25 ;# 1142 reload_outsi+2/1
	sub %r8,%r6,%r24 ;# 1137 subsi3/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl store_op1,%r2 ;# 1146 call_internal_symref
	ldo -3(%r24),%r24 ;# 1144 addsi3/2
	bl L$0162,%r0 ;# 1151 jump
	ldi 1,%r13 ;# 1149 reload_outsi+2/2
L$0153
	ldi 17,%r26 ;# 1159 reload_outsi+2/2
L$1162
	copy %r6,%r25 ;# 1161 reload_outsi+2/1
	sub %r8,%r6,%r24 ;# 1156 subsi3/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl store_op1,%r2 ;# 1165 call_internal_symref
	ldo -6(%r24),%r24 ;# 1163 addsi3/2
L$0162
	ldo 3(%r6),%r6 ;# 1168 addsi3/2
L$0137
	ldw 0(%r5),%r3 ;# 8010 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8013 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 8011 subsi3/1
	ldo 3(%r19),%r19 ;# 8012 addsi3/2
	comb,>>=,n %r20,%r19,L$0164 ;# 8014 bleu+1
	ldil L'65536,%r4 ;# 8684 reload_outsi+2/3
L$0165
	comclr,<> %r4,%r20,%r0 ;# 1195 bleu+1
	bl L$0944,%r0
	zdep %r20,30,31,%r19 ;# 1205 ashlsi3+1
	comb,>>= %r4,%r19,L$0170 ;# 1213 bleu+1
	stw %r19,4(%r5) ;# 1207 reload_outsi+2/6
	stw %r4,4(%r5) ;# 1216 reload_outsi+2/6
L$0170
	ldw 0(%r5),%r26 ;# 1223 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 1227 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 1225 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 1235 bleu+1
	stw %r28,0(%r5) ;# 1231 reload_outsi+2/6
	comb,= %r28,%r3,L$0163 ;# 1245 bleu+1
	sub %r6,%r3,%r19 ;# 1247 subsi3/1
	addl %r28,%r19,%r6 ;# 1250 addsi3/1
	sub %r12,%r3,%r19 ;# 1251 subsi3/1
	comib,= 0,%r10,L$0173 ;# 1256 bleu+1
	addl %r28,%r19,%r12 ;# 1254 addsi3/1
	sub %r10,%r3,%r19 ;# 1257 subsi3/1
	addl %r28,%r19,%r10 ;# 1260 addsi3/1
L$0173
	comib,= 0,%r8,L$0174 ;# 1263 bleu+1
	sub %r8,%r3,%r19 ;# 1264 subsi3/1
	addl %r28,%r19,%r8 ;# 1267 addsi3/1
L$0174
	comib,= 0,%r9,L$0163 ;# 1270 bleu+1
	sub %r9,%r3,%r19 ;# 1271 subsi3/1
	addl %r28,%r19,%r9 ;# 1274 addsi3/1
L$0163
	ldw 0(%r5),%r3 ;# 1175 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 1179 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 1176 subsi3/1
	ldo 3(%r19),%r19 ;# 1177 addsi3/2
	comb,<< %r20,%r19,L$0165
	nop ;# 1181 bleu+1
L$0164
	ldi 14,%r26 ;# 8786 reload_outsi+2/2
	comiclr,= 0,%r13,%r0 ;# 1310 beq-1/2
	ldi 15,%r26
	copy %r8,%r25 ;# 1312 reload_outsi+2/1
	sub %r6,%r8,%r24 ;# 1314 subsi3/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl insert_op1,%r2 ;# 1318 call_internal_symref
	copy %r6,%r23 ;# 1316 reload_outsi+2/1
	ldi 0,%r9 ;# 1321 reload_outsi+2/2
	comib,<> 0,%r11,L$0043 ;# 1326 bleu+1
	ldo 3(%r6),%r6 ;# 1323 addsi3/2
	ldw 0(%r5),%r3 ;# 8019 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8022 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 8020 subsi3/1
	ldo 3(%r19),%r19 ;# 8021 addsi3/2
	comb,>>=,n %r20,%r19,L$0182 ;# 8023 bleu+1
	ldil L'65536,%r4 ;# 8682 reload_outsi+2/3
L$0183
	comb,= %r4,%r20,L$0944 ;# 1352 bleu+1
	zdep %r20,30,31,%r19 ;# 1362 ashlsi3+1
	comb,>>= %r4,%r19,L$0188 ;# 1370 bleu+1
	stw %r19,4(%r5) ;# 1364 reload_outsi+2/6
	stw %r4,4(%r5) ;# 1373 reload_outsi+2/6
L$0188
	ldw 0(%r5),%r26 ;# 1380 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 1384 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 1382 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 1392 bleu+1
	stw %r28,0(%r5) ;# 1388 reload_outsi+2/6
	comb,= %r28,%r3,L$0181 ;# 1402 bleu+1
	sub %r6,%r3,%r19 ;# 1404 subsi3/1
	addl %r28,%r19,%r6 ;# 1407 addsi3/1
	sub %r12,%r3,%r19 ;# 1408 subsi3/1
	comib,= 0,%r10,L$0191 ;# 1413 bleu+1
	addl %r28,%r19,%r12 ;# 1411 addsi3/1
	sub %r10,%r3,%r19 ;# 1414 subsi3/1
	addl %r28,%r19,%r10 ;# 1417 addsi3/1
L$0191
	comib,= 0,%r8,L$0192 ;# 1420 bleu+1
	sub %r8,%r3,%r19 ;# 1421 subsi3/1
	addl %r28,%r19,%r8 ;# 1424 addsi3/1
L$0192
	comib,= 0,%r9,L$0181 ;# 1427 bleu+1
	sub %r9,%r3,%r19 ;# 1428 subsi3/1
	addl %r28,%r19,%r9 ;# 1431 addsi3/1
L$0181
	ldw 0(%r5),%r3 ;# 1332 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 1336 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 1333 subsi3/1
	ldo 3(%r19),%r19 ;# 1334 addsi3/2
	comb,<< %r20,%r19,L$0183
	nop ;# 1338 bleu+1
L$0182
	ldi 18,%r26 ;# 1454 reload_outsi+2/2
	copy %r8,%r25 ;# 1456 reload_outsi+2/1
	ldi 3,%r24 ;# 1458 reload_outsi+2/2
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl insert_op1,%r2 ;# 1462 call_internal_symref
	copy %r6,%r23 ;# 1460 reload_outsi+2/1
	bl L$0043,%r0 ;# 1470 jump
	ldo 3(%r6),%r6 ;# 1464 addsi3/2
L$0196
	ldw 4(%r5),%r20 ;# 8030 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8028 subsi3/1
	ldo 1(%r19),%r19 ;# 8029 addsi3/2
	comb,>>= %r20,%r19,L$0201 ;# 8031 bleu+1
	copy %r6,%r8 ;# 1475 reload_outsi+2/1
	ldil L'65536,%r3 ;# 8680 reload_outsi+2/3
L$0202
	comb,= %r3,%r20,L$0944 ;# 1503 bleu+1
	zdep %r20,30,31,%r19 ;# 1513 ashlsi3+1
	comb,>>= %r3,%r19,L$0207 ;# 1521 bleu+1
	stw %r19,4(%r5) ;# 1515 reload_outsi+2/6
	stw %r3,4(%r5) ;# 1524 reload_outsi+2/6
L$0207
	ldw 0(%r5),%r26 ;# 1531 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 1535 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 1533 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 1543 bleu+1
	stw %r28,0(%r5) ;# 1539 reload_outsi+2/6
	comb,= %r28,%r4,L$0200 ;# 1553 bleu+1
	sub %r6,%r4,%r19 ;# 1555 subsi3/1
	addl %r28,%r19,%r6 ;# 1558 addsi3/1
	sub %r12,%r4,%r19 ;# 1559 subsi3/1
	comib,= 0,%r10,L$0210 ;# 1564 bleu+1
	addl %r28,%r19,%r12 ;# 1562 addsi3/1
	sub %r10,%r4,%r19 ;# 1565 subsi3/1
	addl %r28,%r19,%r10 ;# 1568 addsi3/1
L$0210
	comib,= 0,%r8,L$0211 ;# 1571 bleu+1
	sub %r8,%r4,%r19 ;# 1572 subsi3/1
	addl %r28,%r19,%r8 ;# 1575 addsi3/1
L$0211
	comib,= 0,%r9,L$0200 ;# 1578 bleu+1
	sub %r9,%r4,%r19 ;# 1579 subsi3/1
	addl %r28,%r19,%r9 ;# 1582 addsi3/1
L$0200
	ldw 0(%r5),%r4 ;# 1483 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 1487 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 1484 subsi3/1
	ldo 1(%r19),%r19 ;# 1485 addsi3/2
	comb,<< %r20,%r19,L$0202
	nop ;# 1489 bleu+1
L$0201
	ldi 2,%r19 ;# 1604 movqi+1/2
	bl L$0043,%r0 ;# 1617 jump
	stbs,ma %r19,1(%r6) ;# 1605 movqi+1/6
L$0216
	comb,= %r16,%r19,L$0902 ;# 1626 bleu+1
	ldi 0,%r13 ;# 1623 reload_outsi+2/2
	ldw 0(%r5),%r3 ;# 8035 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8038 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 8036 subsi3/1
	ldo 34(%r19),%r19 ;# 8037 addsi3/2
	comb,>>= %r20,%r19,L$0219 ;# 8039 bleu+1
	ldil L'65536,%r4 ;# 8678 reload_outsi+2/3
L$0220
	comb,= %r4,%r20,L$0944 ;# 1661 bleu+1
	zdep %r20,30,31,%r19 ;# 1671 ashlsi3+1
	comb,>>= %r4,%r19,L$0225 ;# 1679 bleu+1
	stw %r19,4(%r5) ;# 1673 reload_outsi+2/6
	stw %r4,4(%r5) ;# 1682 reload_outsi+2/6
L$0225
	ldw 0(%r5),%r26 ;# 1689 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 1693 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 1691 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 1701 bleu+1
	stw %r28,0(%r5) ;# 1697 reload_outsi+2/6
	comb,= %r28,%r3,L$0218 ;# 1711 bleu+1
	sub %r6,%r3,%r19 ;# 1713 subsi3/1
	addl %r28,%r19,%r6 ;# 1716 addsi3/1
	sub %r12,%r3,%r19 ;# 1717 subsi3/1
	comib,= 0,%r10,L$0228 ;# 1722 bleu+1
	addl %r28,%r19,%r12 ;# 1720 addsi3/1
	sub %r10,%r3,%r19 ;# 1723 subsi3/1
	addl %r28,%r19,%r10 ;# 1726 addsi3/1
L$0228
	comib,= 0,%r8,L$0229 ;# 1729 bleu+1
	sub %r8,%r3,%r19 ;# 1730 subsi3/1
	addl %r28,%r19,%r8 ;# 1733 addsi3/1
L$0229
	comib,= 0,%r9,L$0218 ;# 1736 bleu+1
	sub %r9,%r3,%r19 ;# 1737 subsi3/1
	addl %r28,%r19,%r9 ;# 1740 addsi3/1
L$0218
	ldw 0(%r5),%r3 ;# 1641 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 1645 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 1642 subsi3/1
	ldo 34(%r19),%r19 ;# 1643 addsi3/2
	comb,<< %r20,%r19,L$0220
	nop ;# 1647 bleu+1
L$0219
	ldw 0(%r5),%r4 ;# 8043 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8046 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8044 subsi3/1
	ldo 1(%r19),%r19 ;# 8045 addsi3/2
	comb,>>= %r20,%r19,L$0237 ;# 8047 bleu+1
	copy %r6,%r8 ;# 1763 reload_outsi+2/1
	ldil L'65536,%r3 ;# 8676 reload_outsi+2/3
L$0238
	comb,= %r3,%r20,L$0944 ;# 1791 bleu+1
	zdep %r20,30,31,%r19 ;# 1801 ashlsi3+1
	comb,>>= %r3,%r19,L$0243 ;# 1809 bleu+1
	stw %r19,4(%r5) ;# 1803 reload_outsi+2/6
	stw %r3,4(%r5) ;# 1812 reload_outsi+2/6
L$0243
	ldw 0(%r5),%r26 ;# 1819 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 1823 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 1821 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 1831 bleu+1
	stw %r28,0(%r5) ;# 1827 reload_outsi+2/6
	comb,= %r28,%r4,L$0236 ;# 1841 bleu+1
	sub %r6,%r4,%r19 ;# 1843 subsi3/1
	addl %r28,%r19,%r6 ;# 1846 addsi3/1
	sub %r12,%r4,%r19 ;# 1847 subsi3/1
	comib,= 0,%r10,L$0246 ;# 1852 bleu+1
	addl %r28,%r19,%r12 ;# 1850 addsi3/1
	sub %r10,%r4,%r19 ;# 1853 subsi3/1
	addl %r28,%r19,%r10 ;# 1856 addsi3/1
L$0246
	comib,= 0,%r8,L$0247 ;# 1859 bleu+1
	sub %r8,%r4,%r19 ;# 1860 subsi3/1
	addl %r28,%r19,%r8 ;# 1863 addsi3/1
L$0247
	comib,= 0,%r9,L$0236 ;# 1866 bleu+1
	sub %r9,%r4,%r19 ;# 1867 subsi3/1
	addl %r28,%r19,%r9 ;# 1870 addsi3/1
L$0236
	ldw 0(%r5),%r4 ;# 1771 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 1775 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 1772 subsi3/1
	ldo 1(%r19),%r19 ;# 1773 addsi3/2
	comb,<< %r20,%r19,L$0238
	nop ;# 1777 bleu+1
L$0237
	copy %r6,%r22 ;# 1909 reload_outsi+2/1
	ldo 1(%r6),%r6 ;# 1891 addsi3/2
	ldw -296(%r30),%r19 ;# 2421 reload_outsi+2/5
	ldb 0(%r19),%r19 ;# 1893 movqi+1/5
	ldi 94,%r21 ;# 1896 reload_outsi+2/2
	extrs %r19,31,8,%r19 ;# 1894 extendqisi2
	comb,<> %r21,%r19,L$0251 ;# 1898 bleu+1
	ldi 3,%r20 ;# 8051 movqi+1/2
	ldi 4,%r20 ;# 1900 movqi+1/2
L$0251
	stb %r20,0(%r22) ;# 1911 movqi+1/6
	ldw -296(%r30),%r20 ;# 2424 reload_outsi+2/5
	ldb 0(%r20),%r19 ;# 1923 movqi+1/5
	extrs %r19,31,8,%r19 ;# 1924 extendqisi2
	comb,<> %r21,%r19,L$0254 ;# 1928 bleu+1
	ldo 1(%r20),%r19 ;# 1930 addsi3/2
	stw %r19,-296(%r30) ;# 2427 reload_outsi+2/6
L$0254
	ldw 0(%r5),%r4 ;# 8052 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8055 reload_outsi+2/5
	ldw -296(%r30),%r1 ;# 2433 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8053 subsi3/1
	ldo 1(%r19),%r19 ;# 8054 addsi3/2
	comb,>>= %r20,%r19,L$0259 ;# 8056 bleu+1
	stw %r1,-268(%r30) ;# 8789 reload_outsi+2/6
	ldil L'65536,%r3 ;# 8674 reload_outsi+2/3
L$0260
	comb,= %r3,%r20,L$0944 ;# 1962 bleu+1
	zdep %r20,30,31,%r19 ;# 1972 ashlsi3+1
	comb,>>= %r3,%r19,L$0265 ;# 1980 bleu+1
	stw %r19,4(%r5) ;# 1974 reload_outsi+2/6
	stw %r3,4(%r5) ;# 1983 reload_outsi+2/6
L$0265
	ldw 0(%r5),%r26 ;# 1990 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 1994 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 1992 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 2002 bleu+1
	stw %r28,0(%r5) ;# 1998 reload_outsi+2/6
	comb,= %r28,%r4,L$0258 ;# 2012 bleu+1
	sub %r6,%r4,%r19 ;# 2014 subsi3/1
	addl %r28,%r19,%r6 ;# 2017 addsi3/1
	sub %r12,%r4,%r19 ;# 2018 subsi3/1
	comib,= 0,%r10,L$0268 ;# 2023 bleu+1
	addl %r28,%r19,%r12 ;# 2021 addsi3/1
	sub %r10,%r4,%r19 ;# 2024 subsi3/1
	addl %r28,%r19,%r10 ;# 2027 addsi3/1
L$0268
	comib,= 0,%r8,L$0269 ;# 2030 bleu+1
	sub %r8,%r4,%r19 ;# 2031 subsi3/1
	addl %r28,%r19,%r8 ;# 2034 addsi3/1
L$0269
	comib,= 0,%r9,L$0258 ;# 2037 bleu+1
	sub %r9,%r4,%r19 ;# 2038 subsi3/1
	addl %r28,%r19,%r9 ;# 2041 addsi3/1
L$0258
	ldw 0(%r5),%r4 ;# 1942 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 1946 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 1943 subsi3/1
	ldo 1(%r19),%r19 ;# 1944 addsi3/2
	comb,<< %r20,%r19,L$0260
	nop ;# 1948 bleu+1
L$0259
	ldi 32,%r19 ;# 2063 movqi+1/2
	stbs,ma %r19,1(%r6) ;# 2064 movqi+1/6
	copy %r6,%r26 ;# 2077 reload_outsi+2/1
	ldi 0,%r25 ;# 2079 reload_outsi+2/2
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl memset,%r2 ;# 2083 call_value_internal_symref
	ldi 32,%r24 ;# 2081 reload_outsi+2/2
	ldb -2(%r6),%r19 ;# 2087 zero_extendqisi2/2
	comib,<> 4,%r19,L$0274 ;# 2089 bleu+1
	ldi 93,%r17 ;# 8622 reload_outsi+2/2
	bb,>=,n %r15,23,L$0274 ;# 2094 movsi-4
	ldb 1(%r6),%r19 ;# 2097 movqi+1/5
	depi -1,29,1,%r19 ;# 2099 iorsi3+1/2
	stb %r19,1(%r6) ;# 2101 movqi+1/6
L$0274
	ldi 4,%r18 ;# 8628 reload_outsi+2/2
	and %r15,%r18,%r1 ;# 8629 andsi3/1
	stw %r1,-252(%r30) ;# 8792 reload_outsi+2/6
	ldo -288(%r30),%r11 ;# 8632 addsi3/2
L$0275
	ldw -296(%r30),%r20 ;# 2436 reload_outsi+2/5
L$1165
	comb,= %r16,%r20,L$0902 ;# 2109 bleu+1
	copy %r20,%r21 ;# 8745 reload_outsi+2/1
	ldbs,ma 1(%r21),%r7 ;# 2134 zero_extendqisi2/2
	comib,= 0,%r14,L$0280 ;# 2139 bleu+1
	stw %r21,-296(%r30) ;# 2445 reload_outsi+2/6
	addl %r14,%r7,%r19 ;# 2140 addsi3/1
	ldb 0(%r19),%r7 ;# 2143 zero_extendqisi2/2
L$0280
	bb,>= %r15,31,L$0285 ;# 2159 movsi-4
	ldi 92,%r19 ;# 2161 reload_outsi+2/2
	comb,<>,n %r19,%r7,L$0285 ;# 2163 bleu+1
	comb,= %r16,%r21,L$0903 ;# 2167 bleu+1
	ldo 1(%r21),%r19 ;# 2195 addsi3/2
	ldb 1(%r20),%r3 ;# 2192 zero_extendqisi2/2
	comib,= 0,%r14,L$0288 ;# 2197 bleu+1
	stw %r19,-296(%r30) ;# 2460 reload_outsi+2/6
	addl %r14,%r3,%r19 ;# 2198 addsi3/1
	ldb 0(%r19),%r3 ;# 2201 zero_extendqisi2/2
L$0288
	extru %r3,28,29,%r19 ;# 2216 lshrsi3/2
	addl %r6,%r19,%r19 ;# 2219 addsi3/1
	bl L$0948,%r0 ;# 2235 jump
	extru %r3,31,3,%r20 ;# 2222 andsi3/1
L$0285
	comb,<>,n %r17,%r7,L$0293 ;# 2243 bleu+1
	ldw -268(%r30),%r1 ;# 8798 reload_outsi+2/5
	ldw -296(%r30),%r20 ;# 2466 reload_outsi+2/5
	ldo 1(%r1),%r19 ;# 2244 addsi3/2
	comb,<>,n %r19,%r20,L$0276 ;# 2246 bleu+1
L$0293
	comib,= 0,%r13,L$0294 ;# 2253 bleu+1
	ldi 45,%r1 ;# 8801 reload_outsi+2/2
	comb,<> %r1,%r7,L$1163 ;# 2257 bleu+1
	ldw -296(%r30),%r20 ;# 2524 reload_outsi+2/5
	ldw -296(%r30),%r19 ;# 2469 reload_outsi+2/5
	ldb 0(%r19),%r19 ;# 2259 movqi+1/5
	extrs %r19,31,8,%r19 ;# 2260 extendqisi2
	comb,<>,n %r17,%r19,L$0895 ;# 2264 bleu+1
L$0294
	ldi 45,%r1 ;# 8804 reload_outsi+2/2
	comb,<> %r1,%r7,L$1163 ;# 2280 bleu+1
	ldw -296(%r30),%r20 ;# 2524 reload_outsi+2/5
	ldw -276(%r30),%r1 ;# 8807 reload_outsi+2/5
	ldo -2(%r20),%r19 ;# 2281 addsi3/2
	comb,>>,n %r1,%r19,L$1179 ;# 2283 bleu+1
	ldb -2(%r20),%r19 ;# 2285 movqi+1/5
	ldi 91,%r1 ;# 8810 reload_outsi+2/2
	extrs %r19,31,8,%r19 ;# 2286 extendqisi2
	comb,= %r1,%r19,L$1163 ;# 2290 bleu+1
	ldw -276(%r30),%r1 ;# 8813 reload_outsi+2/5
L$1179
	ldo -3(%r20),%r19 ;# 2294 addsi3/2
	comb,>>,n %r1,%r19,L$0297 ;# 2296 bleu+1
	ldb -3(%r20),%r19 ;# 2298 movqi+1/5
	ldi 91,%r1 ;# 8816 reload_outsi+2/2
	extrs %r19,31,8,%r19 ;# 2299 extendqisi2
	comb,<> %r1,%r19,L$1164 ;# 2303 bleu+1
	ldw -296(%r30),%r19 ;# 2487 reload_outsi+2/5
	ldb -2(%r20),%r19 ;# 2305 movqi+1/5
	ldi 94,%r20 ;# 2308 reload_outsi+2/2
	extrs %r19,31,8,%r19 ;# 2306 extendqisi2
	comb,= %r20,%r19,L$1163 ;# 2310 bleu+1
	ldw -296(%r30),%r20 ;# 2524 reload_outsi+2/5
L$0297
	ldw -296(%r30),%r19 ;# 2487 reload_outsi+2/5
L$1164
	ldb 0(%r19),%r19 ;# 2315 movqi+1/5
	extrs %r19,31,8,%r19 ;# 2316 extendqisi2
	comb,<> %r17,%r19,L$0302 ;# 2320 bleu+1
	ldw -296(%r30),%r20 ;# 2524 reload_outsi+2/5
L$1163
	ldb 0(%r20),%r19 ;# 2526 movqi+1/5
	ldi 45,%r1 ;# 8819 reload_outsi+2/2
	extrs %r19,31,8,%r19 ;# 2527 extendqisi2
	comb,<>,n %r1,%r19,L$0300 ;# 2531 bleu+1
	ldb 1(%r20),%r19 ;# 2535 movqi+1/5
	extrs %r19,31,8,%r19 ;# 2536 extendqisi2
	comb,=,n %r17,%r19,L$0300 ;# 2540 bleu+1
	comb,= %r16,%r20,L$0922 ;# 2550 bleu+1
	ldo 1(%r20),%r19 ;# 2559 addsi3/2
	stw %r19,-296(%r30) ;# 2561 reload_outsi+2/6
L$0302
	stw %r6,-52(%r30) ;# 2588 reload_outsi+2/6
	ldo -296(%r30),%r26 ;# 2590 addsi3/2
	copy %r16,%r25 ;# 2592 reload_outsi+2/1
	copy %r14,%r24 ;# 2594 reload_outsi+2/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl compile_range,%r2 ;# 2598 call_value_internal_symref
	copy %r15,%r23 ;# 2596 reload_outsi+2/1
	movb,= %r28,%r4,L$1165 ;# 2603 decrement_and_branch_until_zero+2/1
	ldw -296(%r30),%r20 ;# 2436 reload_outsi+2/5
	.CALL ARGW0=GR
	bl free,%r2 ;# 2607 call_internal_symref
	ldw -312(%r30),%r26 ;# 2605 reload_outsi+2/5
	bl L$0867,%r0 ;# 2611 jump
	copy %r4,%r28 ;# 2609 reload_outsi+2/1
L$0300
	ldw -252(%r30),%r1 ;# 8822 reload_outsi+2/5
	comib,= 0,%r1,L$0309 ;# 2624 bleu+1
	ldi 91,%r1 ;# 8825 reload_outsi+2/2
	comb,<> %r1,%r7,L$1166 ;# 2628 bleu+1
	ldi 0,%r13 ;# 3624 reload_outsi+2/2
	ldw -296(%r30),%r20 ;# 2630 reload_outsi+2/5
	ldb 0(%r20),%r19 ;# 2632 movqi+1/5
	ldi 58,%r1 ;# 8828 reload_outsi+2/2
	extrs %r19,31,8,%r19 ;# 2633 extendqisi2
	comb,<>,n %r1,%r19,L$1166 ;# 2637 bleu+1
	comb,= %r16,%r20,L$0922 ;# 2647 bleu+1
	ldo 1(%r20),%r19 ;# 2656 addsi3/2
	stw %r19,-296(%r30) ;# 2658 reload_outsi+2/6
	comb,= %r16,%r19,L$0902 ;# 2689 bleu+1
	ldi 0,%r3 ;# 2684 reload_outsi+2/2
L$0317
	ldw -296(%r30),%r19 ;# 2709 reload_outsi+2/5
	comb,= %r16,%r19,L$0922 ;# 2711 bleu+1
	ldo 1(%r19),%r20 ;# 2720 addsi3/2
	stw %r20,-296(%r30) ;# 2722 reload_outsi+2/6
	comib,= 0,%r14,L$0321 ;# 2729 bleu+1
	ldb 0(%r19),%r7 ;# 2725 zero_extendqisi2/2
	addl %r14,%r7,%r19 ;# 2730 addsi3/1
	ldb 0(%r19),%r7 ;# 2733 zero_extendqisi2/2
L$0321
	ldi 58,%r1 ;# 8831 reload_outsi+2/2
	comb,= %r1,%r7,L$1167 ;# 2750 bleu+1
	addl %r11,%r3,%r19 ;# 2789 addsi3/1
	comb,=,n %r17,%r7,L$1167 ;# 2754 bleu+1
	comb,=,n %r16,%r20,L$1167 ;# 2758 bleu+1
	comib,= 6,%r3,L$1167 ;# 2760 bleu+1
	copy %r3,%r20 ;# 2770 reload_outsi+2/1
	ldo 1(%r20),%r19 ;# 2771 addsi3/2
	extru %r19,31,8,%r3 ;# 2772 zero_extendqisi2/1
	addl %r11,%r20,%r20 ;# 2776 addsi3/1
	bl L$0317,%r0 ;# 2783 jump
	stb %r7,0(%r20) ;# 2778 movqi+1/6
L$1167
	comb,<> %r1,%r7,L$0328 ;# 2796 bleu+1
	stb %r0,0(%r19) ;# 2791 movqi+1/6
	ldw -296(%r30),%r19 ;# 2798 reload_outsi+2/5
	ldb 0(%r19),%r19 ;# 2800 movqi+1/5
	extrs %r19,31,8,%r19 ;# 2801 extendqisi2
	comb,<> %r17,%r19,L$1168 ;# 2805 bleu+1
	ldi 255,%r19 ;# 8069 reload_outsi+2/2
	copy %r11,%r26 ;# 2813 reload_outsi+2/1
	ldil LR'L$C0016,%r1 ;# 8835 add_high_const+3
	ldo RR'L$C0016(%r1),%r1 ;# 8836 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2817 call_value_internal_symref
	copy %r1,%r25 ;# 2815 reload_outsi+2/1
	copy %r11,%r26 ;# 2829 reload_outsi+2/1
	ldil LR'L$C0017,%r1 ;# 8837 add_high_const+3
	ldo RR'L$C0017(%r1),%r1 ;# 8838 movhi-2
	copy %r1,%r25 ;# 2831 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2821 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2833 call_value_internal_symref
	stw %r28,-244(%r30) ;# 8841 reload_outsi+2/6
	copy %r11,%r26 ;# 2845 reload_outsi+2/1
	ldil LR'L$C0018,%r1 ;# 8842 add_high_const+3
	ldo RR'L$C0018(%r1),%r1 ;# 8843 movhi-2
	copy %r1,%r25 ;# 2847 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2837 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2849 call_value_internal_symref
	stw %r28,-236(%r30) ;# 8846 reload_outsi+2/6
	copy %r11,%r26 ;# 2861 reload_outsi+2/1
	ldil LR'L$C0019,%r1 ;# 8847 add_high_const+3
	ldo RR'L$C0019(%r1),%r1 ;# 8848 movhi-2
	copy %r1,%r25 ;# 2863 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2853 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2865 call_value_internal_symref
	stw %r28,-228(%r30) ;# 8851 reload_outsi+2/6
	copy %r11,%r26 ;# 2877 reload_outsi+2/1
	ldil LR'L$C0020,%r1 ;# 8852 add_high_const+3
	ldo RR'L$C0020(%r1),%r1 ;# 8853 movhi-2
	copy %r1,%r25 ;# 2879 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2869 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2881 call_value_internal_symref
	stw %r28,-220(%r30) ;# 8856 reload_outsi+2/6
	copy %r11,%r26 ;# 2893 reload_outsi+2/1
	ldil LR'L$C0021,%r1 ;# 8857 add_high_const+3
	ldo RR'L$C0021(%r1),%r1 ;# 8858 movhi-2
	copy %r1,%r25 ;# 2895 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2885 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2897 call_value_internal_symref
	stw %r28,-212(%r30) ;# 8861 reload_outsi+2/6
	copy %r11,%r26 ;# 2909 reload_outsi+2/1
	ldil LR'L$C0022,%r1 ;# 8862 add_high_const+3
	ldo RR'L$C0022(%r1),%r1 ;# 8863 movhi-2
	copy %r1,%r25 ;# 2911 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2901 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2913 call_value_internal_symref
	stw %r28,-204(%r30) ;# 8866 reload_outsi+2/6
	copy %r11,%r26 ;# 2925 reload_outsi+2/1
	ldil LR'L$C0023,%r1 ;# 8867 add_high_const+3
	ldo RR'L$C0023(%r1),%r1 ;# 8868 movhi-2
	copy %r1,%r25 ;# 2927 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2917 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2929 call_value_internal_symref
	stw %r28,-196(%r30) ;# 8871 reload_outsi+2/6
	copy %r11,%r26 ;# 2941 reload_outsi+2/1
	ldil LR'L$C0024,%r1 ;# 8872 add_high_const+3
	ldo RR'L$C0024(%r1),%r1 ;# 8873 movhi-2
	copy %r1,%r25 ;# 2943 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2933 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2945 call_value_internal_symref
	stw %r28,-188(%r30) ;# 8876 reload_outsi+2/6
	copy %r11,%r26 ;# 2957 reload_outsi+2/1
	ldil LR'L$C0025,%r1 ;# 8877 add_high_const+3
	ldo RR'L$C0025(%r1),%r1 ;# 8878 movhi-2
	copy %r1,%r25 ;# 2959 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2949 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2961 call_value_internal_symref
	stw %r28,-180(%r30) ;# 8881 reload_outsi+2/6
	copy %r11,%r26 ;# 2973 reload_outsi+2/1
	ldil LR'L$C0026,%r19 ;# 2970 add_high_const+3
	ldo RR'L$C0026(%r19),%r3 ;# 2971 movhi-2
	copy %r3,%r25 ;# 2975 reload_outsi+2/1
	comiclr,<> 0,%r28,%r28 ;# 2965 scc
	ldi 1,%r28
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2977 call_value_internal_symref
	stw %r28,-172(%r30) ;# 8884 reload_outsi+2/6
	copy %r11,%r26 ;# 2989 reload_outsi+2/1
	ldil LR'L$C0027,%r19 ;# 2986 add_high_const+3
	ldo RR'L$C0027(%r19),%r4 ;# 2987 movhi-2
	comiclr,<> 0,%r28,%r13 ;# 2981 scc
	ldi 1,%r13
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 2993 call_value_internal_symref
	copy %r4,%r25 ;# 2991 reload_outsi+2/1
	copy %r11,%r26 ;# 3005 reload_outsi+2/1
	ldil LR'L$C0017,%r1 ;# 8885 add_high_const+3
	ldo RR'L$C0017(%r1),%r1 ;# 8886 movhi-2
	comiclr,<> 0,%r28,%r7 ;# 2997 scc
	ldi 1,%r7
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3009 call_value_internal_symref
	copy %r1,%r25 ;# 3007 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3013 bleu+1
	copy %r11,%r26 ;# 3018 reload_outsi+2/1
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3022 call_value_internal_symref
	copy %r3,%r25 ;# 3020 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3026 bleu+1
	copy %r11,%r26 ;# 3031 reload_outsi+2/1
	ldil LR'L$C0022,%r1 ;# 8887 add_high_const+3
	ldo RR'L$C0022(%r1),%r1 ;# 8888 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3035 call_value_internal_symref
	copy %r1,%r25 ;# 3033 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3039 bleu+1
	copy %r11,%r26 ;# 3044 reload_outsi+2/1
	ldil LR'L$C0020,%r1 ;# 8889 add_high_const+3
	ldo RR'L$C0020(%r1),%r1 ;# 8890 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3048 call_value_internal_symref
	copy %r1,%r25 ;# 3046 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3052 bleu+1
	copy %r11,%r26 ;# 3057 reload_outsi+2/1
	ldil LR'L$C0016,%r1 ;# 8891 add_high_const+3
	ldo RR'L$C0016(%r1),%r1 ;# 8892 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3061 call_value_internal_symref
	copy %r1,%r25 ;# 3059 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3065 bleu+1
	copy %r11,%r26 ;# 3070 reload_outsi+2/1
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3074 call_value_internal_symref
	copy %r4,%r25 ;# 3072 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3078 bleu+1
	copy %r11,%r26 ;# 3083 reload_outsi+2/1
	ldil LR'L$C0025,%r1 ;# 8893 add_high_const+3
	ldo RR'L$C0025(%r1),%r1 ;# 8894 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3087 call_value_internal_symref
	copy %r1,%r25 ;# 3085 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3091 bleu+1
	copy %r11,%r26 ;# 3096 reload_outsi+2/1
	ldil LR'L$C0023,%r1 ;# 8895 add_high_const+3
	ldo RR'L$C0023(%r1),%r1 ;# 8896 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3100 call_value_internal_symref
	copy %r1,%r25 ;# 3098 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3104 bleu+1
	copy %r11,%r26 ;# 3109 reload_outsi+2/1
	ldil LR'L$C0024,%r1 ;# 8897 add_high_const+3
	ldo RR'L$C0024(%r1),%r1 ;# 8898 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3113 call_value_internal_symref
	copy %r1,%r25 ;# 3111 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3117 bleu+1
	copy %r11,%r26 ;# 3122 reload_outsi+2/1
	ldil LR'L$C0021,%r1 ;# 8899 add_high_const+3
	ldo RR'L$C0021(%r1),%r1 ;# 8900 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3126 call_value_internal_symref
	copy %r1,%r25 ;# 3124 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3130 bleu+1
	copy %r11,%r26 ;# 3135 reload_outsi+2/1
	ldil LR'L$C0019,%r1 ;# 8901 add_high_const+3
	ldo RR'L$C0019(%r1),%r1 ;# 8902 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3139 call_value_internal_symref
	copy %r1,%r25 ;# 3137 reload_outsi+2/1
	comib,= 0,%r28,L$0329 ;# 3143 bleu+1
	copy %r11,%r26 ;# 3148 reload_outsi+2/1
	ldil LR'L$C0018,%r1 ;# 8903 add_high_const+3
	ldo RR'L$C0018(%r1),%r1 ;# 8904 movhi-2
	.CALL ARGW0=GR,ARGW1=GR
	bl strcmp,%r2 ;# 3152 call_value_internal_symref
	copy %r1,%r25 ;# 3150 reload_outsi+2/1
	comib,<>,n 0,%r28,L$0900 ;# 3156 bleu+1
L$0329
	ldw -296(%r30),%r19 ;# 3173 reload_outsi+2/5
	comb,= %r16,%r19,L$0922 ;# 3175 bleu+1
	ldo 1(%r19),%r19 ;# 3184 addsi3/2
	comb,= %r16,%r19,L$0902 ;# 3214 bleu+1
	stw %r19,-296(%r30) ;# 3186 reload_outsi+2/6
	ldi 0,%r22 ;# 3227 reload_outsi+2/2
	addil LR'__alnum-$global$,%r27 ;# 8596 pic2_lo_sum+1
	copy %r1,%r2 ;# 8907 reload_outsi+2/1
	addil LR'__ctype2-$global$,%r27 ;# 8598 pic2_lo_sum+1
	copy %r1,%r23 ;# 8910 reload_outsi+2/1
	addil LR'__ctype-$global$,%r27 ;# 8600 pic2_lo_sum+1
	copy %r1,%r4 ;# 8913 reload_outsi+2/1
	ldi 32,%r25 ;# 8605 reload_outsi+2/2
	ldi 2,%r24 ;# 8607 reload_outsi+2/2
	ldi 16,%r31 ;# 8609 reload_outsi+2/2
	ldi 8,%r29 ;# 8611 reload_outsi+2/2
	ldi 128,%r28 ;# 8613 reload_outsi+2/2
	ldi 255,%r26 ;# 8615 reload_outsi+2/2
	ldw -244(%r30),%r1 ;# 8916 reload_outsi+2/5
L$1173
	comib,=,n 0,%r1,L$0343 ;# 3240 bleu+1
	stw %r22,RR'__alnum-$global$(%r2) ;# 3244 reload_outsi+2/6
	ldw RR'__ctype2-$global$(%r23),%r19 ;# 3248 reload_outsi+2/5
	ldw RR'__ctype-$global$(%r4),%r21 ;# 3260 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3253 addsi3/1
	addl %r21,%r22,%r21 ;# 3265 addsi3/1
	ldb 0(%r19),%r20 ;# 3255 movqi+1/5
	ldb 0(%r21),%r19 ;# 3267 movqi+1/5
	extru %r20,31,1,%r20 ;# 3256 andsi3/1
	and %r19,%r18,%r19 ;# 3270 andsi3/1
	or %r20,%r19,%r20 ;# 3278 xordi3-1
	comib,<> 0,%r20,L$1169 ;# 3280 bleu+1
	extru %r22,31,8,%r19 ;# 3330 zero_extendqisi2/1
L$0343
	ldw -236(%r30),%r1 ;# 8919 reload_outsi+2/5
	comib,= 0,%r1,L$0344 ;# 3285 bleu+1
	ldw RR'__ctype2-$global$(%r23),%r19 ;# 3289 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3290 addsi3/1
	ldb 0(%r19),%r19 ;# 3292 movqi+1/5
	bb,< %r19,31,L$1169 ;# 3296 bleu+3
	extru %r22,31,8,%r19 ;# 3330 zero_extendqisi2/1
L$0344
	ldw -228(%r30),%r1 ;# 8922 reload_outsi+2/5
	comib,=,n 0,%r1,L$0345 ;# 3301 bleu+1
	comb,= %r25,%r22,L$1169 ;# 3305 bleu+1
	extru %r22,31,8,%r19 ;# 3330 zero_extendqisi2/1
	comib,= 9,%r22,L$1170 ;# 3307 bleu+1
	extru %r19,28,29,%r21 ;# 3332 lshrsi3/2
L$0345
	ldw -220(%r30),%r1 ;# 8925 reload_outsi+2/5
	comib,= 0,%r1,L$0341 ;# 3312 bleu+1
	ldw RR'__ctype-$global$(%r4),%r19 ;# 3316 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3317 addsi3/1
	ldb 0(%r19),%r19 ;# 3319 movqi+1/5
	and %r19,%r25,%r19 ;# 3322 andsi3/1
	comib,= 0,%r19,L$0341 ;# 3325 bleu+1
	extru %r22,31,8,%r19 ;# 3330 zero_extendqisi2/1
L$1169
	extru %r19,28,29,%r21 ;# 3332 lshrsi3/2
L$1170
	addl %r6,%r21,%r21 ;# 3335 addsi3/1
	extru %r19,31,3,%r19 ;# 3339 andsi3/1
	subi 31,%r19,%r19 ;# 3340 subsi3/2
	ldb 0(%r21),%r20 ;# 3343 movqi+1/5
	mtsar %r19 ;# 8928 reload_outsi+2/7
	vdepi -1,1,%r20 ;# 3348 vdepi_ior
	stb %r20,0(%r21) ;# 3350 movqi+1/6
L$0341
	ldw -212(%r30),%r1 ;# 8931 reload_outsi+2/5
	comib,= 0,%r1,L$0348 ;# 3354 bleu+1
	ldw RR'__ctype-$global$(%r4),%r19 ;# 3358 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3359 addsi3/1
	ldb 0(%r19),%r19 ;# 3361 movqi+1/5
	and %r19,%r18,%r19 ;# 3364 andsi3/1
	comib,<> 0,%r19,L$1171 ;# 3367 bleu+1
	extru %r22,31,8,%r19 ;# 3426 zero_extendqisi2/1
L$0348
	ldw -204(%r30),%r1 ;# 8934 reload_outsi+2/5
	comib,= 0,%r1,L$0349 ;# 3372 bleu+1
	ldw RR'__ctype2-$global$(%r23),%r19 ;# 3376 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3377 addsi3/1
	ldb 0(%r19),%r19 ;# 3379 movqi+1/5
	and %r19,%r24,%r19 ;# 3382 andsi3/1
	comib,<> 0,%r19,L$1171 ;# 3385 bleu+1
	extru %r22,31,8,%r19 ;# 3426 zero_extendqisi2/1
L$0349
	ldw -196(%r30),%r1 ;# 8937 reload_outsi+2/5
	comib,= 0,%r1,L$0350 ;# 3390 bleu+1
	ldw RR'__ctype-$global$(%r4),%r19 ;# 3394 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3395 addsi3/1
	ldb 0(%r19),%r19 ;# 3397 movqi+1/5
	and %r19,%r24,%r19 ;# 3400 andsi3/1
	comib,<> 0,%r19,L$1171 ;# 3403 bleu+1
	extru %r22,31,8,%r19 ;# 3426 zero_extendqisi2/1
L$0350
	ldw -188(%r30),%r1 ;# 8940 reload_outsi+2/5
	comib,= 0,%r1,L$0346 ;# 3408 bleu+1
	ldw RR'__ctype2-$global$(%r23),%r19 ;# 3412 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3413 addsi3/1
	ldb 0(%r19),%r19 ;# 3415 movqi+1/5
	and %r19,%r18,%r19 ;# 3418 andsi3/1
	comib,= 0,%r19,L$0346 ;# 3421 bleu+1
	extru %r22,31,8,%r19 ;# 3426 zero_extendqisi2/1
L$1171
	extru %r19,28,29,%r21 ;# 3428 lshrsi3/2
	addl %r6,%r21,%r21 ;# 3431 addsi3/1
	extru %r19,31,3,%r19 ;# 3435 andsi3/1
	subi 31,%r19,%r19 ;# 3436 subsi3/2
	ldb 0(%r21),%r20 ;# 3439 movqi+1/5
	mtsar %r19 ;# 8943 reload_outsi+2/7
	vdepi -1,1,%r20 ;# 3444 vdepi_ior
	stb %r20,0(%r21) ;# 3446 movqi+1/6
L$0346
	ldw -180(%r30),%r1 ;# 8946 reload_outsi+2/5
	comib,= 0,%r1,L$0353 ;# 3450 bleu+1
	ldw RR'__ctype-$global$(%r4),%r19 ;# 3454 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3455 addsi3/1
	ldb 0(%r19),%r19 ;# 3457 movqi+1/5
	and %r19,%r31,%r19 ;# 3460 andsi3/1
	comib,<> 0,%r19,L$1172 ;# 3463 bleu+1
	extru %r22,31,8,%r19 ;# 3520 zero_extendqisi2/1
L$0353
	ldw -172(%r30),%r1 ;# 8949 reload_outsi+2/5
	comib,= 0,%r1,L$0354 ;# 3468 bleu+1
	ldw RR'__ctype-$global$(%r4),%r19 ;# 3472 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3473 addsi3/1
	ldb 0(%r19),%r19 ;# 3475 movqi+1/5
	and %r19,%r29,%r19 ;# 3478 andsi3/1
	comib,<> 0,%r19,L$1172 ;# 3481 bleu+1
	extru %r22,31,8,%r19 ;# 3520 zero_extendqisi2/1
L$0354
	comib,= 0,%r13,L$0355 ;# 3486 bleu+1
	ldw RR'__ctype-$global$(%r4),%r19 ;# 3490 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3491 addsi3/1
	ldb 0(%r19),%r19 ;# 3493 movqi+1/5
	bb,< %r19,31,L$1172 ;# 3497 bleu+3
	extru %r22,31,8,%r19 ;# 3520 zero_extendqisi2/1
L$0355
	comib,= 0,%r7,L$0339 ;# 3502 bleu+1
	ldw RR'__ctype-$global$(%r4),%r19 ;# 3506 reload_outsi+2/5
	addl %r19,%r22,%r19 ;# 3507 addsi3/1
	ldb 0(%r19),%r19 ;# 3509 movqi+1/5
	and %r19,%r28,%r19 ;# 3512 andsi3/1
	comib,= 0,%r19,L$0339 ;# 3515 bleu+1
	extru %r22,31,8,%r19 ;# 3520 zero_extendqisi2/1
L$1172
	extru %r19,28,29,%r21 ;# 3522 lshrsi3/2
	addl %r6,%r21,%r21 ;# 3525 addsi3/1
	extru %r19,31,3,%r19 ;# 3529 andsi3/1
	subi 31,%r19,%r19 ;# 3530 subsi3/2
	ldb 0(%r21),%r20 ;# 3533 movqi+1/5
	mtsar %r19 ;# 8952 reload_outsi+2/7
	vdepi -1,1,%r20 ;# 3538 vdepi_ior
	stb %r20,0(%r21) ;# 3540 movqi+1/6
L$0339
	ldo 1(%r22),%r22 ;# 3546 addsi3/2
	comb,>=,n %r26,%r22,L$1173 ;# 3233 bleu+1
	ldw -244(%r30),%r1 ;# 8916 reload_outsi+2/5
	bl L$0275,%r0 ;# 3559 jump
	ldi 1,%r13 ;# 3556 reload_outsi+2/2
L$0328
	ldi 255,%r19 ;# 8069 reload_outsi+2/2
L$1168
	comb,= %r19,%r3,L$0359 ;# 8070 bleu+1
	copy %r19,%r21 ;# 8595 reload_outsi+2/1
L$0360
	ldo -1(%r3),%r20 ;# 3571 addsi3/2
	ldw -296(%r30),%r19 ;# 3583 reload_outsi+2/5
	extru %r20,31,8,%r3 ;# 3572 zero_extendqisi2/1
	ldo -1(%r19),%r19 ;# 3584 addsi3/2
	comb,<> %r21,%r3,L$0360 ;# 3577 bleu+1
	stw %r19,-296(%r30) ;# 3588 reload_outsi+2/6
L$0359
	ldb 11(%r6),%r19 ;# 3599 movqi+1/5
	depi -1,28,1,%r19 ;# 3601 iorsi3+1/2
	stb %r19,11(%r6) ;# 3603 movqi+1/6
	ldb 7(%r6),%r19 ;# 3606 movqi+1/5
	ldi 0,%r13 ;# 3613 reload_outsi+2/2
	depi -1,29,1,%r19 ;# 3608 iorsi3+1/2
	bl L$0275,%r0 ;# 3618 jump
	stb %r19,7(%r6) ;# 3610 movqi+1/6
L$0309
	ldi 0,%r13 ;# 3624 reload_outsi+2/2
L$1166
	extru %r7,21+8-1,8,%r19 ;# 3627 extzv
	addl %r6,%r19,%r19 ;# 3630 addsi3/1
	extru %r7,31,3,%r20 ;# 3633 andsi3/1
L$0948
	subi 31,%r20,%r20 ;# 3634 subsi3/2
	ldb 0(%r19),%r21 ;# 3637 movqi+1/5
	mtsar %r20 ;# 8955 reload_outsi+2/7
	vdepi -1,1,%r21 ;# 3642 vdepi_ior
	bl L$0275,%r0 ;# 3653 jump
	stb %r21,0(%r19) ;# 3644 movqi+1/6
L$0276
	ldb -1(%r6),%r20 ;# 8074 movqi+1/5
	extru %r20,31,8,%r19 ;# 8075 zero_extendqisi2/1
	comib,= 0,%r19,L$0364 ;# 8076 bleu+1
	addl %r19,%r6,%r19 ;# 8079 addsi3/1
	ldb -1(%r19),%r19 ;# 8082 zero_extendqisi2/2
	comib,<> 0,%r19,L$0364 ;# 8083 bleu+1
	ldo -1(%r20),%r19 ;# 8242 addsi3/2
	bl L$1183,%r0 ;# 8253 jump
	stb %r19,-1(%r6) ;# 3688 movqi+1/6
L$0365
	ldo -1(%r19),%r19 ;# 3686 addsi3/2
	stb %r19,-1(%r6) ;# 3688 movqi+1/6
L$1183
	extru %r19,31,8,%r19 ;# 3662 zero_extendqisi2/1
	comib,= 0,%r19,L$0364 ;# 3664 bleu+1
	addl %r19,%r6,%r19 ;# 3668 addsi3/1
	ldb -1(%r19),%r19 ;# 3672 zero_extendqisi2/2
	comib,=,n 0,%r19,L$0365 ;# 3674 bleu+1
	ldb -1(%r6),%r19 ;# 3683 movqi+1/5
L$0364
	ldb -1(%r6),%r19 ;# 3700 zero_extendqisi2/2
	bl L$0043,%r0 ;# 3705 jump
	addl %r6,%r19,%r6 ;# 3701 addsi3/1
L$0368
	bb,>=,n %r15,18,L$0076 ;# 3713 movsi-4
	bl L$1181,%r0 ;# 3721 jump
	ldw 24(%r5),%r19 ;# 3861 reload_outsi+2/5
L$0372
	bb,>=,n %r15,18,L$0076 ;# 3730 movsi-4
	bl,n L$0374,%r0 ;# 3738 jump
L$0376
	bb,>=,n %r15,20,L$0076 ;# 3747 movsi-4
	bl,n L$0378,%r0 ;# 3755 jump
L$0380
	bb,>=,n %r15,16,L$0076 ;# 3764 movsi-4
	bl,n L$0378,%r0 ;# 3772 jump
L$0383
	and %r15,%r20,%r19 ;# 3779 andsi3/1
	comb,= %r20,%r19,L$1174 ;# 3783 bleu+1
	ldi -1,%r4 ;# 5074 reload_outsi+2/2
	bl,n L$0076,%r0 ;# 3791 jump
L$0387
	comb,=,n %r16,%r19,L$0903 ;# 3799 bleu+1
	ldb 0(%r19),%r7 ;# 3831 zero_extendqisi2/2
	ldo 1(%r19),%r19 ;# 3826 addsi3/2
	stw %r19,-296(%r30) ;# 3828 reload_outsi+2/6
	ldo -39(%r7),%r19 ;# 7446 addsi3/2
	addi,uv -86,%r19,%r0 ;# 7447 casesi0
	blr,n %r19,%r0
	b,n L$0397
L$0817
	bl L$0759,%r0 ;# 9437 switch_jump
	ldw 0(%r5),%r4 ;# 8204 reload_outsi+2/5
L$1069
	bl L$0395,%r0
	nop ;# 9440 switch_jump
L$1070
	bl L$0422,%r0
	nop ;# 9443 switch_jump
L$1071
	bl L$0397,%r0
	nop ;# 9446 switch_jump
L$1072
	bl L$0811,%r0
	nop ;# 9449 switch_jump
L$1073
	bl L$0397,%r0
	nop ;# 9452 switch_jump
L$1074
	bl L$0397,%r0
	nop ;# 9455 switch_jump
L$1075
	bl L$0397,%r0
	nop ;# 9458 switch_jump
L$1076
	bl L$0397,%r0
	nop ;# 9461 switch_jump
L$1077
	bl L$0397,%r0
	nop ;# 9464 switch_jump
L$1078
	bl L$0787,%r0
	nop ;# 9467 switch_jump
L$1079
	bl L$0787,%r0
	nop ;# 9470 switch_jump
L$1080
	bl L$0787,%r0
	nop ;# 9473 switch_jump
L$1081
	bl L$0787,%r0
	nop ;# 9476 switch_jump
L$1082
	bl L$0787,%r0
	nop ;# 9479 switch_jump
L$1083
	bl L$0787,%r0
	nop ;# 9482 switch_jump
L$1084
	bl L$0787,%r0
	nop ;# 9485 switch_jump
L$1085
	bl L$0787,%r0
	nop ;# 9488 switch_jump
L$1086
	bl L$0787,%r0
	nop ;# 9491 switch_jump
L$1087
	bl L$0397,%r0
	nop ;# 9494 switch_jump
L$1088
	bl L$0397,%r0
	nop ;# 9497 switch_jump
L$1089
	bl L$0659,%r0 ;# 9500 switch_jump
	ldw 0(%r5),%r4 ;# 8164 reload_outsi+2/5
L$1090
	bl L$0397,%r0
	nop ;# 9503 switch_jump
L$1091
	bl L$0679,%r0 ;# 9506 switch_jump
	ldw 0(%r5),%r4 ;# 8172 reload_outsi+2/5
L$1092
	bl L$0811,%r0
	nop ;# 9509 switch_jump
L$1093
	bl L$0397,%r0
	nop ;# 9512 switch_jump
L$1094
	bl L$0397,%r0
	nop ;# 9515 switch_jump
L$1095
	bl L$0719,%r0 ;# 9518 switch_jump
	ldw 0(%r5),%r4 ;# 8188 reload_outsi+2/5
L$1096
	bl L$0397,%r0
	nop ;# 9521 switch_jump
L$1097
	bl L$0397,%r0
	nop ;# 9524 switch_jump
L$1098
	bl L$0397,%r0
	nop ;# 9527 switch_jump
L$1099
	bl L$0397,%r0
	nop ;# 9530 switch_jump
L$1100
	bl L$0397,%r0
	nop ;# 9533 switch_jump
L$1101
	bl L$0397,%r0
	nop ;# 9536 switch_jump
L$1102
	bl L$0397,%r0
	nop ;# 9539 switch_jump
L$1103
	bl L$0397,%r0
	nop ;# 9542 switch_jump
L$1104
	bl L$0397,%r0
	nop ;# 9545 switch_jump
L$1105
	bl L$0397,%r0
	nop ;# 9548 switch_jump
L$1106
	bl L$0397,%r0
	nop ;# 9551 switch_jump
L$1107
	bl L$0397,%r0
	nop ;# 9554 switch_jump
L$1108
	bl L$0397,%r0
	nop ;# 9557 switch_jump
L$1109
	bl L$0397,%r0
	nop ;# 9560 switch_jump
L$1110
	bl L$0397,%r0
	nop ;# 9563 switch_jump
L$1111
	bl L$0397,%r0
	nop ;# 9566 switch_jump
L$1112
	bl L$0397,%r0
	nop ;# 9569 switch_jump
L$1113
	bl L$0397,%r0
	nop ;# 9572 switch_jump
L$1114
	bl L$0397,%r0
	nop ;# 9575 switch_jump
L$1115
	bl L$0397,%r0
	nop ;# 9578 switch_jump
L$1116
	bl L$0639,%r0 ;# 9581 switch_jump
	ldw 0(%r5),%r4 ;# 8156 reload_outsi+2/5
L$1117
	bl L$0397,%r0
	nop ;# 9584 switch_jump
L$1118
	bl L$0397,%r0
	nop ;# 9587 switch_jump
L$1119
	bl L$0397,%r0
	nop ;# 9590 switch_jump
L$1120
	bl L$0397,%r0
	nop ;# 9593 switch_jump
L$1121
	bl L$0397,%r0
	nop ;# 9596 switch_jump
L$1122
	bl L$0397,%r0
	nop ;# 9599 switch_jump
L$1123
	bl L$0397,%r0
	nop ;# 9602 switch_jump
L$1124
	bl L$0397,%r0
	nop ;# 9605 switch_jump
L$1125
	bl L$0739,%r0 ;# 9608 switch_jump
	ldw 0(%r5),%r4 ;# 8196 reload_outsi+2/5
L$1126
	bl L$0397,%r0
	nop ;# 9611 switch_jump
L$1127
	bl L$0699,%r0 ;# 9614 switch_jump
	ldw 0(%r5),%r4 ;# 8180 reload_outsi+2/5
L$1128
	bl L$0397,%r0
	nop ;# 9617 switch_jump
L$1129
	bl L$0397,%r0
	nop ;# 9620 switch_jump
L$1130
	bl L$0397,%r0
	nop ;# 9623 switch_jump
L$1131
	bl L$0397,%r0
	nop ;# 9626 switch_jump
L$1132
	bl L$0397,%r0
	nop ;# 9629 switch_jump
L$1133
	bl L$0397,%r0
	nop ;# 9632 switch_jump
L$1134
	bl L$0397,%r0
	nop ;# 9635 switch_jump
L$1135
	bl L$0397,%r0
	nop ;# 9638 switch_jump
L$1136
	bl L$0397,%r0
	nop ;# 9641 switch_jump
L$1137
	bl L$0397,%r0
	nop ;# 9644 switch_jump
L$1138
	bl L$0397,%r0
	nop ;# 9647 switch_jump
L$1139
	bl L$0397,%r0
	nop ;# 9650 switch_jump
L$1140
	bl L$0397,%r0
	nop ;# 9653 switch_jump
L$1141
	bl L$0397,%r0
	nop ;# 9656 switch_jump
L$1142
	bl L$0397,%r0
	nop ;# 9659 switch_jump
L$1143
	bl L$0397,%r0
	nop ;# 9662 switch_jump
L$1144
	bl L$0397,%r0
	nop ;# 9665 switch_jump
L$1145
	bl L$0397,%r0
	nop ;# 9668 switch_jump
L$1146
	bl L$0397,%r0
	nop ;# 9671 switch_jump
L$1147
	bl L$0397,%r0
	nop ;# 9674 switch_jump
L$1148
	bl L$0619,%r0 ;# 9677 switch_jump
	ldw 0(%r5),%r4 ;# 8148 reload_outsi+2/5
L$1149
	bl L$0397,%r0
	nop ;# 9680 switch_jump
L$1150
	bl L$0397,%r0
	nop ;# 9683 switch_jump
L$1151
	bl L$0397,%r0
	nop ;# 9686 switch_jump
L$1152
	bl L$0506,%r0
	nop ;# 9689 switch_jump
L$1153
	bl L$0472,%r0 ;# 9692 switch_jump
	ldil L'33792,%r19 ;# 4724 add_high_const+3
L$1154
	bl,n L$0397,%r0 ;# 7450 jump
L$0395
	bb,<,n %r15,18,L$0397 ;# 3853 bleu+3
	ldw 24(%r5),%r19 ;# 3861 reload_outsi+2/5
L$1181
	ldo 1(%r19),%r19 ;# 3862 addsi3/2
	stw %r19,24(%r5) ;# 3866 reload_outsi+2/6
	ldw -304(%r30),%r20 ;# 3871 reload_outsi+2/5
	ldw -260(%r30),%r1 ;# 8958 reload_outsi+2/5
	ldw -308(%r30),%r19 ;# 3873 reload_outsi+2/5
	ldo 1(%r1),%r1 ;# 3868 addsi3/2
	comb,<> %r19,%r20,L$0398 ;# 3875 bleu+1
	stw %r1,-260(%r30) ;# 8961 reload_outsi+2/6
	zdep %r20,28,29,%r25 ;# 3885 ashlsi3+1
	sh1addl %r20,%r25,%r25 ;# 3886 ashlsi3-2
	ldw -312(%r30),%r26 ;# 3890 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 3894 call_value_internal_symref
	zdep %r25,29,30,%r25 ;# 3892 ashlsi3+1
	comib,= 0,%r28,L$0953 ;# 3903 bleu+1
	stw %r28,-312(%r30) ;# 3898 reload_outsi+2/6
	ldw -308(%r30),%r19 ;# 3912 reload_outsi+2/5
	zdep %r19,30,31,%r19 ;# 3914 ashlsi3+1
	stw %r19,-308(%r30) ;# 3916 reload_outsi+2/6
L$0398
	ldw -304(%r30),%r20 ;# 3921 reload_outsi+2/5
	ldw -312(%r30),%r21 ;# 3923 reload_outsi+2/5
	ldw 0(%r5),%r19 ;# 3933 reload_outsi+2/5
	sh2addl %r20,%r20,%r20 ;# 3928 ashlsi3-2
	sh2addl %r20,%r21,%r21 ;# 3931 ashlsi3-2
	sub %r12,%r19,%r19 ;# 3934 subsi3/1
	stw %r19,0(%r21) ;# 3936 reload_outsi+2/6
	ldw -304(%r30),%r19 ;# 3939 reload_outsi+2/5
	ldw -312(%r30),%r20 ;# 3941 reload_outsi+2/5
	sh2addl %r19,%r19,%r19 ;# 3946 ashlsi3-2
	comib,= 0,%r10,L$0400 ;# 3951 bleu+1
	sh2addl %r19,%r20,%r20 ;# 3949 ashlsi3-2
	ldw 0(%r5),%r19 ;# 3953 reload_outsi+2/5
	sub %r10,%r19,%r19 ;# 3954 subsi3/1
	ldo 1(%r19),%r19 ;# 3955 addsi3/2
	bl L$0401,%r0 ;# 3958 jump
	stw %r19,4(%r20) ;# 3957 reload_outsi+2/6
L$0400
	stw %r0,4(%r20) ;# 3962 reload_outsi+2/6
L$0401
	ldw -304(%r30),%r20 ;# 3966 reload_outsi+2/5
	ldw -312(%r30),%r21 ;# 3968 reload_outsi+2/5
	ldw 0(%r5),%r19 ;# 3978 reload_outsi+2/5
	sh2addl %r20,%r20,%r20 ;# 3973 ashlsi3-2
	sh2addl %r20,%r21,%r21 ;# 3976 ashlsi3-2
	sub %r6,%r19,%r19 ;# 3979 subsi3/1
	stw %r19,12(%r21) ;# 3981 reload_outsi+2/6
	ldw -304(%r30),%r19 ;# 3984 reload_outsi+2/5
	ldw -312(%r30),%r20 ;# 3986 reload_outsi+2/5
	ldw -260(%r30),%r1 ;# 8964 reload_outsi+2/5
	sh2addl %r19,%r19,%r19 ;# 3991 ashlsi3-2
	sh2addl %r19,%r20,%r20 ;# 3994 ashlsi3-2
	ldi 255,%r19 ;# 3999 reload_outsi+2/2
	comb,<< %r19,%r1,L$0402 ;# 4001 bleu+1
	stw %r1,16(%r20) ;# 3996 reload_outsi+2/6
	ldw -304(%r30),%r20 ;# 4005 reload_outsi+2/5
	ldw -312(%r30),%r21 ;# 4007 reload_outsi+2/5
	ldw 0(%r5),%r19 ;# 4017 reload_outsi+2/5
	sh2addl %r20,%r20,%r20 ;# 4012 ashlsi3-2
	sh2addl %r20,%r21,%r21 ;# 4015 ashlsi3-2
	sub %r6,%r19,%r19 ;# 4018 subsi3/1
	ldo 2(%r19),%r19 ;# 4019 addsi3/2
	stw %r19,8(%r21) ;# 4021 reload_outsi+2/6
	ldw 0(%r5),%r4 ;# 8087 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8090 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8088 subsi3/1
	ldo 3(%r19),%r19 ;# 8089 addsi3/2
	comb,>>=,n %r20,%r19,L$0407 ;# 8091 bleu+1
	ldil L'65536,%r3 ;# 8593 reload_outsi+2/3
L$0408
	comb,= %r3,%r20,L$0944 ;# 4049 bleu+1
	zdep %r20,30,31,%r19 ;# 4059 ashlsi3+1
	comb,>>= %r3,%r19,L$0413 ;# 4067 bleu+1
	stw %r19,4(%r5) ;# 4061 reload_outsi+2/6
	stw %r3,4(%r5) ;# 4070 reload_outsi+2/6
L$0413
	ldw 0(%r5),%r26 ;# 4077 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 4081 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 4079 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 4089 bleu+1
	stw %r28,0(%r5) ;# 4085 reload_outsi+2/6
	comb,= %r28,%r4,L$0406 ;# 4099 bleu+1
	sub %r6,%r4,%r19 ;# 4101 subsi3/1
	comib,= 0,%r10,L$0416 ;# 4110 bleu+1
	addl %r28,%r19,%r6 ;# 4104 addsi3/1
	sub %r10,%r4,%r19 ;# 4111 subsi3/1
	addl %r28,%r19,%r10 ;# 4114 addsi3/1
L$0416
	comib,= 0,%r8,L$0417 ;# 4117 bleu+1
	sub %r8,%r4,%r19 ;# 4118 subsi3/1
	addl %r28,%r19,%r8 ;# 4121 addsi3/1
L$0417
	comib,= 0,%r9,L$0406 ;# 4124 bleu+1
	sub %r9,%r4,%r19 ;# 4125 subsi3/1
	addl %r28,%r19,%r9 ;# 4128 addsi3/1
L$0406
	ldw 0(%r5),%r4 ;# 4029 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 4033 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 4030 subsi3/1
	ldo 3(%r19),%r19 ;# 4031 addsi3/2
	comb,<< %r20,%r19,L$0408
	nop ;# 4035 bleu+1
L$0407
	ldi 5,%r19 ;# 4150 movqi+1/2
	stbs,ma %r19,1(%r6) ;# 4151 movqi+1/6
	ldb -257(%r30),%r1 ;# 4156 movqi+1/5
	stbs,ma %r1,1(%r6) ;# 8968 movqi+1/6
	stbs,ma %r0,1(%r6) ;# 4159 movqi+1/6
L$0402
	ldi 0,%r10 ;# 4182 reload_outsi+2/2
	ldi 0,%r8 ;# 4185 reload_outsi+2/2
	copy %r6,%r12 ;# 4188 reload_outsi+2/1
	ldw -304(%r30),%r19 ;# 4174 reload_outsi+2/5
	ldi 0,%r9 ;# 4191 reload_outsi+2/2
	ldo 1(%r19),%r19 ;# 4175 addsi3/2
	bl L$0043,%r0 ;# 4193 jump
	stw %r19,-304(%r30) ;# 4179 reload_outsi+2/6
L$0422
	bb,< %r15,18,L$0397 ;# 4201 bleu+3
	ldw -304(%r30),%r19 ;# 4207 reload_outsi+2/5
	comib,<>,n 0,%r19,L$0374 ;# 4209 bleu+1
	bb,>=,n %r15,14,L$0950 ;# 4215 movsi-4
	bl,n L$0397,%r0 ;# 4230 jump
L$0374
	comib,=,n 0,%r10,L$0427 ;# 4237 bleu+1
	ldw 0(%r5),%r4 ;# 8095 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8098 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8096 subsi3/1
	ldo 1(%r19),%r19 ;# 8097 addsi3/2
	comb,>>=,n %r20,%r19,L$0432 ;# 8099 bleu+1
	ldil L'65536,%r3 ;# 8591 reload_outsi+2/3
L$0433
	comb,= %r3,%r20,L$0944 ;# 4266 bleu+1
	zdep %r20,30,31,%r19 ;# 4276 ashlsi3+1
	comb,>>= %r3,%r19,L$0438 ;# 4284 bleu+1
	stw %r19,4(%r5) ;# 4278 reload_outsi+2/6
	stw %r3,4(%r5) ;# 4287 reload_outsi+2/6
L$0438
	ldw 0(%r5),%r26 ;# 4294 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 4298 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 4296 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 4306 bleu+1
	stw %r28,0(%r5) ;# 4302 reload_outsi+2/6
	comb,= %r28,%r4,L$0431 ;# 4316 bleu+1
	sub %r6,%r4,%r19 ;# 4318 subsi3/1
	addl %r28,%r19,%r6 ;# 4321 addsi3/1
	sub %r12,%r4,%r19 ;# 4322 subsi3/1
	comib,= 0,%r10,L$0441 ;# 4327 bleu+1
	addl %r28,%r19,%r12 ;# 4325 addsi3/1
	sub %r10,%r4,%r19 ;# 4328 subsi3/1
	addl %r28,%r19,%r10 ;# 4331 addsi3/1
L$0441
	comib,= 0,%r8,L$0442 ;# 4334 bleu+1
	sub %r8,%r4,%r19 ;# 4335 subsi3/1
	addl %r28,%r19,%r8 ;# 4338 addsi3/1
L$0442
	comib,= 0,%r9,L$0431 ;# 4341 bleu+1
	sub %r9,%r4,%r19 ;# 4342 subsi3/1
	addl %r28,%r19,%r9 ;# 4345 addsi3/1
L$0431
	ldw 0(%r5),%r4 ;# 4246 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 4250 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 4247 subsi3/1
	ldo 1(%r19),%r19 ;# 4248 addsi3/2
	comb,<< %r20,%r19,L$0433
	nop ;# 4252 bleu+1
L$0432
	ldi 19,%r19 ;# 4367 movqi+1/2
	stbs,ma %r19,1(%r6) ;# 4368 movqi+1/6
	uaddcm %r6,%r10,%r24 ;# 4381 adddi3+1
	ldi 13,%r26 ;# 4384 reload_outsi+2/2
	copy %r10,%r25 ;# 4386 reload_outsi+2/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl store_op1,%r2 ;# 4390 call_internal_symref
	ldo -3(%r24),%r24 ;# 4388 addsi3/2
L$0427
	ldw -304(%r30),%r19 ;# 4395 reload_outsi+2/5
	comib,<>,n 0,%r19,L$0447 ;# 4397 bleu+1
	bb,<,n %r15,14,L$0076 ;# 4403 bleu+3
L$0950
	.CALL ARGW0=GR
	bl free,%r2 ;# 4414 call_internal_symref
	ldw -312(%r30),%r26 ;# 4412 reload_outsi+2/5
	bl L$0867,%r0 ;# 4418 jump
	ldi 16,%r28 ;# 4416 reload_outsi+2/2
L$0447
	ldo -1(%r19),%r19 ;# 4427 addsi3/2
	stw %r19,-304(%r30) ;# 4431 reload_outsi+2/6
	ldw -312(%r30),%r20 ;# 4436 reload_outsi+2/5
	sh2addl %r19,%r19,%r19 ;# 4441 ashlsi3-2
	sh2addl %r19,%r20,%r20 ;# 4444 ashlsi3-2
	ldw 0(%r5),%r21 ;# 4446 reload_outsi+2/5
	ldw 0(%r20),%r19 ;# 4448 reload_outsi+2/5
	ldw 4(%r20),%r20 ;# 4464 reload_outsi+2/5
	comib,= 0,%r20,L$0450 ;# 4466 bleu+1
	addl %r21,%r19,%r12 ;# 4449 addsi3/1
	addl %r21,%r20,%r19 ;# 4483 addsi3/1
	bl L$0451,%r0 ;# 4485 jump
	ldo -1(%r19),%r10 ;# 4484 addsi3/2
L$0450
	ldi 0,%r10 ;# 4489 reload_outsi+2/2
L$0451
	ldw -304(%r30),%r19 ;# 4493 reload_outsi+2/5
	ldw -312(%r30),%r20 ;# 4495 reload_outsi+2/5
	ldw 0(%r5),%r21 ;# 4505 reload_outsi+2/5
	sh2addl %r19,%r19,%r19 ;# 4500 ashlsi3-2
	sh2addl %r19,%r20,%r20 ;# 4503 ashlsi3-2
	ldw 12(%r20),%r19 ;# 4507 reload_outsi+2/5
	ldw 16(%r20),%r7 ;# 4523 reload_outsi+2/5
	addl %r21,%r19,%r8 ;# 4508 addsi3/1
	ldi 255,%r19 ;# 4529 reload_outsi+2/2
	comb,<< %r19,%r7,L$0043 ;# 4531 bleu+1
	ldi 0,%r9 ;# 4526 reload_outsi+2/2
	ldw 8(%r20),%r19 ;# 4550 reload_outsi+2/5
	ldw -260(%r30),%r1 ;# 8971 reload_outsi+2/5
	addl %r21,%r19,%r19 ;# 4551 addsi3/1
	sub %r1,%r7,%r20 ;# 4557 subsi3/1
	stb %r20,0(%r19) ;# 4559 movqi+1/6
	ldw 0(%r5),%r4 ;# 8103 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8106 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8104 subsi3/1
	ldo 3(%r19),%r19 ;# 8105 addsi3/2
	comb,>>=,n %r20,%r19,L$0457 ;# 8107 bleu+1
	ldil L'65536,%r3 ;# 8589 reload_outsi+2/3
L$0458
	comb,= %r3,%r20,L$0944 ;# 4587 bleu+1
	zdep %r20,30,31,%r19 ;# 4597 ashlsi3+1
	comb,>>= %r3,%r19,L$0463 ;# 4605 bleu+1
	stw %r19,4(%r5) ;# 4599 reload_outsi+2/6
	stw %r3,4(%r5) ;# 4608 reload_outsi+2/6
L$0463
	ldw 0(%r5),%r26 ;# 4615 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 4619 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 4617 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 4627 bleu+1
	stw %r28,0(%r5) ;# 4623 reload_outsi+2/6
	comb,= %r28,%r4,L$0456 ;# 4637 bleu+1
	sub %r6,%r4,%r19 ;# 4639 subsi3/1
	addl %r28,%r19,%r6 ;# 4642 addsi3/1
	sub %r12,%r4,%r19 ;# 4643 subsi3/1
	comib,= 0,%r10,L$0466 ;# 4648 bleu+1
	addl %r28,%r19,%r12 ;# 4646 addsi3/1
	sub %r10,%r4,%r19 ;# 4649 subsi3/1
	addl %r28,%r19,%r10 ;# 4652 addsi3/1
L$0466
	comib,= 0,%r8,L$0467 ;# 4655 bleu+1
	sub %r8,%r4,%r19 ;# 4656 subsi3/1
	addl %r28,%r19,%r8 ;# 4659 addsi3/1
L$0467
	comib,= 0,%r9,L$0456 ;# 4662 bleu+1
	sub %r9,%r4,%r19 ;# 4663 subsi3/1
	addl %r28,%r19,%r9 ;# 4666 addsi3/1
L$0456
	ldw 0(%r5),%r4 ;# 4567 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 4571 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 4568 subsi3/1
	ldo 3(%r19),%r19 ;# 4569 addsi3/2
	comb,<< %r20,%r19,L$0458
	nop ;# 4573 bleu+1
L$0457
	ldi 6,%r19 ;# 4688 movqi+1/2
	stbs,ma %r19,1(%r6) ;# 4689 movqi+1/6
	stbs,ma %r7,1(%r6) ;# 4694 movqi+1/6
	ldw -260(%r30),%r1 ;# 8974 reload_outsi+2/5
	sub %r1,%r7,%r19 ;# 4700 subsi3/1
	bl L$0043,%r0 ;# 4720 jump
	stbs,ma %r19,1(%r6) ;# 4702 movqi+1/6
L$0472
	ldo R'33792(%r19),%r19 ;# 4725 movhi-2
	and %r15,%r19,%r19 ;# 4726 andsi3/1
	comib,<>,n 0,%r19,L$0397 ;# 4728 bleu+1
L$0378
	bb,<,n %r15,21,L$0076 ;# 4739 bleu+3
	ldw 0(%r5),%r3 ;# 8111 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8114 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 8112 subsi3/1
	ldo 3(%r19),%r19 ;# 8113 addsi3/2
	comb,>>=,n %r20,%r19,L$0476 ;# 8115 bleu+1
	ldil L'65536,%r4 ;# 8587 reload_outsi+2/3
L$0477
	comb,= %r4,%r20,L$0944 ;# 4768 bleu+1
	zdep %r20,30,31,%r19 ;# 4778 ashlsi3+1
	comb,>>= %r4,%r19,L$0482 ;# 4786 bleu+1
	stw %r19,4(%r5) ;# 4780 reload_outsi+2/6
	stw %r4,4(%r5) ;# 4789 reload_outsi+2/6
L$0482
	ldw 0(%r5),%r26 ;# 4796 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 4800 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 4798 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 4808 bleu+1
	stw %r28,0(%r5) ;# 4804 reload_outsi+2/6
	comb,= %r28,%r3,L$0475 ;# 4818 bleu+1
	sub %r6,%r3,%r19 ;# 4820 subsi3/1
	addl %r28,%r19,%r6 ;# 4823 addsi3/1
	sub %r12,%r3,%r19 ;# 4824 subsi3/1
	comib,= 0,%r10,L$0485 ;# 4829 bleu+1
	addl %r28,%r19,%r12 ;# 4827 addsi3/1
	sub %r10,%r3,%r19 ;# 4830 subsi3/1
	addl %r28,%r19,%r10 ;# 4833 addsi3/1
L$0485
	comib,= 0,%r8,L$0486 ;# 4836 bleu+1
	sub %r8,%r3,%r19 ;# 4837 subsi3/1
	addl %r28,%r19,%r8 ;# 4840 addsi3/1
L$0486
	comib,= 0,%r9,L$0475 ;# 4843 bleu+1
	sub %r9,%r3,%r19 ;# 4844 subsi3/1
	addl %r28,%r19,%r9 ;# 4847 addsi3/1
L$0475
	ldw 0(%r5),%r3 ;# 4748 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 4752 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 4749 subsi3/1
	ldo 3(%r19),%r19 ;# 4750 addsi3/2
	comb,<< %r20,%r19,L$0477
	nop ;# 4754 bleu+1
L$0476
	ldi 14,%r26 ;# 4873 reload_outsi+2/2
	copy %r12,%r25 ;# 4875 reload_outsi+2/1
	sub %r6,%r25,%r24 ;# 4870 subsi3/1
	ldo 3(%r24),%r24 ;# 4877 addsi3/2
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl insert_op1,%r2 ;# 4881 call_internal_symref
	copy %r6,%r23 ;# 4879 reload_outsi+2/1
	ldi 0,%r9 ;# 4884 reload_outsi+2/2
	comib,= 0,%r10,L$0490 ;# 4889 bleu+1
	ldo 3(%r6),%r6 ;# 4886 addsi3/2
	ldi 13,%r26 ;# 4894 reload_outsi+2/2
	copy %r10,%r25 ;# 4896 reload_outsi+2/1
	sub %r6,%r25,%r24 ;# 4891 subsi3/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl store_op1,%r2 ;# 4900 call_internal_symref
	ldo -3(%r24),%r24 ;# 4898 addsi3/2
L$0490
	ldw 0(%r5),%r3 ;# 8119 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8122 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 8120 subsi3/1
	ldo 3(%r19),%r19 ;# 8121 addsi3/2
	comb,>>= %r20,%r19,L$0492 ;# 8123 bleu+1
	copy %r6,%r10 ;# 4904 reload_outsi+2/1
	ldil L'65536,%r4 ;# 8585 reload_outsi+2/3
L$0493
	comb,= %r4,%r20,L$0944 ;# 4929 bleu+1
	zdep %r20,30,31,%r19 ;# 4939 ashlsi3+1
	comb,>>= %r4,%r19,L$0498 ;# 4947 bleu+1
	stw %r19,4(%r5) ;# 4941 reload_outsi+2/6
	stw %r4,4(%r5) ;# 4950 reload_outsi+2/6
L$0498
	ldw 0(%r5),%r26 ;# 4957 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 4961 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 4959 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 4969 bleu+1
	stw %r28,0(%r5) ;# 4965 reload_outsi+2/6
	comb,= %r28,%r3,L$0491 ;# 4979 bleu+1
	sub %r6,%r3,%r19 ;# 4981 subsi3/1
	comib,= 0,%r10,L$0501 ;# 4990 bleu+1
	addl %r28,%r19,%r6 ;# 4984 addsi3/1
	sub %r10,%r3,%r19 ;# 4991 subsi3/1
	addl %r28,%r19,%r10 ;# 4994 addsi3/1
L$0501
	comib,= 0,%r8,L$0502 ;# 4997 bleu+1
	sub %r8,%r3,%r19 ;# 4998 subsi3/1
	addl %r28,%r19,%r8 ;# 5001 addsi3/1
L$0502
	comib,= 0,%r9,L$0491 ;# 5004 bleu+1
	sub %r9,%r3,%r19 ;# 5005 subsi3/1
	addl %r28,%r19,%r9 ;# 5008 addsi3/1
L$0491
	ldw 0(%r5),%r3 ;# 4909 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 4913 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 4910 subsi3/1
	ldo 3(%r19),%r19 ;# 4911 addsi3/2
	comb,<< %r20,%r19,L$0493
	nop ;# 4915 bleu+1
L$0492
	ldo 3(%r6),%r6 ;# 5030 addsi3/2
	ldi 0,%r8 ;# 5033 reload_outsi+2/2
	bl L$0043,%r0 ;# 5038 jump
	copy %r6,%r12 ;# 5036 reload_outsi+2/1
L$0506
	bb,>= %r15,22,L$0397 ;# 5046 movsi-4
	ldi 4608,%r20 ;# 5048 reload_outsi+2/2
	and %r15,%r20,%r19 ;# 5049 andsi3/1
	comb,= %r20,%r19,L$0397 ;# 5053 bleu+1
	ldw -296(%r30),%r20 ;# 5055 reload_outsi+2/5
	ldw -276(%r30),%r1 ;# 8977 reload_outsi+2/5
	ldo -2(%r20),%r19 ;# 5056 addsi3/2
	comb,<> %r1,%r19,L$1175 ;# 5058 bleu+1
	ldi -1,%r4 ;# 5074 reload_outsi+2/2
	comb,=,n %r16,%r20,L$0397 ;# 5062 bleu+1
L$1174
	ldw -296(%r30),%r20 ;# 5079 reload_outsi+2/5
L$1175
	copy %r4,%r11 ;# 5076 reload_outsi+2/1
	comb,<> %r16,%r20,L$0517 ;# 5085 bleu+1
	ldo -1(%r20),%r23 ;# 5080 addsi3/2
	bb,>=,n %r15,19,L$0915 ;# 5092 movsi-4
	bl L$1182,%r0 ;# 5107 jump
	stw %r23,-296(%r30) ;# 5968 reload_outsi+2/6
L$0517
	ldo 1(%r20),%r19 ;# 5134 addsi3/2
	stw %r19,-296(%r30) ;# 5136 reload_outsi+2/6
	comib,= 0,%r14,L$0515 ;# 5143 bleu+1
	ldb 0(%r20),%r7 ;# 5139 zero_extendqisi2/2
	addl %r14,%r7,%r19 ;# 5144 addsi3/1
	ldb 0(%r19),%r7 ;# 5147 zero_extendqisi2/2
L$0515
	addil LR'__ctype-$global$,%r27 ;# 8257 pic2_lo_sum+1
	ldw RR'__ctype-$global$(%r1),%r21 ;# 8259 reload_outsi+2/5
	addl %r21,%r7,%r19 ;# 8260 addsi3/1
	ldb 0(%r19),%r19 ;# 8261 movqi+1/5
	bb,>= %r19,29,L$0513 ;# 8265 movsi-4
	ldi 4,%r20 ;# 8263 reload_outsi+2/2
	copy %r20,%r22 ;# 8583 reload_outsi+2/1
L$0522
	comiclr,< -1,%r11,%r0 ;# 8128 movsicc+1/1
	ldi 0,%r11
	sh2addl %r11,%r11,%r19 ;# 5188 ashlsi3-2
	sh1addl %r19,%r7,%r19 ;# 5191 ashlsi3-2
	ldw -296(%r30),%r20 ;# 5194 reload_outsi+2/5
	comb,= %r16,%r20,L$0513 ;# 5196 bleu+1
	ldo -48(%r19),%r11 ;# 5192 addsi3/2
	ldo 1(%r20),%r19 ;# 5215 addsi3/2
	stw %r19,-296(%r30) ;# 5217 reload_outsi+2/6
	comib,= 0,%r14,L$0520 ;# 5224 bleu+1
	ldb 0(%r20),%r7 ;# 5220 zero_extendqisi2/2
	addl %r14,%r7,%r19 ;# 5225 addsi3/1
	ldb 0(%r19),%r7 ;# 5228 zero_extendqisi2/2
L$0520
	addl %r21,%r7,%r19 ;# 5166 addsi3/1
	ldb 0(%r19),%r19 ;# 5168 movqi+1/5
	and %r19,%r22,%r19 ;# 5172 andsi3/1
	comib,<> 0,%r19,L$0522
	nop ;# 5174 bleu+1
L$0513
	ldi 44,%r19 ;# 5252 reload_outsi+2/2
	comb,<> %r19,%r7,L$0532 ;# 5254 bleu+1
	ldw -296(%r30),%r20 ;# 5259 reload_outsi+2/5
	comb,= %r16,%r20,L$0533 ;# 5261 bleu+1
	ldo 1(%r20),%r19 ;# 5278 addsi3/2
	stw %r19,-296(%r30) ;# 5280 reload_outsi+2/6
	comib,= 0,%r14,L$0535 ;# 5287 bleu+1
	ldb 0(%r20),%r7 ;# 5283 zero_extendqisi2/2
	addl %r14,%r7,%r19 ;# 5288 addsi3/1
	ldb 0(%r19),%r7 ;# 5291 zero_extendqisi2/2
L$0535
	addil LR'__ctype-$global$,%r27 ;# 8269 pic2_lo_sum+1
	ldw RR'__ctype-$global$(%r1),%r21 ;# 8271 reload_outsi+2/5
	addl %r21,%r7,%r19 ;# 8272 addsi3/1
	ldb 0(%r19),%r19 ;# 8273 movqi+1/5
	bb,>= %r19,29,L$0533 ;# 8277 movsi-4
	ldi 4,%r20 ;# 8275 reload_outsi+2/2
	copy %r20,%r22 ;# 8578 reload_outsi+2/1
L$0542
	comiclr,< -1,%r4,%r0 ;# 8132 movsicc+1/1
	ldi 0,%r4
	sh2addl %r4,%r4,%r19 ;# 5332 ashlsi3-2
	sh1addl %r19,%r7,%r19 ;# 5335 ashlsi3-2
	ldw -296(%r30),%r20 ;# 5338 reload_outsi+2/5
	comb,= %r16,%r20,L$0533 ;# 5340 bleu+1
	ldo -48(%r19),%r4 ;# 5336 addsi3/2
	ldo 1(%r20),%r19 ;# 5359 addsi3/2
	stw %r19,-296(%r30) ;# 5361 reload_outsi+2/6
	comib,= 0,%r14,L$0540 ;# 5368 bleu+1
	ldb 0(%r20),%r7 ;# 5364 zero_extendqisi2/2
	addl %r14,%r7,%r19 ;# 5369 addsi3/1
	ldb 0(%r19),%r7 ;# 5372 zero_extendqisi2/2
L$0540
	addl %r21,%r7,%r19 ;# 5310 addsi3/1
	ldb 0(%r19),%r19 ;# 5312 movqi+1/5
	and %r19,%r22,%r19 ;# 5316 andsi3/1
	comib,<> 0,%r19,L$0542
	nop ;# 5318 bleu+1
L$0533
	comiclr,< -1,%r4,%r0 ;# 8136 beq-1/4
	zdepi -1,31,15,%r4
	bl,n L$0553,%r0 ;# 5401 jump
L$0532
	copy %r11,%r4 ;# 5406 reload_outsi+2/1
L$0553
	comib,> 0,%r11,L$0951 ;# 5410 bleu+1
	zdepi -1,31,15,%r19 ;# 5412 reload_outsi+2/4
	comb,<,n %r19,%r4,L$0951 ;# 5414 bleu+1
	comb,<,n %r4,%r11,L$0951 ;# 5416 bleu+1
	bb,< %r15,19,L$1176 ;# 5451 bleu+3
	ldi 125,%r19 ;# 5514 reload_outsi+2/2
	ldi 92,%r19 ;# 5455 reload_outsi+2/2
	comb,<> %r19,%r7,L$0915 ;# 5457 bleu+1
	ldw -296(%r30),%r20 ;# 5473 reload_outsi+2/5
	comb,= %r16,%r20,L$0922 ;# 5475 bleu+1
	ldo 1(%r20),%r19 ;# 5484 addsi3/2
	stw %r19,-296(%r30) ;# 5486 reload_outsi+2/6
	comib,= 0,%r14,L$0558 ;# 5493 bleu+1
	ldb 0(%r20),%r7 ;# 5489 zero_extendqisi2/2
	addl %r14,%r7,%r19 ;# 5494 addsi3/1
	ldb 0(%r19),%r7 ;# 5497 zero_extendqisi2/2
L$0558
	ldi 125,%r19 ;# 5514 reload_outsi+2/2
L$1176
	comb,=,n %r19,%r7,L$0566 ;# 5516 bleu+1
L$0951
	bb,<,n %r15,19,L$0511 ;# 5523 bleu+3
	.CALL ARGW0=GR
	bl free,%r2 ;# 5534 call_internal_symref
	ldw -312(%r30),%r26 ;# 5532 reload_outsi+2/5
	bl L$0867,%r0 ;# 5538 jump
	ldi 10,%r28 ;# 5536 reload_outsi+2/2
L$0566
	comib,<>,n 0,%r8,L$0569 ;# 5545 bleu+1
	bb,<,n %r15,26,L$0917 ;# 5552 bleu+3
	bb,>=,n %r15,27,L$0511 ;# 5571 movsi-4
	copy %r6,%r8 ;# 5574 reload_outsi+2/1
L$0569
	comib,<> 0,%r4,L$0574 ;# 5587 bleu+1
	ldi 10,%r13 ;# 8980 reload_outsi+2/2
	ldw 0(%r5),%r3 ;# 8139 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8142 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 8140 subsi3/1
	ldo 3(%r19),%r19 ;# 8141 addsi3/2
	comb,>>=,n %r20,%r19,L$0576 ;# 8143 bleu+1
	ldil L'65536,%r4 ;# 8573 reload_outsi+2/3
L$0577
	comb,= %r4,%r20,L$0944 ;# 5613 bleu+1
	zdep %r20,30,31,%r19 ;# 5623 ashlsi3+1
	comb,>>= %r4,%r19,L$0582 ;# 5631 bleu+1
	stw %r19,4(%r5) ;# 5625 reload_outsi+2/6
	stw %r4,4(%r5) ;# 5634 reload_outsi+2/6
L$0582
	ldw 0(%r5),%r26 ;# 5641 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 5645 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 5643 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 5653 bleu+1
	stw %r28,0(%r5) ;# 5649 reload_outsi+2/6
	comb,= %r28,%r3,L$0575 ;# 5663 bleu+1
	sub %r6,%r3,%r19 ;# 5665 subsi3/1
	addl %r28,%r19,%r6 ;# 5668 addsi3/1
	sub %r12,%r3,%r19 ;# 5669 subsi3/1
	comib,= 0,%r10,L$0585 ;# 5674 bleu+1
	addl %r28,%r19,%r12 ;# 5672 addsi3/1
	sub %r10,%r3,%r19 ;# 5675 subsi3/1
	addl %r28,%r19,%r10 ;# 5678 addsi3/1
L$0585
	comib,= 0,%r8,L$0586 ;# 5681 bleu+1
	sub %r8,%r3,%r19 ;# 5682 subsi3/1
	addl %r28,%r19,%r8 ;# 5685 addsi3/1
L$0586
	comib,= 0,%r9,L$0575 ;# 5688 bleu+1
	sub %r9,%r3,%r19 ;# 5689 subsi3/1
	addl %r28,%r19,%r9 ;# 5692 addsi3/1
L$0575
	ldw 0(%r5),%r3 ;# 5593 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 5597 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 5594 subsi3/1
	ldo 3(%r19),%r19 ;# 5595 addsi3/2
	comb,<< %r20,%r19,L$0577
	nop ;# 5599 bleu+1
L$0576
	ldi 12,%r26 ;# 5718 reload_outsi+2/2
	copy %r8,%r25 ;# 5720 reload_outsi+2/1
	sub %r6,%r8,%r24 ;# 5722 subsi3/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl insert_op1,%r2 ;# 5726 call_internal_symref
	copy %r6,%r23 ;# 5724 reload_outsi+2/1
	bl L$0590,%r0 ;# 5730 jump
	ldo 3(%r6),%r6 ;# 5728 addsi3/2
L$0574
	comiclr,> 2,%r4,%r0 ;# 8282 beq-1/2
	ldi 20,%r13
	ldw 0(%r5),%r3 ;# 8285 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8288 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 8286 subsi3/1
	addl %r19,%r13,%r19 ;# 8287 addsi3/1
	comb,>>=,n %r20,%r19,L$0868 ;# 8289 bleu+1
	ldil L'65536,%r7 ;# 8571 reload_outsi+2/3
L$0595
	comb,= %r7,%r20,L$0944 ;# 5769 bleu+1
	zdep %r20,30,31,%r19 ;# 5779 ashlsi3+1
	comb,>>= %r7,%r19,L$0600 ;# 5787 bleu+1
	stw %r19,4(%r5) ;# 5781 reload_outsi+2/6
	stw %r7,4(%r5) ;# 5790 reload_outsi+2/6
L$0600
	ldw 0(%r5),%r26 ;# 5797 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 5801 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 5799 reload_outsi+2/5
	comib,= 0,%r28,L$0953 ;# 5809 bleu+1
	stw %r28,0(%r5) ;# 5805 reload_outsi+2/6
	comb,= %r28,%r3,L$0593 ;# 5819 bleu+1
	sub %r6,%r3,%r19 ;# 5821 subsi3/1
	addl %r28,%r19,%r6 ;# 5824 addsi3/1
	sub %r12,%r3,%r19 ;# 5825 subsi3/1
	comib,= 0,%r10,L$0603 ;# 5830 bleu+1
	addl %r28,%r19,%r12 ;# 5828 addsi3/1
	sub %r10,%r3,%r19 ;# 5831 subsi3/1
	addl %r28,%r19,%r10 ;# 5834 addsi3/1
L$0603
	comib,= 0,%r8,L$0604 ;# 5837 bleu+1
	sub %r8,%r3,%r19 ;# 5838 subsi3/1
	addl %r28,%r19,%r8 ;# 5841 addsi3/1
L$0604
	comib,= 0,%r9,L$0593 ;# 5844 bleu+1
	sub %r9,%r3,%r19 ;# 5845 subsi3/1
	addl %r28,%r19,%r9 ;# 5848 addsi3/1
L$0593
	ldw 0(%r5),%r3 ;# 5749 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 5753 reload_outsi+2/5
	sub %r6,%r3,%r19 ;# 5750 subsi3/1
	addl %r19,%r13,%r19 ;# 5751 addsi3/1
	comb,<< %r20,%r19,L$0595
	nop ;# 5755 bleu+1
L$0868
	comib,>= 1,%r4,L$0608 ;# 5872 bleu+1
	ldo 5(%r6),%r19 ;# 5870 addsi3/2
	sub %r19,%r8,%r19 ;# 5874 subsi3/1
	bl L$0609,%r0 ;# 5876 jump
	ldo 2(%r19),%r24 ;# 5875 addsi3/2
L$0608
	sub %r19,%r8,%r19 ;# 5879 subsi3/1
	ldo -3(%r19),%r24 ;# 5880 addsi3/2
L$0609
	ldi 20,%r26 ;# 5885 reload_outsi+2/2
	copy %r8,%r25 ;# 5887 reload_outsi+2/1
	copy %r11,%r23 ;# 5891 reload_outsi+2/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl insert_op2,%r2 ;# 5893 call_internal_symref
	stw %r6,-52(%r30) ;# 5883 reload_outsi+2/6
	ldo 5(%r6),%r6 ;# 5895 addsi3/2
	ldi 22,%r26 ;# 5900 reload_outsi+2/2
	copy %r8,%r25 ;# 5902 reload_outsi+2/1
	ldi 5,%r24 ;# 5904 reload_outsi+2/2
	copy %r11,%r23 ;# 5906 reload_outsi+2/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl insert_op2,%r2 ;# 5908 call_internal_symref
	stw %r6,-52(%r30) ;# 5898 reload_outsi+2/6
	comib,>= 1,%r4,L$0590 ;# 5913 bleu+1
	ldo 5(%r6),%r6 ;# 5910 addsi3/2
	ldi 21,%r26 ;# 5921 reload_outsi+2/2
	copy %r6,%r25 ;# 5923 reload_outsi+2/1
	sub %r8,%r6,%r24 ;# 5917 subsi3/1
	ldo 2(%r24),%r24 ;# 5925 addsi3/2
	ldo -1(%r4),%r4 ;# 5919 addsi3/2
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl store_op2,%r2 ;# 5929 call_internal_symref
	copy %r4,%r23 ;# 5927 reload_outsi+2/1
	ldo 5(%r6),%r6 ;# 5931 addsi3/2
	stw %r6,-52(%r30) ;# 5936 reload_outsi+2/6
	ldi 22,%r26 ;# 5938 reload_outsi+2/2
	copy %r8,%r25 ;# 5940 reload_outsi+2/1
	sub %r6,%r8,%r24 ;# 5942 subsi3/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR
	bl insert_op2,%r2 ;# 5946 call_internal_symref
	copy %r4,%r23 ;# 5944 reload_outsi+2/1
	ldo 5(%r6),%r6 ;# 5948 addsi3/2
L$0590
	bl L$0043,%r0 ;# 5963 jump
	ldi 0,%r9 ;# 5956 reload_outsi+2/2
L$0511
	stw %r23,-296(%r30) ;# 5968 reload_outsi+2/6
L$1182
	ldw -296(%r30),%r20 ;# 5977 reload_outsi+2/5
	comb,= %r16,%r20,L$0922 ;# 5979 bleu+1
	ldo 1(%r20),%r21 ;# 5988 addsi3/2
	ldb 0(%r20),%r20 ;# 5992 movqi+1/5
	stw %r21,-296(%r30) ;# 5990 reload_outsi+2/6
	comib,= 0,%r14,L$0612 ;# 5997 bleu+1
	extru %r20,31,8,%r7 ;# 5993 zero_extendqisi2/1
	addl %r14,%r7,%r19 ;# 5998 addsi3/1
	ldb 0(%r19),%r7 ;# 6001 zero_extendqisi2/2
L$0612
	bb,< %r15,19,L$0076 ;# 6019 bleu+3
	ldw -276(%r30),%r1 ;# 8983 reload_outsi+2/5
	comb,>>= %r1,%r21,L$0076 ;# 6025 bleu+1
	extrs %r20,31,8,%r20 ;# 6030 extendqisi2
	ldi 92,%r19 ;# 6032 reload_outsi+2/2
	comb,=,n %r19,%r20,L$0397 ;# 6034 bleu+1
	bl,n L$0076,%r0 ;# 6042 jump
L$0619
	ldw 4(%r5),%r20 ;# 8151 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8149 subsi3/1
	ldo 1(%r19),%r19 ;# 8150 addsi3/2
	comb,>>= %r20,%r19,L$0624 ;# 8152 bleu+1
	copy %r6,%r8 ;# 6047 reload_outsi+2/1
	ldil L'65536,%r3 ;# 8569 reload_outsi+2/3
L$0625
	comb,= %r3,%r20,L$0944 ;# 6075 bleu+1
	zdep %r20,30,31,%r19 ;# 6085 ashlsi3+1
	comb,>>= %r3,%r19,L$0630 ;# 6093 bleu+1
	stw %r19,4(%r5) ;# 6087 reload_outsi+2/6
	stw %r3,4(%r5) ;# 6096 reload_outsi+2/6
L$0630
	ldw 0(%r5),%r26 ;# 6103 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 6107 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 6105 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 6115 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 6111 reload_outsi+2/6
	comb,= %r28,%r4,L$0623 ;# 6125 bleu+1
	sub %r6,%r4,%r19 ;# 6127 subsi3/1
	addl %r28,%r19,%r6 ;# 6130 addsi3/1
	sub %r12,%r4,%r19 ;# 6131 subsi3/1
	comib,= 0,%r10,L$0633 ;# 6136 bleu+1
	addl %r28,%r19,%r12 ;# 6134 addsi3/1
	sub %r10,%r4,%r19 ;# 6137 subsi3/1
	addl %r28,%r19,%r10 ;# 6140 addsi3/1
L$0633
	comib,= 0,%r8,L$0634 ;# 6143 bleu+1
	sub %r8,%r4,%r19 ;# 6144 subsi3/1
	addl %r28,%r19,%r8 ;# 6147 addsi3/1
L$0634
	comib,= 0,%r9,L$0623 ;# 6150 bleu+1
	sub %r9,%r4,%r19 ;# 6151 subsi3/1
	addl %r28,%r19,%r9 ;# 6154 addsi3/1
L$0623
	ldw 0(%r5),%r4 ;# 6055 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 6059 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 6056 subsi3/1
	ldo 1(%r19),%r19 ;# 6057 addsi3/2
	comb,<< %r20,%r19,L$0625
	nop ;# 6061 bleu+1
L$0624
	ldi 23,%r19 ;# 6176 movqi+1/2
	bl L$0043,%r0 ;# 6189 jump
	stbs,ma %r19,1(%r6) ;# 6177 movqi+1/6
L$0639
	ldw 4(%r5),%r20 ;# 8159 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8157 subsi3/1
	ldo 1(%r19),%r19 ;# 8158 addsi3/2
	comb,>>= %r20,%r19,L$0644 ;# 8160 bleu+1
	copy %r6,%r8 ;# 6194 reload_outsi+2/1
	ldil L'65536,%r3 ;# 8567 reload_outsi+2/3
L$0645
	comb,= %r3,%r20,L$0944 ;# 6222 bleu+1
	zdep %r20,30,31,%r19 ;# 6232 ashlsi3+1
	comb,>>= %r3,%r19,L$0650 ;# 6240 bleu+1
	stw %r19,4(%r5) ;# 6234 reload_outsi+2/6
	stw %r3,4(%r5) ;# 6243 reload_outsi+2/6
L$0650
	ldw 0(%r5),%r26 ;# 6250 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 6254 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 6252 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 6262 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 6258 reload_outsi+2/6
	comb,= %r28,%r4,L$0643 ;# 6272 bleu+1
	sub %r6,%r4,%r19 ;# 6274 subsi3/1
	addl %r28,%r19,%r6 ;# 6277 addsi3/1
	sub %r12,%r4,%r19 ;# 6278 subsi3/1
	comib,= 0,%r10,L$0653 ;# 6283 bleu+1
	addl %r28,%r19,%r12 ;# 6281 addsi3/1
	sub %r10,%r4,%r19 ;# 6284 subsi3/1
	addl %r28,%r19,%r10 ;# 6287 addsi3/1
L$0653
	comib,= 0,%r8,L$0654 ;# 6290 bleu+1
	sub %r8,%r4,%r19 ;# 6291 subsi3/1
	addl %r28,%r19,%r8 ;# 6294 addsi3/1
L$0654
	comib,= 0,%r9,L$0643 ;# 6297 bleu+1
	sub %r9,%r4,%r19 ;# 6298 subsi3/1
	addl %r28,%r19,%r9 ;# 6301 addsi3/1
L$0643
	ldw 0(%r5),%r4 ;# 6202 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 6206 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 6203 subsi3/1
	ldo 1(%r19),%r19 ;# 6204 addsi3/2
	comb,<< %r20,%r19,L$0645
	nop ;# 6208 bleu+1
L$0644
	ldi 24,%r19 ;# 6323 movqi+1/2
	bl L$0043,%r0 ;# 6336 jump
	stbs,ma %r19,1(%r6) ;# 6324 movqi+1/6
L$0659
	ldw 4(%r5),%r20 ;# 8167 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8165 subsi3/1
	ldo 1(%r19),%r19 ;# 8166 addsi3/2
	comb,>>=,n %r20,%r19,L$0664 ;# 8168 bleu+1
	ldil L'65536,%r3 ;# 8565 reload_outsi+2/3
L$0665
	comb,= %r3,%r20,L$0944 ;# 6366 bleu+1
	zdep %r20,30,31,%r19 ;# 6376 ashlsi3+1
	comb,>>= %r3,%r19,L$0670 ;# 6384 bleu+1
	stw %r19,4(%r5) ;# 6378 reload_outsi+2/6
	stw %r3,4(%r5) ;# 6387 reload_outsi+2/6
L$0670
	ldw 0(%r5),%r26 ;# 6394 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 6398 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 6396 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 6406 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 6402 reload_outsi+2/6
	comb,= %r28,%r4,L$0663 ;# 6416 bleu+1
	sub %r6,%r4,%r19 ;# 6418 subsi3/1
	addl %r28,%r19,%r6 ;# 6421 addsi3/1
	sub %r12,%r4,%r19 ;# 6422 subsi3/1
	comib,= 0,%r10,L$0673 ;# 6427 bleu+1
	addl %r28,%r19,%r12 ;# 6425 addsi3/1
	sub %r10,%r4,%r19 ;# 6428 subsi3/1
	addl %r28,%r19,%r10 ;# 6431 addsi3/1
L$0673
	comib,= 0,%r8,L$0674 ;# 6434 bleu+1
	sub %r8,%r4,%r19 ;# 6435 subsi3/1
	addl %r28,%r19,%r8 ;# 6438 addsi3/1
L$0674
	comib,= 0,%r9,L$0663 ;# 6441 bleu+1
	sub %r9,%r4,%r19 ;# 6442 subsi3/1
	addl %r28,%r19,%r9 ;# 6445 addsi3/1
L$0663
	ldw 0(%r5),%r4 ;# 6346 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 6350 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 6347 subsi3/1
	ldo 1(%r19),%r19 ;# 6348 addsi3/2
	comb,<< %r20,%r19,L$0665
	nop ;# 6352 bleu+1
L$0664
	ldi 25,%r19 ;# 6467 movqi+1/2
	bl L$0043,%r0 ;# 6480 jump
	stbs,ma %r19,1(%r6) ;# 6468 movqi+1/6
L$0679
	ldw 4(%r5),%r20 ;# 8175 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8173 subsi3/1
	ldo 1(%r19),%r19 ;# 8174 addsi3/2
	comb,>>=,n %r20,%r19,L$0684 ;# 8176 bleu+1
	ldil L'65536,%r3 ;# 8563 reload_outsi+2/3
L$0685
	comb,= %r3,%r20,L$0944 ;# 6510 bleu+1
	zdep %r20,30,31,%r19 ;# 6520 ashlsi3+1
	comb,>>= %r3,%r19,L$0690 ;# 6528 bleu+1
	stw %r19,4(%r5) ;# 6522 reload_outsi+2/6
	stw %r3,4(%r5) ;# 6531 reload_outsi+2/6
L$0690
	ldw 0(%r5),%r26 ;# 6538 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 6542 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 6540 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 6550 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 6546 reload_outsi+2/6
	comb,= %r28,%r4,L$0683 ;# 6560 bleu+1
	sub %r6,%r4,%r19 ;# 6562 subsi3/1
	addl %r28,%r19,%r6 ;# 6565 addsi3/1
	sub %r12,%r4,%r19 ;# 6566 subsi3/1
	comib,= 0,%r10,L$0693 ;# 6571 bleu+1
	addl %r28,%r19,%r12 ;# 6569 addsi3/1
	sub %r10,%r4,%r19 ;# 6572 subsi3/1
	addl %r28,%r19,%r10 ;# 6575 addsi3/1
L$0693
	comib,= 0,%r8,L$0694 ;# 6578 bleu+1
	sub %r8,%r4,%r19 ;# 6579 subsi3/1
	addl %r28,%r19,%r8 ;# 6582 addsi3/1
L$0694
	comib,= 0,%r9,L$0683 ;# 6585 bleu+1
	sub %r9,%r4,%r19 ;# 6586 subsi3/1
	addl %r28,%r19,%r9 ;# 6589 addsi3/1
L$0683
	ldw 0(%r5),%r4 ;# 6490 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 6494 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 6491 subsi3/1
	ldo 1(%r19),%r19 ;# 6492 addsi3/2
	comb,<< %r20,%r19,L$0685
	nop ;# 6496 bleu+1
L$0684
	ldi 26,%r19 ;# 6611 movqi+1/2
	bl L$0043,%r0 ;# 6624 jump
	stbs,ma %r19,1(%r6) ;# 6612 movqi+1/6
L$0699
	ldw 4(%r5),%r20 ;# 8183 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8181 subsi3/1
	ldo 1(%r19),%r19 ;# 8182 addsi3/2
	comb,>>=,n %r20,%r19,L$0704 ;# 8184 bleu+1
	ldil L'65536,%r3 ;# 8561 reload_outsi+2/3
L$0705
	comb,= %r3,%r20,L$0944 ;# 6654 bleu+1
	zdep %r20,30,31,%r19 ;# 6664 ashlsi3+1
	comb,>>= %r3,%r19,L$0710 ;# 6672 bleu+1
	stw %r19,4(%r5) ;# 6666 reload_outsi+2/6
	stw %r3,4(%r5) ;# 6675 reload_outsi+2/6
L$0710
	ldw 0(%r5),%r26 ;# 6682 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 6686 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 6684 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 6694 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 6690 reload_outsi+2/6
	comb,= %r28,%r4,L$0703 ;# 6704 bleu+1
	sub %r6,%r4,%r19 ;# 6706 subsi3/1
	addl %r28,%r19,%r6 ;# 6709 addsi3/1
	sub %r12,%r4,%r19 ;# 6710 subsi3/1
	comib,= 0,%r10,L$0713 ;# 6715 bleu+1
	addl %r28,%r19,%r12 ;# 6713 addsi3/1
	sub %r10,%r4,%r19 ;# 6716 subsi3/1
	addl %r28,%r19,%r10 ;# 6719 addsi3/1
L$0713
	comib,= 0,%r8,L$0714 ;# 6722 bleu+1
	sub %r8,%r4,%r19 ;# 6723 subsi3/1
	addl %r28,%r19,%r8 ;# 6726 addsi3/1
L$0714
	comib,= 0,%r9,L$0703 ;# 6729 bleu+1
	sub %r9,%r4,%r19 ;# 6730 subsi3/1
	addl %r28,%r19,%r9 ;# 6733 addsi3/1
L$0703
	ldw 0(%r5),%r4 ;# 6634 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 6638 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 6635 subsi3/1
	ldo 1(%r19),%r19 ;# 6636 addsi3/2
	comb,<< %r20,%r19,L$0705
	nop ;# 6640 bleu+1
L$0704
	ldi 27,%r19 ;# 6755 movqi+1/2
	bl L$0043,%r0 ;# 6768 jump
	stbs,ma %r19,1(%r6) ;# 6756 movqi+1/6
L$0719
	ldw 4(%r5),%r20 ;# 8191 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8189 subsi3/1
	ldo 1(%r19),%r19 ;# 8190 addsi3/2
	comb,>>=,n %r20,%r19,L$0724 ;# 8192 bleu+1
	ldil L'65536,%r3 ;# 8559 reload_outsi+2/3
L$0725
	comb,= %r3,%r20,L$0944 ;# 6798 bleu+1
	zdep %r20,30,31,%r19 ;# 6808 ashlsi3+1
	comb,>>= %r3,%r19,L$0730 ;# 6816 bleu+1
	stw %r19,4(%r5) ;# 6810 reload_outsi+2/6
	stw %r3,4(%r5) ;# 6819 reload_outsi+2/6
L$0730
	ldw 0(%r5),%r26 ;# 6826 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 6830 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 6828 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 6838 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 6834 reload_outsi+2/6
	comb,= %r28,%r4,L$0723 ;# 6848 bleu+1
	sub %r6,%r4,%r19 ;# 6850 subsi3/1
	addl %r28,%r19,%r6 ;# 6853 addsi3/1
	sub %r12,%r4,%r19 ;# 6854 subsi3/1
	comib,= 0,%r10,L$0733 ;# 6859 bleu+1
	addl %r28,%r19,%r12 ;# 6857 addsi3/1
	sub %r10,%r4,%r19 ;# 6860 subsi3/1
	addl %r28,%r19,%r10 ;# 6863 addsi3/1
L$0733
	comib,= 0,%r8,L$0734 ;# 6866 bleu+1
	sub %r8,%r4,%r19 ;# 6867 subsi3/1
	addl %r28,%r19,%r8 ;# 6870 addsi3/1
L$0734
	comib,= 0,%r9,L$0723 ;# 6873 bleu+1
	sub %r9,%r4,%r19 ;# 6874 subsi3/1
	addl %r28,%r19,%r9 ;# 6877 addsi3/1
L$0723
	ldw 0(%r5),%r4 ;# 6778 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 6782 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 6779 subsi3/1
	ldo 1(%r19),%r19 ;# 6780 addsi3/2
	comb,<< %r20,%r19,L$0725
	nop ;# 6784 bleu+1
L$0724
	ldi 28,%r19 ;# 6899 movqi+1/2
	bl L$0043,%r0 ;# 6912 jump
	stbs,ma %r19,1(%r6) ;# 6900 movqi+1/6
L$0739
	ldw 4(%r5),%r20 ;# 8199 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8197 subsi3/1
	ldo 1(%r19),%r19 ;# 8198 addsi3/2
	comb,>>=,n %r20,%r19,L$0744 ;# 8200 bleu+1
	ldil L'65536,%r3 ;# 8557 reload_outsi+2/3
L$0745
	comb,= %r3,%r20,L$0944 ;# 6942 bleu+1
	zdep %r20,30,31,%r19 ;# 6952 ashlsi3+1
	comb,>>= %r3,%r19,L$0750 ;# 6960 bleu+1
	stw %r19,4(%r5) ;# 6954 reload_outsi+2/6
	stw %r3,4(%r5) ;# 6963 reload_outsi+2/6
L$0750
	ldw 0(%r5),%r26 ;# 6970 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 6974 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 6972 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 6982 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 6978 reload_outsi+2/6
	comb,= %r28,%r4,L$0743 ;# 6992 bleu+1
	sub %r6,%r4,%r19 ;# 6994 subsi3/1
	addl %r28,%r19,%r6 ;# 6997 addsi3/1
	sub %r12,%r4,%r19 ;# 6998 subsi3/1
	comib,= 0,%r10,L$0753 ;# 7003 bleu+1
	addl %r28,%r19,%r12 ;# 7001 addsi3/1
	sub %r10,%r4,%r19 ;# 7004 subsi3/1
	addl %r28,%r19,%r10 ;# 7007 addsi3/1
L$0753
	comib,= 0,%r8,L$0754 ;# 7010 bleu+1
	sub %r8,%r4,%r19 ;# 7011 subsi3/1
	addl %r28,%r19,%r8 ;# 7014 addsi3/1
L$0754
	comib,= 0,%r9,L$0743 ;# 7017 bleu+1
	sub %r9,%r4,%r19 ;# 7018 subsi3/1
	addl %r28,%r19,%r9 ;# 7021 addsi3/1
L$0743
	ldw 0(%r5),%r4 ;# 6922 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 6926 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 6923 subsi3/1
	ldo 1(%r19),%r19 ;# 6924 addsi3/2
	comb,<< %r20,%r19,L$0745
	nop ;# 6928 bleu+1
L$0744
	ldi 10,%r19 ;# 7043 movqi+1/2
	bl L$0043,%r0 ;# 7056 jump
	stbs,ma %r19,1(%r6) ;# 7044 movqi+1/6
L$0759
	ldw 4(%r5),%r20 ;# 8207 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8205 subsi3/1
	ldo 1(%r19),%r19 ;# 8206 addsi3/2
	comb,>>=,n %r20,%r19,L$0764 ;# 8208 bleu+1
	ldil L'65536,%r3 ;# 8555 reload_outsi+2/3
L$0765
	comb,= %r3,%r20,L$0944 ;# 7086 bleu+1
	zdep %r20,30,31,%r19 ;# 7096 ashlsi3+1
	comb,>>= %r3,%r19,L$0770 ;# 7104 bleu+1
	stw %r19,4(%r5) ;# 7098 reload_outsi+2/6
	stw %r3,4(%r5) ;# 7107 reload_outsi+2/6
L$0770
	ldw 0(%r5),%r26 ;# 7114 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 7118 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 7116 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 7126 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 7122 reload_outsi+2/6
	comb,= %r28,%r4,L$0763 ;# 7136 bleu+1
	sub %r6,%r4,%r19 ;# 7138 subsi3/1
	addl %r28,%r19,%r6 ;# 7141 addsi3/1
	sub %r12,%r4,%r19 ;# 7142 subsi3/1
	comib,= 0,%r10,L$0773 ;# 7147 bleu+1
	addl %r28,%r19,%r12 ;# 7145 addsi3/1
	sub %r10,%r4,%r19 ;# 7148 subsi3/1
	addl %r28,%r19,%r10 ;# 7151 addsi3/1
L$0773
	comib,= 0,%r8,L$0774 ;# 7154 bleu+1
	sub %r8,%r4,%r19 ;# 7155 subsi3/1
	addl %r28,%r19,%r8 ;# 7158 addsi3/1
L$0774
	comib,= 0,%r9,L$0763 ;# 7161 bleu+1
	sub %r9,%r4,%r19 ;# 7162 subsi3/1
	addl %r28,%r19,%r9 ;# 7165 addsi3/1
L$0763
	ldw 0(%r5),%r4 ;# 7066 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 7070 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 7067 subsi3/1
	ldo 1(%r19),%r19 ;# 7068 addsi3/2
	comb,<< %r20,%r19,L$0765
	nop ;# 7072 bleu+1
L$0764
	ldi 11,%r19 ;# 7187 movqi+1/2
	bl L$0043,%r0 ;# 7200 jump
	stbs,ma %r19,1(%r6) ;# 7188 movqi+1/6
L$0787
	bb,< %r15,17,L$0076 ;# 7216 bleu+3
	ldo -48(%r7),%r19 ;# 7224 addsi3/2
	ldw -260(%r30),%r1 ;# 8986 reload_outsi+2/5
	extru %r19,31,8,%r3 ;# 7225 zero_extendqisi2/1
	comb,<< %r1,%r3,L$0939 ;# 7230 bleu+1
	ldo -312(%r30),%r26 ;# 7244 addsi3/2
	.CALL ARGW0=GR,ARGW1=GR
	bl group_in_compile_stack,%r2 ;# 7248 call_value_internal_symref
	copy %r3,%r25 ;# 7246 reload_outsi+2/1
	extrs %r28,31,8,%r28 ;# 7251 extendqisi2
	comib,<>,n 0,%r28,L$0076 ;# 7253 bleu+1
	ldw 0(%r5),%r4 ;# 8212 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8215 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8213 subsi3/1
	ldo 2(%r19),%r19 ;# 8214 addsi3/2
	comb,>>= %r20,%r19,L$0795 ;# 8216 bleu+1
	copy %r6,%r8 ;# 7260 reload_outsi+2/1
	ldil L'65536,%r7 ;# 8553 reload_outsi+2/3
L$0796
	comb,= %r7,%r20,L$0944 ;# 7288 bleu+1
	zdep %r20,30,31,%r19 ;# 7298 ashlsi3+1
	comb,>>= %r7,%r19,L$0801 ;# 7306 bleu+1
	stw %r19,4(%r5) ;# 7300 reload_outsi+2/6
	stw %r7,4(%r5) ;# 7309 reload_outsi+2/6
L$0801
	ldw 0(%r5),%r26 ;# 7316 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 7320 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 7318 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 7328 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 7324 reload_outsi+2/6
	comb,= %r28,%r4,L$0794 ;# 7338 bleu+1
	sub %r6,%r4,%r19 ;# 7340 subsi3/1
	addl %r28,%r19,%r6 ;# 7343 addsi3/1
	sub %r12,%r4,%r19 ;# 7344 subsi3/1
	comib,= 0,%r10,L$0804 ;# 7349 bleu+1
	addl %r28,%r19,%r12 ;# 7347 addsi3/1
	sub %r10,%r4,%r19 ;# 7350 subsi3/1
	addl %r28,%r19,%r10 ;# 7353 addsi3/1
L$0804
	comib,= 0,%r8,L$0805 ;# 7356 bleu+1
	sub %r8,%r4,%r19 ;# 7357 subsi3/1
	addl %r28,%r19,%r8 ;# 7360 addsi3/1
L$0805
	comib,= 0,%r9,L$0794 ;# 7363 bleu+1
	sub %r9,%r4,%r19 ;# 7364 subsi3/1
	addl %r28,%r19,%r9 ;# 7367 addsi3/1
L$0794
	ldw 0(%r5),%r4 ;# 7268 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 7272 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 7269 subsi3/1
	ldo 2(%r19),%r19 ;# 7270 addsi3/2
	comb,<< %r20,%r19,L$0796
	nop ;# 7274 bleu+1
L$0795
	ldi 7,%r19 ;# 7389 movqi+1/2
	stbs,ma %r19,1(%r6) ;# 7390 movqi+1/6
	bl L$0043,%r0 ;# 7405 jump
	stbs,ma %r3,1(%r6) ;# 7393 movqi+1/6
L$0811
	bb,< %r15,30,L$0104
	nop ;# 7414 bleu+3
L$0397
	comib,= 0,%r14,L$0815 ;# 7429 bleu+1
	addl %r14,%r7,%r19 ;# 7430 addsi3/1
	bl L$0816,%r0 ;# 7434 jump
	ldb 0(%r19),%r19 ;# 7433 zero_extendqisi2/2
L$0815
	copy %r7,%r19 ;# 7438 reload_outsi+2/1
L$0816
	copy %r19,%r7 ;# 7441 reload_outsi+2/1
L$0076
	comib,=,n 0,%r9,L$0820 ;# 7460 bleu+1
	ldb 0(%r9),%r20 ;# 7463 zero_extendqisi2/2
	addl %r9,%r20,%r19 ;# 7464 addsi3/1
	ldo 1(%r19),%r19 ;# 7465 addsi3/2
	comb,<> %r6,%r19,L$0820 ;# 7467 bleu+1
	ldi 255,%r19 ;# 7472 reload_outsi+2/2
	comb,= %r19,%r20,L$0820 ;# 7474 bleu+1
	ldw -296(%r30),%r21 ;# 7476 reload_outsi+2/5
	ldb 0(%r21),%r19 ;# 7478 movqi+1/5
	extrs %r19,31,8,%r20 ;# 7479 extendqisi2
	ldi 42,%r19 ;# 7481 reload_outsi+2/2
	comb,= %r19,%r20,L$0820 ;# 7483 bleu+1
	ldi 94,%r19 ;# 7490 reload_outsi+2/2
	comb,=,n %r19,%r20,L$0820 ;# 7492 bleu+1
	bb,>= %r15,30,L$0821 ;# 7497 movsi-4
	ldi 92,%r19 ;# 7504 reload_outsi+2/2
	comb,<>,n %r19,%r20,L$0822 ;# 7506 bleu+1
	ldb 1(%r21),%r19 ;# 7510 movqi+1/5
	extrs %r19,31,8,%r20 ;# 7511 extendqisi2
L$0821
	ldi 43,%r19 ;# 7534 reload_outsi+2/2
	comb,= %r19,%r20,L$0820 ;# 7536 bleu+1
	ldi 63,%r19 ;# 7543 reload_outsi+2/2
	comb,=,n %r19,%r20,L$0820 ;# 7545 bleu+1
L$0822
	bb,>=,n %r15,22,L$0819 ;# 7553 movsi-4
	bb,>= %r15,19,L$0823 ;# 7558 movsi-4
	ldw -296(%r30),%r19 ;# 7560 reload_outsi+2/5
	ldb 0(%r19),%r19 ;# 7562 movqi+1/5
	ldi 123,%r20 ;# 7565 reload_outsi+2/2
	extrs %r19,31,8,%r19 ;# 7563 extendqisi2
	comb,=,n %r20,%r19,L$0820 ;# 7567 bleu+1
	ldw 0(%r5),%r4 ;# 8228 reload_outsi+2/5
	bl,n L$1177,%r0 ;# 7568 jump
L$0823
	ldw -296(%r30),%r21 ;# 7572 reload_outsi+2/5
	ldb 0(%r21),%r19 ;# 7574 movqi+1/5
	ldi 92,%r20 ;# 7577 reload_outsi+2/2
	extrs %r19,31,8,%r19 ;# 7575 extendqisi2
	comb,<>,n %r20,%r19,L$0819 ;# 7579 bleu+1
	ldb 1(%r21),%r19 ;# 7583 movqi+1/5
	ldi 123,%r20 ;# 7586 reload_outsi+2/2
	extrs %r19,31,8,%r19 ;# 7584 extendqisi2
	comb,<>,n %r20,%r19,L$0819 ;# 7588 bleu+1
L$0820
	ldw 0(%r5),%r4 ;# 8220 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 8223 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8221 subsi3/1
	ldo 2(%r19),%r19 ;# 8222 addsi3/2
	comb,>>= %r20,%r19,L$0829 ;# 8224 bleu+1
	copy %r6,%r8 ;# 7596 reload_outsi+2/1
	ldil L'65536,%r3 ;# 8551 reload_outsi+2/3
L$0830
	comb,= %r3,%r20,L$0944 ;# 7624 bleu+1
	zdep %r20,30,31,%r19 ;# 7634 ashlsi3+1
	comb,>>= %r3,%r19,L$0835 ;# 7642 bleu+1
	stw %r19,4(%r5) ;# 7636 reload_outsi+2/6
	stw %r3,4(%r5) ;# 7645 reload_outsi+2/6
L$0835
	ldw 0(%r5),%r26 ;# 7652 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 7656 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 7654 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 7664 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 7660 reload_outsi+2/6
	comb,= %r28,%r4,L$0828 ;# 7674 bleu+1
	sub %r6,%r4,%r19 ;# 7676 subsi3/1
	addl %r28,%r19,%r6 ;# 7679 addsi3/1
	sub %r12,%r4,%r19 ;# 7680 subsi3/1
	comib,= 0,%r10,L$0838 ;# 7685 bleu+1
	addl %r28,%r19,%r12 ;# 7683 addsi3/1
	sub %r10,%r4,%r19 ;# 7686 subsi3/1
	addl %r28,%r19,%r10 ;# 7689 addsi3/1
L$0838
	comib,= 0,%r8,L$0839 ;# 7692 bleu+1
	sub %r8,%r4,%r19 ;# 7693 subsi3/1
	addl %r28,%r19,%r8 ;# 7696 addsi3/1
L$0839
	comib,= 0,%r9,L$0828 ;# 7699 bleu+1
	sub %r9,%r4,%r19 ;# 7700 subsi3/1
	addl %r28,%r19,%r9 ;# 7703 addsi3/1
L$0828
	ldw 0(%r5),%r4 ;# 7604 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 7608 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 7605 subsi3/1
	ldo 2(%r19),%r19 ;# 7606 addsi3/2
	comb,<< %r20,%r19,L$0830
	nop ;# 7610 bleu+1
L$0829
	ldi 1,%r19 ;# 7725 movqi+1/2
	stbs,ma %r19,1(%r6) ;# 7726 movqi+1/6
	stbs,ma %r0,1(%r6) ;# 7729 movqi+1/6
	ldo -1(%r6),%r9 ;# 7741 addsi3/2
L$0819
	ldw 0(%r5),%r4 ;# 8228 reload_outsi+2/5
L$1177
	ldw 4(%r5),%r20 ;# 8231 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 8229 subsi3/1
	ldo 1(%r19),%r19 ;# 8230 addsi3/2
	comb,>>=,n %r20,%r19,L$0848 ;# 8232 bleu+1
	ldil L'65536,%r3 ;# 8549 reload_outsi+2/3
L$0849
	comb,= %r3,%r20,L$0944 ;# 7771 bleu+1
	zdep %r20,30,31,%r19 ;# 7781 ashlsi3+1
	comb,>>= %r3,%r19,L$0854 ;# 7789 bleu+1
	stw %r19,4(%r5) ;# 7783 reload_outsi+2/6
	stw %r3,4(%r5) ;# 7792 reload_outsi+2/6
L$0854
	ldw 0(%r5),%r26 ;# 7799 reload_outsi+2/5
	.CALL ARGW0=GR,ARGW1=GR
	bl realloc,%r2 ;# 7803 call_value_internal_symref
	ldw 4(%r5),%r25 ;# 7801 reload_outsi+2/5
	comiclr,<> 0,%r28,%r0 ;# 7811 bleu+1
	bl L$0953,%r0
	stw %r28,0(%r5) ;# 7807 reload_outsi+2/6
	comb,= %r28,%r4,L$0847 ;# 7821 bleu+1
	sub %r6,%r4,%r19 ;# 7823 subsi3/1
	addl %r28,%r19,%r6 ;# 7826 addsi3/1
	sub %r12,%r4,%r19 ;# 7827 subsi3/1
	comib,= 0,%r10,L$0857 ;# 7832 bleu+1
	addl %r28,%r19,%r12 ;# 7830 addsi3/1
	sub %r10,%r4,%r19 ;# 7833 subsi3/1
	addl %r28,%r19,%r10 ;# 7836 addsi3/1
L$0857
	comib,= 0,%r8,L$0858 ;# 7839 bleu+1
	sub %r8,%r4,%r19 ;# 7840 subsi3/1
	addl %r28,%r19,%r8 ;# 7843 addsi3/1
L$0858
	comib,= 0,%r9,L$0847 ;# 7846 bleu+1
	sub %r9,%r4,%r19 ;# 7847 subsi3/1
	addl %r28,%r19,%r9 ;# 7850 addsi3/1
L$0847
	ldw 0(%r5),%r4 ;# 7751 reload_outsi+2/5
	ldw 4(%r5),%r20 ;# 7755 reload_outsi+2/5
	sub %r6,%r4,%r19 ;# 7752 subsi3/1
	ldo 1(%r19),%r19 ;# 7753 addsi3/2
	comb,<< %r20,%r19,L$0849
	nop ;# 7757 bleu+1
L$0848
	stbs,ma %r7,1(%r6) ;# 7872 movqi+1/6
	ldb 0(%r9),%r19 ;# 7885 movqi+1/5
	ldo 1(%r19),%r19 ;# 7888 addsi3/2
	stb %r19,0(%r9) ;# 7890 movqi+1/6
L$0043
	ldw -296(%r30),%r19 ;# 2328 reload_outsi+2/5
L$1161
	comclr,= %r16,%r19,%r0 ;# 258 bleu+1
	bl L$1178,%r0
	ldw -296(%r30),%r19 ;# 2334 reload_outsi+2/5
L$0044
	comib,= 0,%r10,L$0865 ;# 7913 bleu+1
	ldi 13,%r26 ;# 7918 reload_outsi+2/2
	copy %r10,%r25 ;# 7920 reload_outsi+2/1
	sub %r6,%r25,%r24 ;# 7915 subsi3/1
	.CALL ARGW0=GR,ARGW1=GR,ARGW2=GR
	bl store_op1,%r2 ;# 7924 call_internal_symref
	ldo -3(%r24),%r24 ;# 7922 addsi3/2
L$0865
	ldw -304(%r30),%r19 ;# 7928 reload_outsi+2/5
	comib,<>,n 0,%r19,L$0866 ;# 7930 bleu+1
	.CALL ARGW0=GR
	bl free,%r2 ;# 7946 call_internal_symref
	ldw -312(%r30),%r26 ;# 7944 reload_outsi+2/5
	ldw 0(%r5),%r19 ;# 7949 reload_outsi+2/5
	ldi 0,%r28 ;# 7955 reload_outsi+2/2
	sub %r6,%r19,%r19 ;# 7950 subsi3/1
	bl L$0867,%r0 ;# 7957 jump
	stw %r19,8(%r5) ;# 7952 reload_outsi+2/6
L$0895
	.CALL ARGW0=GR
	bl free,%r2 ;# 2269 call_internal_symref
	ldw -312(%r30),%r26 ;# 2267 reload_outsi+2/5
	bl L$0867,%r0 ;# 2273 jump
	ldi 11,%r28 ;# 2271 reload_outsi+2/2
L$0900
	.CALL ARGW0=GR
	bl free,%r2 ;# 3161 call_internal_symref
	ldw -312(%r30),%r26 ;# 3159 reload_outsi+2/5
	bl L$0867,%r0 ;# 3165 jump
	ldi 4,%r28 ;# 3163 reload_outsi+2/2
L$0902
	.CALL ARGW0=GR
	bl free,%r2 ;# 3218 call_internal_symref
	ldw -312(%r30),%r26 ;# 3216 reload_outsi+2/5
	bl L$0867,%r0 ;# 3222 jump
	ldi 7,%r28 ;# 3220 reload_outsi+2/2
L$0903
	.CALL ARGW0=GR
	bl free,%r2 ;# 3803 call_internal_symref
	ldw -312(%r30),%r26 ;# 3801 reload_outsi+2/5
	bl L$0867,%r0 ;# 3807 jump
	ldi 5,%r28 ;# 3805 reload_outsi+2/2
L$0915
	.CALL ARGW0=GR
	bl free,%r2 ;# 5461 call_internal_symref
	ldw -312(%r30),%r26 ;# 5459 reload_outsi+2/5
	bl L$0867,%r0 ;# 5465 jump
	ldi 9,%r28 ;# 5463 reload_outsi+2/2
L$0917
	.CALL ARGW0=GR
	bl free,%r2 ;# 5557 call_internal_symref
	ldw -312(%r30),%r26 ;# 5555 reload_outsi+2/5
	bl L$0867,%r0 ;# 5561 jump
	ldi 13,%r28 ;# 5559 reload_outsi+2/2
L$0922
	bl L$0867,%r0 ;# 5983 jump
	ldi 14,%r28 ;# 5981 reload_outsi+2/2
L$0939
	.CALL ARGW0=GR
	bl free,%r2 ;# 7235 call_internal_symref
	ldw -312(%r30),%r26 ;# 7233 reload_outsi+2/5
	bl L$0867,%r0 ;# 7239 jump
	ldi 6,%r28 ;# 7237 reload_outsi+2/2
L$0944
	bl L$0867,%r0 ;# 7775 jump
	ldi 15,%r28 ;# 7773 reload_outsi+2/2
L$0866
	.CALL ARGW0=GR
	bl free,%r2 ;# 7935 call_internal_symref
	ldw -312(%r30),%r26 ;# 7933 reload_outsi+2/5
	ldi 8,%r28 ;# 7937 reload_outsi+2/2
L$0867
	ldw -340(%r30),%r2 ;# 9026 reload_outsi+2/5
	ldw -168(%r30),%r18 ;# 9028 reload_outsi+2/5
	ldw -164(%r30),%r17 ;# 9030 reload_outsi+2/5
	ldw -160(%r30),%r16 ;# 9032 reload_outsi+2/5
	ldw -156(%r30),%r15 ;# 9034 reload_outsi+2/5
	ldw -152(%r30),%r14 ;# 9036 reload_outsi+2/5
	ldw -148(%r30),%r13 ;# 9038 reload_outsi+2/5
	ldw -144(%r30),%r12 ;# 9040 reload_outsi+2/5
	ldw -140(%r30),%r11 ;# 9042 reload_outsi+2/5
	ldw -136(%r30),%r10 ;# 9044 reload_outsi+2/5
	ldw -132(%r30),%r9 ;# 9046 reload_outsi+2/5
	ldw -128(%r30),%r8 ;# 9048 reload_outsi+2/5
	ldw -124(%r30),%r7 ;# 9050 reload_outsi+2/5
	ldw -120(%r30),%r6 ;# 9052 reload_outsi+2/5
	ldw -116(%r30),%r5 ;# 9054 reload_outsi+2/5
	ldw -112(%r30),%r4 ;# 9056 reload_outsi+2/5
	ldw -108(%r30),%r3 ;# 9058 reload_outsi+2/5
	bv %r0(%r2) ;# 9061 return_internal
	ldo -320(%r30),%r30 ;# 9060 addsi3/2
	.EXIT
	.PROCEND
	.data

	.align 1
re_syntax_table
	.block 256
