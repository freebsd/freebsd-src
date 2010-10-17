# GAS should print a warning  when an odd register is used as a target 
# of multi-word instructions: ld2w, ld4bh, ld4bhu, ld2h, st2w, st4hb, st2h, 
# and mulx2h

st2w r1, @(r0, 0)	||	nop 
ld2w r1, @(r0, 0)	||	nop 
ld4bh r1, @(r0, 0)	||	nop 
ld4bhu r1, @(r0, 0)	||	nop 
ld2h r1, @(r0, 0)	||	nop 
st4hb r1, @(r0, 0)	||	nop 
st2h r1, @(r0, 0)	||	nop 
nop	||	mulx2h r1, r5, r6   
