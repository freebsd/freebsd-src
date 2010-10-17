#objdump: -dr
#name: flags: clearf, setf and nop

.*:     file format .*-cris

Disassembly of section \.text:
0+ <start>:
[	 ]+0:[	 ]+0f05[ 	]+nop[ ]*
[	 ]+2:[	 ]+b015[ 	]+ax[ ]*
[	 ]+4:[	 ]+bff5[ 	]+setf[ ]+cvznxi[be][dm]
[	 ]+6:[	 ]+fff5[ 	]+clearf[ ]+cvznxi[be][dm]
[	 ]+8:[	 ]+b025[ 	]+ei[ ]*
[	 ]+a:[	 ]+f025[ 	]+di[ ]*
[	 ]+c:[	 ]+b995[ 	]+setf[ ]+cnx[dm]
[	 ]+e:[	 ]+f995[ 	]+clearf[ ]+cnx[dm]
[	 ]+10:[	 ]+b005[ 	]+setf[ ]*
[	 ]+12:[	 ]+f005[ 	]+clearf[ ]*
[	 ]+14:[	 ]+b105[ 	]+setf[ ]+c
[	 ]+16:[	 ]+f105[ 	]+clearf[ ]+c
[	 ]+18:[	 ]+b205[ 	]+setf[ ]+v
[	 ]+1a:[	 ]+f205[ 	]+clearf[ ]+v
[	 ]+1c:[	 ]+b405[ 	]+setf[ ]+z
[	 ]+1e:[	 ]+f405[ 	]+clearf[ ]+z
[	 ]+20:[	 ]+b805[ 	]+setf[ ]+n
[	 ]+22:[	 ]+f805[ 	]+clearf[ ]+n
[	 ]+24:[	 ]+b015[ 	]+ax[ ]*
[	 ]+26:[	 ]+f015[ 	]+clearf[ ]+x
[	 ]+28:[	 ]+b025[ 	]+ei[ ]*
[	 ]+2a:[	 ]+f025[ 	]+di[ ]*
[	 ]+2c:[	 ]+b045[ 	]+setf[ ]+[be]
[	 ]+2e:[	 ]+f045[ 	]+clearf[ ]+[be]
[	 ]+30:[	 ]+b085[ 	]+setf[ ]+[dm]
[	 ]+32:[	 ]+f085[ 	]+clearf[ ]+[dm]
[	 ]+34:[	 ]+f305[ 	]+clearf[ ]+cv
[	 ]+36:[	 ]+b305[ 	]+setf[ ]+cv
[	 ]+38:[	 ]+f035[ 	]+clearf[ ]+xi
[	 ]+3a:[	 ]+b035[ 	]+setf[ ]+xi
[	 ]+3c:[	 ]+f305[ 	]+clearf[ ]+cv
[	 ]+3e:[	 ]+b305[ 	]+setf[ ]+cv
[	 ]+40:[	 ]+f035[ 	]+clearf[ ]+xi
[	 ]+42:[	 ]+b035[ 	]+setf[ ]+xi
[	 ]+44:[	 ]+f825[ 	]+clearf[ ]+ni
[	 ]+46:[	 ]+b825[ 	]+setf[ ]+ni
[	 ]+48:[	 ]+f825[ 	]+clearf[ ]+ni
[	 ]+4a:[	 ]+b825[ 	]+setf[ ]+ni
[	 ]+4c:[	 ]+fb15[ 	]+clearf[ ]+cvnx
[	 ]+4e:[	 ]+bb15[ 	]+setf[ ]+cvnx
[	 ]+50:[	 ]+fb15[ 	]+clearf[ ]+cvnx
[	 ]+52:[	 ]+bb15[ 	]+setf[ ]+cvnx
[	 ]+54:[	 ]+f0f5[ 	]+clearf[ ]+xi[be][dm]
[	 ]+56:[	 ]+b0f5[ 	]+setf[ ]+xi[be][dm]
[	 ]+58:[	 ]+f0f5[ 	]+clearf[ ]+xi[be][dm]
[	 ]+5a:[	 ]+b0f5[ 	]+setf[ ]+xi[be][dm]
[	 ]+5c:[	 ]+fa55[ 	]+clearf[ ]+vnx[be]
[	 ]+5e:[	 ]+ba55[ 	]+setf[ ]+vnx[be]
[	 ]+60:[	 ]+fa55[ 	]+clearf[ ]+vnx[be]
[	 ]+62:[	 ]+ba55[ 	]+setf[ ]+vnx[be]
[	 ]+64:[	 ]+bff5[ 	]+setf[ ]+cvznxi[be][dm]
[	 ]+66:[	 ]+fff5[ 	]+clearf[ ]+cvznxi[be][dm]
[	 ]+68:[	 ]+b045[ 	]+setf[ ]+[be]
[	 ]+6a:[	 ]+f045[ 	]+clearf[ ]+[be]
[	 ]+6c:[	 ]+b085[ 	]+setf[ ]+[dm]
[	 ]+6e:[	 ]+f085[ 	]+clearf[ ]+[dm]
[	 ]+70:[	 ]+f0f5[ 	]+clearf[ ]+xi[be][dm]
[	 ]+72:[	 ]+b0f5[ 	]+setf[ ]+xi[be][dm]
[	 ]+74:[	 ]+f0f5[ 	]+clearf[ ]+xi[be][dm]
[	 ]+76:[	 ]+b0f5[ 	]+setf[ ]+xi[be][dm]
[	 ]+78:[	 ]+fa55[ 	]+clearf[ ]+vnx[be]
[	 ]+7a:[	 ]+ba55[ 	]+setf[ ]+vnx[be]
[	 ]+7c:[	 ]+fa55[ 	]+clearf[ ]+vnx[be]
[	 ]+7e:[	 ]+ba55[ 	]+setf[ ]+vnx[be]
