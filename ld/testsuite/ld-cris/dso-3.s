; The visibility definitions here override the default
; definitions in the object where the symbols are defined.  We
; check STV_HIDDEN and STV_PROTECTED with function and object
; definition respectively.  This is by no means a full coverage,
; just enough to be a test-case for the bug described in
; libdso-3.d.  Use ld-elfvsb for general visibility tests.

 .hidden expobj
 .protected expfn

 .text
 .global globsym
 .type	globsym,@function
globsym:
 move.d	expfn:GOTOFF,$r3
 move.d	expfn:PLTG,$r3
 move.d	expfn:PLT,$r3
 move.d	expobj:GOTOFF,$r3
.Lfe1:
 .size	globsym,.Lfe1-globsym
