;;; -*- mode: emacs-lisp; indent-tabs-mode: nil -*-

;; emacs-23.x has a bug in cc-mode that that incorrectly deals with
;; case labels with character constants.

(require 'cl)
(require 'cc-defs)
(require 'cc-vars)
(require 'cc-langs)

;; Hack load-in-progress to silence the c-lang-defconst error.  For
;; some reason, load-in-progress is nil at some times when it
;; shouldn't be, at least on released emacs-23.1.1.
(let ((load-in-progress t))

  ;; Updated c-nonlabel-token-key based on cc-langs.el 5.267.2.22, to
  ;; allow character constants in case labels.
  (c-lang-defconst c-nonlabel-token-key
    "Regexp matching things that can't occur in generic colon labels,
neither in a statement nor in a declaration context.  The regexp is
tested at the beginning of every sexp in a suspected label,
i.e. before \":\".  Only used if `c-recognize-colon-labels' is set."
    t (concat
       ;; Don't allow string literals.
       "\"\\|"
       ;; All keywords except `c-label-kwds' and `c-protection-kwds'.
       (c-make-keywords-re t
         (set-difference (c-lang-const c-keywords)
                         (append (c-lang-const c-label-kwds)
                                 (c-lang-const c-protection-kwds))
                         :test 'string-equal)))
    ;; Also check for open parens in C++, to catch member init lists in
    ;; constructors.  We normally allow it so that macros with arguments
    ;; work in labels.
    c++ (concat "\\s\(\\|" (c-lang-const c-nonlabel-token-key)))
  (c-lang-defvar c-nonlabel-token-key (c-lang-const c-nonlabel-token-key))

  ;; Monkey-patch by way of c-mode-common-hook, as the byte-compiled
  ;; version of c-init-language-vars will have the old value.  This
  ;; avoids finding some way to re-evaluate the defun for
  ;; c-init-language-vars.
  (defun krb5-c-monkey-patch-caselabel ()
    (setq c-nonlabel-token-key (c-lang-const c-nonlabel-token-key)))
  (add-hook 'c-mode-common-hook 'krb5-c-monkey-patch-caselabel))
