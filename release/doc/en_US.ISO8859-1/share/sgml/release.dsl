<!-- $FreeBSD$ -->

<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY release.dsl PUBLIC "-//FreeBSD//DOCUMENT Release Notes DocBook Language Neutral Stylesheet//EN" CDATA DSSSL>
<!ENTITY % output.html  "IGNORE"> 
<!ENTITY % output.print "IGNORE">
]>

<style-sheet>
  <style-specification use="docbook">
    <style-specification-body>
 
      <![ %output.html; [ 
	(define ($email-footer$)
          (make sequence
	    (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"
                (literal "This file, and other release-related documents, can be downloaded from ")
	        (make element gi: "a"
                      attributes: (list (list "href" (entity-text "release.url")))
                  (literal (entity-text "release.url")))
                (literal ".")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "For questions about FreeBSD, read the ")
                (make element gi: "a"
                      attributes: (list (list "href" "http://www.freebsd.org/docs.html"))
                  (literal "documentation"))
                (literal " before contacting <")
                (make element gi: "a"
                      attributes: (list (list "href" "mailto:freebsd-questions@FreeBSD.org"))
                  (literal "freebsd-questions@FreeBSD.org"))
                (literal ">.")
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "All users of FreeBSD ")
		(literal (entity-text "release.branch"))
		(literal " should subscribe to the ")
                (literal "<")
                (make element gi: "a"
                      attributes: (list (list "href" "mailto:freebsd-stable@FreeBSD.org"))
                  (literal "freebsd-stable@FreeBSD.org"))
                (literal "> mailing list.")))

            (make element gi: "p"
                  attributes: (list (list "align" "center"))
	      (literal "For questions about this documentation, e-mail <")
              (make element gi: "a"
                    attributes: (list (list "href" "mailto:freebsd-doc@FreeBSD.org"))
                (literal "freebsd-doc@FreeBSD.org"))
	      (literal ">."))))))

	<!-- Convert " ... " to `` ... '' in the HTML output. -->
	(element quote
	  (make sequence
	    (literal "``")
	    (process-children)
	    (literal "''")))

        <!-- Generate links to HTML man pages -->
        (define %refentry-xref-link% #t)

        <!-- Specify how to generate the man page link HREF -->
        (define ($create-refentry-xref-link$ refentrytitle manvolnum)
	  (string-append "http://www.FreeBSD.org/cgi/man.cgi?query="
			 refentrytitle "&" "sektion=" manvolnum))
      ]]>
    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="release.dsl">
</style-sheet>
