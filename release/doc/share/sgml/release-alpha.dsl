<!-- $FreeBSD$ -->

<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY freebsd.dsl PUBLIC "-//FreeBSD//DOCUMENT DocBook Stylesheet//EN" CDATA DSSSL>
]>

<style-sheet>
  <style-specification use="docbook">
    <style-specification-body>

; The architecture we're building for.  We really need to pull this out of a 
; entity somewhere, so it can be defined without needing to munge the
; stylesheet.  Or do we?
(define for-arch "alpha")

(default
  (let* ((arch (attribute-string (normalize "arch"))))
    (if (or (equal? arch #f)
	    (equal? arch ""))
	(next-match)
; We can do a lot more flexible things here.  Like it'd be nice to
; tokenize the arch= attribute and do comparisons of for-arch against
; different substrings.
	(cond ((equal? arch for-arch) (next-match))
	      (else (empty-sosofo))))))

    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="freebsd.dsl">
</style-sheet>

