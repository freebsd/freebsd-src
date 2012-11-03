<!--
	$FreeBSD$
	$FreeBSDde: de-docproj/relnotes/de_DE.ISO8859-1/share/xml/release.dsl,v 1.9 2003/05/24 22:43:52 ue Exp $
	basiert auf: 1.7
-->

<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY release.dsl PUBLIC "-//FreeBSD//DOCUMENT Release Notes DocBook Language Neutral Stylesheet//EN" CDATA DSSSL>
<!ENTITY % output.html  "IGNORE"> 
<!ENTITY % output.print "IGNORE">
]>

<style-sheet>
  <style-specification use="docbook">
    <style-specification-body>
 
      <![ %output.html; [ 
	<!-- Generate links to HTML man pages -->
	(define %refentry-xref-link% #t)

	(define ($email-footer$)
          (make sequence
	    (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"
                (literal "Diese Datei und andere Dokumente zu dieser Version sind bei ")
		(create-link (list (list "HREF" (entity-text "release.url")))
                  (literal (entity-text "release.url")))
                (literal "verfuegbar.")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "Wenn Sie Fragen zu FreeBSD haben, lesen Sie erst die ")
		(create-link
		  (list (list "HREF" "http://www.FreeBSD.org/docs.html"))
                  (literal "Dokumentation,"))
                (literal " bevor Sie sich an <")
		(create-link
		  (list (list "HREF" "mailto:de-bsd-questions@de.FreeBSD.org"))
                  (literal "de-bsd-questions@de.FreeBSD.org"))
                (literal "> wenden.")
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "Alle Anwender von FreeBSD ")
		(literal (entity-text "release.branch"))
		(literal " sollten sich in die Mailingliste ")
                (literal "<")
		(create-link (list (list "HREF" "mailto:current@FreeBSD.org"))
                  (literal "current@FreeBSD.org"))
                (literal "> eintragen.")))

            (make element gi: "p"
                  attributes: (list (list "align" "center"))
	      (literal "Wenn Sie Fragen zu dieser Dokumentation haben, wenden Sie sich an <")
	      (create-link (list (list "HREF" "mailto:de-bsd-translators@de.FreeBSD.org"))
                (literal "de-bsd-translators@de.FreeBSD.org"))
	      (literal ">."))))))
      ]]>

    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="release.dsl">
</style-sheet>
