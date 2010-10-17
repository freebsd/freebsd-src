	.text
	.global foo
foo:	
        mvfc r0,cr0
        mvtc r0,psw	;; cr0=psw
        mvfc r0,cr1
        mvtc r0,bpsw 	;; cr1=bpsw
        mvfc r0,cr2
        mvtc r0,pc	;; cr2=pc
        mvfc r0,cr3; 
        mvtc r0,bpc	;; cr3=bpc
        mvfc r0,cr7
        mvtc r0,rpt_c	;; cr7=rpt_c
        mvfc r0,cr8
        mvtc r0,rpt_s	;; cr8=rpt_s
        mvfc r0,cr9
        mvtc r0,rpt_e	;; cr9=rpt_e
        mvfc r0,cr10
        mvtc r0,mod_s	;; cr10=mod_s
        mvfc r0,cr11
        mvtc r0,mod_e	;; cr11=mod_e
        mvfc r0,cr14
        mvtc r0,iba	;; cr14=iba
	
	
