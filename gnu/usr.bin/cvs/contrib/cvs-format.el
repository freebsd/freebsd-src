;; -*- lisp-interaction -*-
;; -*- emacs-lisp -*-
;;
;;
;; originally from...
;;	Rich's personal .emacs file.  feel free to copy.
;;
;; Last Mod Wed Feb 5 16:11:47 PST 1992, by rich@cygnus.com
;;

;;
;;
;;	This section sets constants used by c-mode for formating
;;
;;

;;  If `c-auto-newline' is non-`nil', newlines are inserted both
;;before and after braces that you insert, and after colons and semicolons.
;;Correct C indentation is done on all the lines that are made this way.

(setq c-auto-newline nil)


;;*Non-nil means TAB in C mode should always reindent the current line,
;;regardless of where in the line point is when the TAB command is used.
;;It might be desirable to set this to nil for CVS, since unlike GNU
;; CVS often uses comments over to the right separated by TABs.
;; Depends some on whether you're in the habit of using TAB to
;; reindent.
;(setq c-tab-always-indent nil)

;;; It seems to me that 
;;;    `M-x set-c-style BSD RET'
;;; or
;;;    (set-c-style "BSD")
;;; takes care of the indentation parameters correctly.


;;  C does not have anything analogous to particular function names for which
;;special forms of indentation are desirable.  However, it has a different
;;need for customization facilities: many different styles of C indentation
;;are in common use.
;;
;;  There are six variables you can set to control the style that Emacs C
;;mode will use.
;;
;;`c-indent-level'     
;;     Indentation of C statements within surrounding block.  The surrounding
;;     block's indentation is the indentation of the line on which the
;;     open-brace appears.

(setq c-indent-level 4)

;;`c-continued-statement-offset'     
;;     Extra indentation given to a substatement, such as the then-clause of
;;     an if or body of a while.

(setq c-continued-statement-offset 4)

;;`c-brace-offset'     
;;     Extra indentation for line if it starts with an open brace.

(setq c-brace-offset -4)

;;`c-brace-imaginary-offset'     
;;     An open brace following other text is treated as if it were this far
;;     to the right of the start of its line.

(setq c-brace-imaginary-offset 0)

;;`c-argdecl-indent'     
;;     Indentation level of declarations of C function arguments.

(setq c-argdecl-indent 4)

;;`c-label-offset'     
;;     Extra indentation for line that is a label, or case or default.

(setq c-label-offset -4)

;;;; eof
