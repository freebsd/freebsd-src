; Check that some ISO 8859-1 "scandinavian" letters are accepted in
; comments (such that they do not break lines).  This is highly dependent
; on the host C library.
 .text
start:
 nop ; borkåborkäborköborkÅborkÄborkÖbork.
