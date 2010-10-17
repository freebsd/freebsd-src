; Test that strings are evaluated as in the manual (slightly modified).
; FIXME:  This should be a generic test.  Note that this will
; fail if the parsing "\x20a" fails to recoignize this as " a".
 .text
start:
 .ascii "This\0x20is a\040string\x20with\x20a \"newline\" at the 'end'"
 .ascii "Megatroid\n", "AX-Foo\r\n"
end:
