<!-- $FreeBSD$ -->

<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY % output.html		"IGNORE">
<!ENTITY % output.print 	"IGNORE">
<!ENTITY % include.historic	"IGNORE">
<!ENTITY % no.include.historic	"IGNORE">
<!ENTITY freebsd.dsl PUBLIC "-//FreeBSD//DOCUMENT DocBook Stylesheet//EN" CDATA DSSSL>
<!ENTITY % release.ent PUBLIC "-//FreeBSD//ENTITIES Release Specification//EN">
%release.ent;
]>

<style-sheet>
  <style-specification use="docbook">
    <style-specification-body>

; Configure behavior of this stylesheet
<![ %include.historic; [
      (define %include-historic% #t)
]]>
<![ %no.include.historic; [
      (define %include-historic% #f)
]]>

; String manipulation functions
(define (split-string-to-list STR)
  ;; return list of STR separated with char #\ or #\,
  (if (string? STR)
      (let loop ((i (string-delim-index STR)))
        (cond ((equal? (cdr i) '()) '())
              (else (cons (substring STR (list-ref i 0) (- (list-ref i 1) 1))
                          (loop (cdr i))))))
      '()))

(define (string-delim-index STR)
  ;; return indexes of STR separated with char #\ or #\,
  (if (string? STR)
      (let ((strlen (string-length STR)))
        (let loop ((i 0))
          (cond ((= i strlen) (cons (+ strlen 1) '()))
                ((= i 0)      (cons i (loop (+ i 1))))
                ((or (equal? (string-ref STR i) #\ )
                     (equal? (string-ref STR i) #\,)) (cons (+ i 1) (loop (+ i 1))))
                (else (loop (+ i 1))))))
      '()
      ))

(define (string-list-match? STR STR-LIST)
  (let loop ((s STR-LIST))
    (cond
     ((equal? s #f) #f)
     ((equal? s '()) #f)
     ((equal? (car s) #f) #f)
     ((equal? STR (car s)) #t)
     (else (loop (cdr s))))))

; Deal with conditional inclusion of text via entities.
(default
  (let* ((arch (attribute-string (normalize "arch")))
	 (role (attribute-string (normalize "role")))
	 (for-arch (entity-text "arch")))
    (cond

     ; If role=historic, and we're not printing historic things, then
     ; don't output this element.
     ((and (equal? role "historic")
	   (not %include-historic%))
      (empty-sosofo))
      

     ; If arch= not specified, then print unconditionally.  This clause
     ; handles the majority of cases.
     ((or (equal? arch #f) (equal? arch ""))
      (next-match))

     ; arch= specified, see if it's equal to "all".  If so, then
     ; print unconditionally.  Note that this clause could be
     ; combined with the check to see if arch= wasn't specified
     ; or was empty; they have the same outcome.
     ((equal? arch "all")
      (next-match))

     ; arch= specified.  If we're building for all architectures,
     ; then print it prepended with the set of architectures to which
     ; this element applies.
     ;
     ; XXX This doesn't work.
;     ((equal? for-arch "all")
;      (sosofo-append (literal "[") (literal arch) (literal "] ")
;		     (process-children)))

     ; arch= specified, so we need to check to see if the specified
     ; parameter includes the architecture we're building for.
     ((string-list-match? for-arch (split-string-to-list arch))
      (next-match))

     ; None of the above
     (else (empty-sosofo)))))

; We might have some sect1 level elements where the modification times
; are significant.  An example of this is the "What's New" section in
; the release notes.  We enable the printing of pubdate entry in
; sect1info elements to support this.
(element (sect1info pubdate) (process-children))

    <![ %output.print; [
; Put URLs in footnotes, and put footnotes at the bottom of each page.
      (define bop-footnotes #t)
      (define %footnote-ulinks% #t)

; disable (make page-footnote) in tgroup element temporarily.
      (element ulink 
	(make sequence
	  (if (node-list-empty? (children (current-node)))
	      (literal (attribute-string (normalize "url")))
	      (make sequence
		($charseq$)
		(if %footnote-ulinks%
		    (if (node-list-empty?
			 (ancestor-member (current-node)
					  (list (normalize "tgroup"))))
			(next-match)
			(make sequence
			  (empty-sosofo)))
		    (empty-sosofo))))))
    ]]>

    <![ %output.html; [
      (define %callout-graphics%
	;; Use graphics in callouts?
	#f)

	<!-- Convert " ... " to `` ... '' in the HTML output. -->
	(element quote
	  (make sequence
	    (literal "``")
	    (process-children)
	    (literal "''")))

        <!-- Specify how to generate the man page link HREF -->
        (define ($create-refentry-xref-link$ #!optional (n (current-node)))
          (let* ((r (select-elements (children n) (normalize "refentrytitle")))
                 (m (select-elements (children n) (normalize "manvolnum")))
                 (v (attribute-string (normalize "vendor") n))
                 (u (string-append "&release.man.url;?query="
                         (data r) "&" "sektion=" (data m))))
            (case v
              (("xfree86") (string-append u "&" "manpath=XFree86+&release.manpath.xfree86;" ))
              (("netbsd")  (string-append u "&" "manpath=NetBSD+&release.manpath.netbsd;"))
              (("ports")   (string-append u "&" "manpath=FreeBSD+&release.manpath.freebsd-ports;"))
              (else        (string-append u "&" "manpath=FreeBSD+&release.manpath.freebsd;")))))
    ]]>

      (define (toc-depth nd)
        (if (string=? (gi nd) (normalize "book"))
            3
            3))

    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="freebsd.dsl">
</style-sheet>
