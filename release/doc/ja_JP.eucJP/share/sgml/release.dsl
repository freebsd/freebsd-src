<!-- $FreeBSD$ -->
<!-- Original revision: 1.6.2.1 -->

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
	      (literal "この文書の原文に関するお問い合わせは <")
	      (create-link (list (list "HREF" "mailto:doc@FreeBSD.org"))
                (literal "doc@FreeBSD.org"))
	      (literal "> まで、")
              (make empty-element gi: "br")
	      (literal "日本語訳に関するお問い合わせは、<")
	      (create-link (list (list "HREF" "http://www.jp.FreeBSD.org/ml.html#doc-jp"))
                 (literal "doc-jp@jp.FreeBSD.org"))
	      (literal "> まで電子メールでお願いします。"))))))


	<!-- Convert " ... " to `` ... '' in the HTML output. -->
	(element quote
	  (make sequence
	    (literal "``")
	    (process-children)
	    (literal "''")))

        <!-- Generate links to HTML man pages -->
        (define %refentry-xref-link% #t)

        <!-- Specify how to generate the man page link HREF -->
        (define ($create-refentry-xref-link$ #!optional (n (current-node)))
          (let* ((r (select-elements (children n) (normalize "refentrytitle")))
                 (m (select-elements (children n) (normalize "manvolnum")))
                 (v (attribute-string (normalize "vendor") n))
                 (u (string-append "http://www.FreeBSD.org/cgi/man.cgi?query="
                         (data r) "&" "sektion=" (data m))))
            (case v
              (("xfree86") (string-append u "&" "manpath=XFree86+4.2.0"))
              (("netbsd")  (string-append u "&" "manpath=NetBSD+1.5"))
              (("ports")   (string-append u "&" "manpath=FreeBSD+Ports"))
              (else        (string-append u "&" "manpath=FreeBSD+5.0-RELEASE")))))
      ]]>

      (define (toc-depth nd)
        (if (string=? (gi nd) (normalize "book"))
            3
            3))

    </style-specification-body>
  </style-specification>

  <external-specification id="docbook" document="release.dsl">
</style-sheet>
