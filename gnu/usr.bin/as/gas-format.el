;; -*- lisp-interaction -*-
;; -*- emacs-lisp -*-
;;
;;
;; originally from...
;;	Rich's personal .emacs file.  feel free to copy.
;;
;; this file sets emacs up for the type of C source code formatting used within
;; gas.  I don't use gnu indent.  If you do, and find a setup that approximates
;; these settings, please send it to me.
;;
;; Last Mod Thu Feb 13 00:59:16 PST 1992, by rich@sendai
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


;;  If `c-tab-always-indent' is non-`nil', the TAB command
;;in C mode does indentation only if point is at the left margin or within
;;the line's indentation.  If there is non-whitespace to the left of point,
;;then TAB just inserts a tab character in the buffer.  Normally,
;;this variable is `nil', and TAB always reindents the current line.

(setq c-tab-always-indent nil)

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

(setq c-indent-level 8)

;;`c-continued-statement-offset'     
;;     Extra indentation given to a substatement, such as the then-clause of
;;     an if or body of a while.

(setq c-continued-statement-offset 4)

;;`c-brace-offset'     
;;     Extra indentation for line if it starts with an open brace.

(setq c-brace-offset 0)

;;`c-brace-imaginary-offset'     
;;     An open brace following other text is treated as if it were this far
;;     to the right of the start of its line.

(setq c-brace-imaginary-offset 0)

;;`c-argdecl-indent'     
;;     Indentation level of declarations of C function arguments.

(setq c-argdecl-indent 0)

;;`c-label-offset'     
;;     Extra indentation for line that is a label, or case or default.

(setq c-label-offset -8)

;; end of gas-format.el
