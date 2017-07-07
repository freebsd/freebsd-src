;;; -*- mode: emacs-lisp; indent-tabs-mode: nil -*-
(defconst krb5-c-style
  '("bsd"
    (c-basic-offset     . 4)
    (c-cleanup-list     . (brace-elseif-brace
                           brace-else-brace
                           defun-close-semi))
    (c-comment-continuation-stars       . "* ")
    (c-comment-only-line-offset . 0)
    (c-electric-pound-behavior  . (alignleft))
    (c-hanging-braces-alist     . ((block-close . c-snug-do-while)
                                   (brace-list-open)
                                   (class-open after)
                                   (extern-lang-open after)
                                   (substatement-open after)))
    (c-hanging-colons-alist     . ((case-label after)
                                   (label after)))
    (c-hanging-comment-starter-p        . nil)
    (c-hanging-comment-ender-p          . nil)
    (c-indent-comments-syntactically-p  . t)
    (c-label-minimum-indentation        . 0)
    (c-offsets-alist    . ((inextern-lang . 0)
                           (arglist-close . 0)))
    (c-special-indent-hook      . nil)
    (fill-column                . 79)))

;; Use eval-after-load rather than c-initialization-hook; this ensures
;; that the style gets defined even if a user loads this file after
;; initializing cc-mode.
(eval-after-load 'cc-mode (c-add-style "krb5" krb5-c-style))

;; We don't use a c-file-style file-local variable setting in our
;; source code, to avoid errors for emacs users who don't define the
;; "krb5" style.  Instead, use this heuristic.
;;
;; TODO: modify to also look for unique files in the source tree.
(defun krb5-c-mode-hook ()
  (if (and (eq major-mode 'c-mode)
           (eq c-basic-offset 4)
           (eq indent-tabs-mode nil))
      (c-set-style "krb5")))

;; (add-hook 'c-mode-common-hook 'krb5-c-mode-hook)

;; Use hack-local-variables-hook because the c-mode hooks run before
;; hack-local-variables runs.
(add-hook 'hack-local-variables-hook 'krb5-c-mode-hook)

;; emacs-23.x has a buggy cc-mode that incorrectly deals with case
;; labels with character constants.
(if (and (string-match "^23\\." emacs-version)
         (require 'cc-defs)
         (string-match "5\\.31\\.[0-7]" c-version))
    (let ((load-path (cons (file-name-directory load-file-name) load-path)))
      (load "krb5-hack-cc-mode-caselabel")))
