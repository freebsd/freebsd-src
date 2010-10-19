;# Re-definition of an already .equiv-ed symbol (to another symbol).
;# The assembler should reject this.
 .equiv x, y
 .equiv y, 1
 .equiv x, 0
