;# Re-definition of an already .equiv-ed symbol (to an expression).
;# The assembler should reject this.
 .equiv x, y-z
 .equiv y, 1
 .equiv z, 1
 .equiv x, 1
