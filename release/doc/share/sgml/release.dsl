<!-- $FreeBSD$ -->

<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY freebsd.dsl PUBLIC "-//FreeBSD//DOCUMENT DocBook Stylesheet//EN" CDATA DSSSL>
]>

<style-sheet>
  <style-specification use="docbook">
    <style-specification-body>

; The architecture we're building for.  We need to define this as a
; procedure, because we may not be able to evaluate it until we are
; at a point in formatting where (current-node) is defined.

(default
  (let* ((arch (attribute-string (normalize "arch")))
	 (for-arch (entity-text "arch")))
    (if (or (equal? arch #f)
	    (equal? arch ""))
	(next-match)
; We can do a lot more flexible things here.  Like it'd be nice to
; tokenize the arch= attribute and do comparisons of for-arch against
; different substrings.
	(cond ((equal? arch for-arch) (next-match))
	      (else (empty-sosofo))))))

; We might have some sect1 level elements where the modification times
; are significant.  An example of this is the "What's New" section in
; the release notes.  We enable the printing of pubdate entry in
; sect1info elements to support this.
(element (sect1info pubdate) (process-children))

    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="freebsd.dsl">
</style-sheet>

