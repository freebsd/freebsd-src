;;; cookie.el,v 1.2 1992/04/07 20:49:12 berliner Exp
;;; cookie.el -- Utility to display cookies in buffers
;;; Copyright (C) 1991, 1992  Per Cederqvist
;;;
;;; This program is free software; you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 2 of the License, or
;;; (at your option) any later version.
;;;
;;; This program is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with this program; if not, write to the Free Software
;;; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

;;;; TO-DO: Byt namn! tin -> wrapper (eller n}got b{ttre).

;;; Note that this file is still under development.  Comments,
;;; enhancements and bug fixes are welcome.
;;; Send them to ceder@lysator.liu.se.

(defun impl nil (error "Not yet implemented!"))

;;; Cookie is a package that imlements a connection between an
;;; elib-dll and the contents of a buffer. Possible uses are dired
;;; (have all files in a list, and show them), buffer-list,
;;; kom-prioritize (in the LysKOM elisp client) and others. pcl-cvs.el
;;; uses cookie.el.
;;;
;;; A cookie buffer contains a header, any number of cookies, and a
;;; footer. The header and footer are constant strings that are given
;;; to cookie-create when the buffer is placed under cookie. Each cookie
;;; is displayed in the buffer by calling a user-supplied function
;;; that takes a cookie and returns a string. The string may be
;;; empty, or contain any number of lines. An extra newline is always
;;; appended unless the string is empty.
;;;
;;; Cookie does not affect the mode of the buffer in any way. It
;;; merely makes it easy to connect an underlying data representation
;;; to the buffer contents.
;;;
;;; The cookie-node data type:
;;;      start-marker
;;;      ;; end-marker      This field is no longer present.
;;;      cookie          The user-supplied element.
;;;
;;; A dll of cookie-nodes are held in the buffer local variable
;;; cake-tin.
;;;
;;; A tin is an object that contains one cookie. You can get the next
;;; and previous tin.
;;;

(require 'elib-dll)
(provide 'cookie)

(defvar cookies nil
  "A doubly linked list that contains the underlying data representation
for the contents of a cookie buffer. The package elib-dll is used to
manipulate this list.")

(defvar cookie-pretty-printer nil
  "The function that is used to pretty-print a cookie in this buffer.")

(defvar cookie-header nil
  "The tin that holds the header cookie.")

(defvar cookie-footer nil
  "The tin that holds the footer cookie.")

(defvar cookie-last-tin nil
  "The tin the cursor was positioned at, the last time the cookie
package checked the cursor position. Buffer local in all buffers
the cookie package works on. You may set this if your package
thinks it knows where the cursor will be the next time this
package is called. It can speed things up.

It must never be set to a tin that has been deleted.")

;;; ================================================================
;;;      Internal functions for use in the cookie package

(put 'cookie-set-buffer 'lisp-indent-hook 1)

(defmacro cookie-set-buffer (buffer &rest forms)

  ;; Execute FORMS with BUFFER selected as current buffer.
  ;; Return value of last form in FORMS.  INTERNAL USE ONLY.

  (let ((old-buffer (make-symbol "old-buffer")))
    (` (let (((, old-buffer) (current-buffer)))
	 (set-buffer (get-buffer-create (, buffer)))
	 (unwind-protect
	     (progn (,@ forms))
	   (set-buffer (, old-buffer)))))))


(defmacro cookie-filter-hf (tin)

  ;; Evaluate TIN once and return it. BUT if it is
  ;; equal to cookie-header or cookie-footer return nil instead.
  ;; INTERNAL USE ONLY.

  (let ((tempvar (make-symbol "tin")))
    (` (let (((, tempvar) (, tin)))
	 (if (or (eq (, tempvar) cookie-header)
		 (eq (, tempvar) cookie-footer))
	     nil
	   (, tempvar))))))


;;; cookie-tin
;;; Constructor:

(defun cookie-create-tin (start-marker
			  cookie)
  ;; Create a  tin.   INTERNAL USE ONLY.
  (cons 'COOKIE-TIN (vector start-marker nil cookie)))


;;; Selectors:

(defun cookie-tin-start-marker (cookie-tin)
  ;; Get start-marker from cookie-tin.    INTERNAL USE ONLY.
  (elt (cdr cookie-tin) 0))

;(defun cookie-tin-end-marker (cookie-tin)
;  ;;Get end-marker from cookie-tin.   INTERNAL USE ONLY.
;  (elt (cdr cookie-tin) 1))

(defun cookie-tin-cookie-safe (cookie-tin)
  ;; Get cookie from cookie-tin.   INTERNAL USE ONLY.
  ;; Returns nil if given nil as input.
  ;; This is the same as cookie-tin-cookie in version 18.57
  ;; of emacs, but elt should signal an error when given nil
  ;; as input (according to the info files).
  (elt (cdr cookie-tin) 2))

(defun cookie-tin-cookie (cookie-tin)
  ;; Get cookie from cookie-tin.   INTERNAL USE ONLY.
  (elt (cdr cookie-tin) 2))


;;; Modifiers:

(defun set-cookie-tin-start-marker (cookie-tin newval)
  ;; Set start-marker in cookie-tin to NEWVAL.   INTERNAL USE ONLY.
  (aset (cdr cookie-tin) 0 newval))

;(defun set-cookie-tin-end-marker (cookie-tin newval)
;  ;; Set end-marker in cookie-tin to NEWVAL.   INTERNAL USE ONLY.
;  (aset (cdr cookie-tin) 1 newval))

(defun set-cookie-tin-cookie (cookie-tin newval)
  ;; Set cookie in cookie-tin to NEWVAL.   INTERNAL USE ONLY.
  (aset (cdr cookie-tin) 2 newval))



;;; Predicate:

(defun cookie-tin-p (object)
  ;; Return t if OBJECT is a tin.   INTERNAL USE ONLY.
  (eq (car-safe object) 'COOKIE-TIN))

;;; end of cookie-tin data type.
				 

(defun cookie-create-tin-and-insert (cookie string pos)
  ;; Insert STRING at POS in current buffer. Remember start
  ;; position. Create a tin containing them and the COOKIE.
  ;;    INTERNAL USE ONLY.

  (save-excursion
    (goto-char pos)
    ;; Remember the position as a number so that it doesn't move
    ;; when we insert the string.
    (let ((start (if (markerp pos)
		     (marker-position pos)
		   pos)))
      ;; Use insert-before-markers so that the marker for the
      ;; next cookie is updated.
      (insert-before-markers string)
      (insert-before-markers ?\n)
      (cookie-create-tin (copy-marker start) cookie))))


(defun cookie-delete-tin-internal (tin)
  ;; Delete a cookie from the buffer.  INTERNAL USE ONLY.
  ;; Can not be used on the footer.
  (delete-region (cookie-tin-start-marker (dll-element cookies tin))
		 (cookie-tin-start-marker
		  (dll-element cookies
			       (dll-next cookies  tin)))))



(defun cookie-refresh-tin (tin)
  ;; Redisplay the cookie represented by TIN. INTERNAL USE ONLY.
  ;; Can not be used on the footer.

  (save-excursion
    ;; First, remove the string:
    (delete-region (cookie-tin-start-marker (dll-element cookies tin))
		   (1- (marker-position
			(cookie-tin-start-marker
			 (dll-element cookies
				      (dll-next cookies  tin))))))

    ;; Calculate and insert the string.

    (goto-char (cookie-tin-start-marker (dll-element cookies tin)))
    (insert
     (funcall cookie-pretty-printer
	      (cookie-tin-cookie (dll-element cookies tin))))))


;;; ================================================================
;;;      The public members of the cookie package


(defun cookie-cookie (buffer tin)
  "Get the cookie from a TIN. Args: BUFFER TIN."
  (cookie-set-buffer buffer
    (cookie-tin-cookie (dll-element cookies tin))))




(defun cookie-create (buffer pretty-printer &optional header footer)

  "Start to use the cookie package in BUFFER.
BUFFER may be a buffer or a buffer name. It is created if it does not exist.
Beware that the entire contents of the buffer will be erased.
PRETTY-PRINTER is a function that takes one cookie and returns a string
to be displayed in the buffer. The string may be empty. If it is not
empty a newline will be added automatically. It may span several lines.
Optional third argument HEADER is a string that will always be present
at the top of the buffer. HEADER should end with a newline. Optionaly
fourth argument FOOTER is similar, and will always be inserted at the
bottom of the buffer."

  (cookie-set-buffer buffer

    (erase-buffer)

    (make-local-variable 'cookie-last-tin)
    (make-local-variable 'cookie-pretty-printer)
    (make-local-variable 'cookie-header)
    (make-local-variable 'cookie-footer)
    (make-local-variable 'cookies)

    (setq cookie-last-tin nil)
    (setq cookie-pretty-printer pretty-printer)
    (setq cookies (dll-create))

    (dll-enter-first cookies
		     (cookie-create-tin-and-insert
		      header header 0))
    (setq cookie-header (dll-nth cookies 0))

    (dll-enter-last cookies
		    (cookie-create-tin-and-insert
		     footer footer (point-max)))
    (setq cookie-footer (dll-nth cookies -1))

    (goto-char (point-min))
    (forward-line 1)))


(defun cookie-set-header (buffer header)
  "Change the header. Args: BUFFER HEADER."
  (impl))


(defun cookie-set-footer (buffer header)
  "Change the footer. Args: BUFFER FOOTER."
  (impl))



(defun cookie-enter-first (buffer cookie)
  "Enter a COOKIE first in BUFFER.
Args: BUFFER COOKIE."

  (cookie-set-buffer buffer

    ;; It is always safe to insert an element after the first element,
    ;; because the header is always present. (dll-nth cookies 0) should
    ;; never return nil.

    (dll-enter-after
     cookies
     (dll-nth cookies 0)
     (cookie-create-tin-and-insert
      cookie
      (funcall cookie-pretty-printer cookie)
      (cookie-tin-start-marker
       (dll-element cookies (dll-nth cookies 1)))))))



(defun cookie-enter-last (buffer cookie)
  "Enter a COOKIE last in BUFFER.
Args: BUFFER COOKIE."

  (cookie-set-buffer buffer

    ;; Remember that the header and footer are always present. There
    ;; is no need to check if (dll-nth cookies -2) returns nil.

    (dll-enter-before
     cookies
     (dll-nth cookies -1)
     (cookie-create-tin-and-insert
      cookie
      (funcall cookie-pretty-printer cookie)
      (cookie-tin-start-marker (dll-last cookies))))))


(defun cookie-enter-after (buffer node cookie)
  (impl))


(defun cookie-enter-before (buffer node cookie)
  (impl))



(defun cookie-next (buffer tin)
  "Get the next tin. Args: BUFFER TIN.
Returns nil if TIN is nil or the last cookie."
  (if tin
      (cookie-set-buffer buffer
	(cookie-filter-hf (dll-next cookies tin)))))



(defun cookie-previous (buffer tin)
  "Get the previous tin. Args: BUFFER TIN.
Returns nil if TIN is nil or the first cookie."
  (if tin
      (cookie-set-buffer buffer
	(cookie-filter-hf (dll-previous cookies tin)))))


(defun cookie-nth (buffer n)

  "Return the Nth tin. Args: BUFFER N.
N counts from zero. Nil is returned if there is less than N cookies.
If N is negative, return the -(N+1)th last element.
Thus, (cookie-nth dll 0) returns the first node,
and (cookie-nth dll -1) returns the last node.

Use cookie-cookie to extract the cookie from the tin."

  (cookie-set-buffer buffer

    ;; Skip the header (or footer, if n is negative).
    (if (< n 0)
	(setq n (1- n))
      (setq n (1+ n)))

    (cookie-filter-hf (dll-nth cookies n))))



(defun cookie-delete (buffer tin)
  "Delete a cookie. Args: BUFFER TIN."

  (cookie-set-buffer buffer
    (if (eq cookie-last-tin tin)
	(setq cookie-last-tin nil))

    (cookie-delete-tin-internal tin)
    (dll-delete cookies tin)))



(defun cookie-delete-first (buffer)
  "Delete first cookie and return it. Args: BUFFER.
Returns nil if there is no cookie left."

  (cookie-set-buffer buffer

    ;; We have to check that we do not try to delete the footer.

    (let ((tin (dll-nth cookies 1)))	;Skip the header.
      (if (eq tin cookie-footer)
	  nil
	(cookie-delete-tin-internal tin)
	(cookie-tin-cookie (dll-delete cookies tin))))))



(defun cookie-delete-last (buffer)
  "Delete last cookie and return it. Args: BUFFER.
Returns nil if there is no cookie left."

  (cookie-set-buffer buffer

    ;; We have to check that we do not try to delete the header.

    (let ((tin (dll-nth cookies -2)))	;Skip the footer.
      (if (eq tin cookie-header)
	  nil
	(cookie-delete-tin-internal tin)
	(cookie-tin-cookie (dll-delete cookies tin))))))



(defun cookie-first (buffer)

  "Return the first cookie in BUFFER. The cookie is not removed."

  (cookie-set-buffer buffer
    (let ((tin (cookie-filter-hf (dll-nth cookies -1))))
      (if tin
	  (cookie-tin-cookie-safe
	   (dll-element cookies tin))))))


(defun cookie-last (buffer)

  "Return the last cookie in BUFFER. The cookie is not removed."

  (cookie-set-buffer buffer
    (let ((tin (cookie-filter-hf (dll-nth cookies -2))))
      (if tin
	  (cookie-tin-cookie-safe
	   (dll-element cookies tin))))))


(defun cookie-empty (buffer)

  "Return true if there are no cookies in BUFFER."

  (cookie-set-buffer buffer
    (eq (dll-nth cookies 1) cookie-footer)))


(defun cookie-length (buffer)

  "Return number of cookies in BUFFER."

  ;; Don't count the footer and header.

  (cookie-set-buffer buffer
    (- (dll-length cookies) 2)))


(defun cookie-all (buffer)

  "Return a list of all cookies in BUFFER."

  (cookie-set-buffer buffer
    (let (result 
	  (tin (dll-nth cookies -2)))
      (while (not (eq tin cookie-header))
	(setq result (cons (cookie-tin-cookie (dll-element cookies tin))
			   result))
	(setq tin (dll-previous cookies tin)))
      result)))

(defun cookie-clear (buffer)

  "Remove all cookies in buffer."

  (cookie-set-buffer buffer
    (cookie-create buffer cookie-pretty-printer
		   (cookie-tin-cookie (dll-element cookies cookie-header))
		   (cookie-tin-cookie (dll-element cookies cookie-footer)))))



(defun cookie-map (map-function buffer &rest map-args)

  "Apply MAP-FUNCTION to all cookies in BUFFER.
MAP-FUNCTION is applied to the first element first.
If MAP-FUNCTION returns non-nil the cookie will be refreshed.

Note that BUFFER will be current buffer when MAP-FUNCTION is called.

If more than two arguments are given to cookie-map, remaining
arguments will be passed to MAP-FUNCTION."

  (cookie-set-buffer buffer
    (let ((tin (dll-nth cookies 1))
	  result)

      (while (not (eq tin cookie-footer))

	(if (apply map-function
		   (cookie-tin-cookie (dll-element cookies tin))
		   map-args)
	    (cookie-refresh-tin tin))

	(setq tin (dll-next cookies tin))))))



(defun cookie-map-reverse (map-function buffer &rest map-args)

  "Apply MAP-FUNCTION to all cookies in BUFFER.
MAP-FUNCTION is applied to the last cookie first.
If MAP-FUNCTION returns non-nil the cookie will be refreshed.

Note that BUFFER will be current buffer when MAP-FUNCTION is called.

If more than two arguments are given to cookie-map, remaining
arguments will be passed to MAP-FUNCTION."

  (cookie-set-buffer buffer
    (let ((tin (dll-nth cookies -2))
	  result)

      (while (not (eq tin cookie-header))

	(if (apply map-function
		   (cookie-tin-cookie (dll-element cookies tin))
		   map-args)
	    (cookie-refresh-tin tin))

	(setq tin (dll-previous cookies tin))))))



(defun cookie-enter-cookies (buffer cookie-list)

  "Insert all cookies in the list COOKIE-LIST last in BUFFER.
Args: BUFFER COOKIE-LIST."

  (while cookie-list
    (cookie-enter-last buffer (car cookie-list))
    (setq cookie-list (cdr cookie-list))))


(defun cookie-filter (buffer predicate)

  "Remove all cookies in BUFFER for which PREDICATE returns nil.
Note that BUFFER will be current-buffer when PREDICATE is called.

The PREDICATE is called with one argument, the cookie."

  (cookie-set-buffer buffer
    (let ((tin (dll-nth cookies 1))
	  next)
      (while (not (eq tin cookie-footer))
	(setq next (dll-next cookies tin))
	(if (funcall predicate (cookie-tin-cookie (dll-element cookies tin)))
	    nil
	  (cookie-delete-tin-internal tin)
	  (dll-delete cookies tin))
	(setq tin next)))))


(defun cookie-filter-tins (buffer predicate)

  "Remove all cookies in BUFFER for which PREDICATE returns nil.
Note that BUFFER will be current-buffer when PREDICATE is called.

The PREDICATE is called with one argument, the tin."

  (cookie-set-buffer buffer
    (let ((tin (dll-nth cookies 1))
	  next)
      (while (not (eq tin cookie-footer))
	(setq next (dll-next cookies tin))
	(if (funcall predicate tin)
	    nil
	  (cookie-delete-tin-internal tin)
	  (dll-delete cookies tin))
	(setq tin next)))))

(defun cookie-pos-before-middle-p (pos tin1 tin2)

  "Return true if POS is in the first half of the region defined by TIN1 and
TIN2."

  (< pos (/ (+ (cookie-tin-start-marker (dll-element cookeis tin1))
	       (cookie-tin-start-marker (dll-element cookeis tin2)))
	    2)))
  

(defun cookie-get-selection (buffer pos &optional guess force-guess)

  "Return the tin the POS is within.
Args: BUFFER POS &optional GUESS FORCE-GUESS.
GUESS should be a tin that it is likely that POS is near. If FORCE-GUESS
is non-nil GUESS is always used as a first guess, otherwise the first
guess is the first tin, last tin, or GUESS, whichever is nearest to
pos in the BUFFER.

If pos points within the header, the first cookie is returned.
If pos points within the footer, the last cookie is returned.
Nil is returned if there is no cookie.

It is often good to specify cookie-last-tin as GUESS, but remember
that cookie-last-tin is buffer local in all buffers that cookie
operates on."

  (cookie-set-buffer buffer

    (cond
     ; No cookies present?
     ((eq (dll-nth cookies 1) (dll-nth cookies -1))
      nil)

     ; Before first cookie?
     ((< pos (cookie-tin-start-marker
	      (dll-element cookies (dll-nth cookies 1))))
      (dll-nth cookies 1))

     ; After last cookie?
     ((>= pos (cookie-tin-start-marker (dll-last cookies)))
      (dll-nth cookies -2))

     ; We now now that pos is within a cookie.
     (t
      ; Make an educated guess about which of the three known
      ; cookies (the first, the last, or GUESS) is nearest.
      (setq
       guess
       (cond
	(force-guess guess)
	(guess
	 (cond
	  ;; Closest to first cookie?
	  ((cookie-pos-before-middle-p
	    pos guess
	    (dll-nth cookies 1))
	   (dll-nth cookies 1))
	  ;; Closest to GUESS?
	  ((cookie-pos-before-middle-p
	    pos guess
	    cookie-footer)
	   guess)
	  ;; Closest to last cookie.
	  (t (dll-previous cookies cookie-footer))))
	(t
	 ;; No guess given.
	 (cond
	  ;; First half?
	  ((cookie-pos-before-middle-p
	    pos (dll-nth cookies 1)
	    cookie-footer)	
	   (dll-nth cookies 1))
	  (t (dll-previous cookies cookie-footer))))))

      ;; GUESS is now a "best guess".
     
      ;; Find the correct cookie. First determine in which direction
      ;; it lies, and then move in that direction until it is found.
    
      (cond
       ;; Is pos after the guess?
       ((>= pos (cookie-tin-start-marker (dll-element cookiess guess)))

	;; Loop until we are exactly one cookie too far down...
	(while (>= pos (cookie-tin-start-marker (dll-element cookiess guess)))
	  (setq guess (dll-next cookies guess)))

	;; ...and return the previous cookie.
	(dll-previous cookies guess))

       ;; Pos is before guess
       (t

	(while (< pos (cookie-tin-start-marker (dll-element cookiess guess)))
	  (setq guess (dll-previous cookies guess)))

	guess))))))


(defun cookie-start-marker (buffer tin)

  "Return start-position of a cookie in BUFFER.
Args: BUFFER TIN.
The marker that is returned should not be modified in any way,
and is only valid until the contents of the cookie buffer changes."

  (cookie-set-buffer buffer
    (cookie-tin-start-marker (dll-element cookies tin))))


(defun cookie-end-marker (buffer tin)

  "Return end-position of a cookie in BUFFER.
Args: BUFFER TIN.
The marker that is returned should not be modified in any way,
and is only valid until the contents of the cookie buffer changes."

  (cookie-set-buffer buffer
    (cookie-tin-start-marker
     (dll-element cookies (dll-next cookies tin)))))



(defun cookie-refresh (buffer)

  "Refresh all cookies in BUFFER.
Cookie-pretty-printer will be called for all cookies and the new result
displayed.

See also cookie-invalidate-tins."

  (cookie-set-buffer buffer

    (erase-buffer)

    (set-marker (cookie-tin-start-marker (dll-element cookies cookie-header))
		(point) buffer)
    (insert (cookie-tin-cookie (dll-element cookies cookie-header)))
    (insert "\n")
    
    (let ((tin (dll-nth cookies 1)))
      (while (not (eq tin cookie-footer))

	(set-marker (cookie-tin-start-marker (dll-element cookies tin))
		    (point) buffer)
	(insert
	 (funcall cookie-pretty-printer
		  (cookie-tin-cookie (dll-element cookies tin))))
	(insert "\n")
	(setq tin (dll-next cookies tin))))
    
    (set-marker (cookie-tin-start-marker (dll-element cookies cookie-footer))
		(point) buffer)
    (insert (cookie-tin-cookie (dll-element cookies cookie-footer)))
    (insert "\n")))


(defun cookie-invalidate-tins (buffer &rest tins)

  "Refresh some cookies.
Args: BUFFER &rest TINS."

  (cookie-set-buffer buffer
    
    (while tins
      (cookie-refresh-tin (car tins))
      (setq tins (cdr tins)))))


;;; Cookie movement commands.

(defun cookie-set-goal-column (buffer goal)
  "Set goal-column for BUFFER.
Args: BUFFER GOAL.
goal-column is made buffer-local."
  (cookie-set-buffer buffer
    (make-local-variable 'goal-column)
    (setq goal-column goal)))


(defun cookie-previous-cookie (buffer pos arg)
  "Move point to the ARGth previous cookie.
Don't move if we are at the first cookie.
ARG is the prefix argument when called interactively.
Args: BUFFER POS ARG.
Sets cookie-last-tin to the cookie we move to."

  (interactive (list (current-buffer) (point)
		     (prefix-numeric-value current-prefix-arg)))

  (cookie-set-buffer buffer
    (setq cookie-last-tin
	  (cookie-get-selection buffer pos cookie-last-tin))

    (while (and cookie-last-tin (> arg 0))
      (setq arg (1- arg))
      (setq cookie-last-tin 
	    (dll-previous cookies cookie-last-tin)))

    ;; Never step above the first cookie.

    (if (null (cookie-filter-hf cookie-last-tin))
	(setq cookie-last-tin (dll-nth cookies 1)))

    (goto-char
     (cookie-tin-start-marker
      (dll-element cookies cookie-last-tin)))

    (if goal-column
	(move-to-column goal-column))))



(defun cookie-next-cookie (buffer pos arg)
  "Move point to the ARGth next cookie.
Don't move if we are at the last cookie.
ARG is the prefix argument when called interactively.
Args: BUFFER POS ARG.
Sets cookie-last-tin to the cookie we move to."

  (interactive (list (current-buffer) (point)
		     (prefix-numeric-value current-prefix-arg)))

  (cookie-set-buffer buffer
    (setq cookie-last-tin
	  (cookie-get-selection buffer pos cookie-last-tin))

    (while (and cookie-last-tin (> arg 0))
      (setq arg (1- arg))
      (setq cookie-last-tin 
	    (dll-next cookies cookie-last-tin)))

    (if (null (cookie-filter-hf cookie-last-tin))
	(setq cookie-last-tin (dll-nth cookies -2)))

    (goto-char
     (cookie-tin-start-marker
      (dll-element cookies cookie-last-tin)))

    (if goal-column
	(move-to-column goal-column))))


(defun cookie-collect-tins (buffer predicate &rest predicate-args)

  "Return a list of all tins in BUFFER whose cookie PREDICATE
returns true for.
PREDICATE is a function that takes a cookie as its argument.
The tins on the returned list will appear in the same order
as in the buffer. You should not rely on in which order PREDICATE
is called. Note that BUFFER is current-buffer when PREDICATE
is called. (If you call cookie-collect with another buffer set
as current-buffer and need to access buffer-local variables
from that buffer within PREDICATE you must send them via
PREDICATE-ARGS).

If more than two arguments are given to cookie-collect the remaining
arguments will be passed to PREDICATE.

Use cookie-cookie to get the cookie from the tin."

  (cookie-set-buffer buffer
    (let ((tin (dll-nth cookies -2))
	  result)

      (while (not (eq tin cookie-header))

	(if (apply predicate
		   (cookie-tin-cookie (dll-element cookies tin))
		   predicate-args)
	    (setq result (cons tin result)))

	(setq tin (dll-previous cookies tin)))
      result)))


(defun cookie-collect-cookies (buffer predicate &rest predicate-args)

  "Return a list of all cookies in BUFFER that PREDICATE
returns true for.
PREDICATE is a function that takes a cookie as its argument.
The cookie on the returned list will appear in the same order
as in the buffer. You should not rely on in which order PREDICATE
is called. Note that BUFFER is current-buffer when PREDICATE
is called. (If you call cookie-collect with another buffer set
as current-buffer and need to access buffer-local variables
from that buffer within PREDICATE you must send them via
PREDICATE-ARGS).

If more than two arguments are given to cookie-collect the remaining
arguments will be passed to PREDICATE."

  (cookie-set-buffer buffer
    (let ((tin (dll-nth cookies -2))
	  result)

      (while (not (eq tin cookie-header))

	(if (apply predicate
		   (cookie-tin-cookie (dll-element cookies tin))
		   predicate-args)
	    (setq result (cons (cookie-tin-cookie (dll-element cookies tin))
			       result)))

	(setq tin (dll-previous cookies tin)))
      result)))
