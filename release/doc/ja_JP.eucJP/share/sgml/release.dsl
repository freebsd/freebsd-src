<!-- $FreeBSD$ -->
<!-- Original revision: 1.1.2.7 -->

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
                (literal "このファイルの他、リリース関連の文書は ")
		(create-link (list (list "HREF" (entity-text "release.url")))
                  (literal (entity-text "release.url")))
                (literal " からダウンロードできます。")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "FreeBSD に関するお問い合わせは、<")
		(create-link
                  (list (list "HREF" "mailto:questions@FreeBSD.org"))
                  (literal "questions@FreeBSD.org"))
                (literal "> へ質問を投稿する前に")
		(create-link
		  (list (list "HREF" "http://www.FreeBSD.org/docs.html"))
                  (literal "解説文書"))
                (literal "をお読みください。")
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
              (make element gi: "small"  
                (literal "FreeBSD ")
		(literal (entity-text "release.branch"))
		(literal " をお使いの方は、ぜひ ")
                (literal "<")
		(create-link (list (list "HREF" "mailto:stable@FreeBSD.org"))
                  (literal "stable@FreeBSD.org"))
                (literal "> メーリングリストに参加ください。")))
            (make element gi: "p"
                  attributes: (list (list "align" "center"))
                (make element gi: "small"
	          (literal "この文書の原文に関するお問い合わせは <")
                  (create-link (list (list "HREF" "mailto:doc@FreeBSD.org"))
                               (literal "doc@FreeBSD.org"))
                  (literal "> まで、")
                  (make empty-element gi: "br")
                  (literal "日本語訳に関するお問い合わせは <")
                  (create-link (list (list "HREF" "http://www.jp.FreeBSD.org/ml.html#doc-jp"))
                               (literal "doc-jp@jp.FreeBSD.org"))
                  (literal "> まで電子メールでお願いします。")))))))
      ]]>
    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="release.dsl">
</style-sheet>
