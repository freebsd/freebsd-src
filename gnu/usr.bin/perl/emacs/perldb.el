;; Run perl -d under Emacs
;; Based on gdb.el, as written by W. Schelter, and modified by rms.
;; Modified for Perl by Ray Lischner (uunet!mntgfx!lisch), Nov 1990.

;; This file is part of GNU Emacs.
;; Copyright (C) 1988,1990 Free Software Foundation, Inc.

;; GNU Emacs is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
;; to anyone for the consequences of using it or for whether it serves
;; any particular purpose or works at all, unless he says so in writing.
;; Refer to the GNU Emacs General Public License for full details.

;; Everyone is granted permission to copy, modify and redistribute GNU
;; Emacs, but only under the conditions described in the GNU Emacs
;; General Public License.  A copy of this license is supposed to have
;; been given to you along with GNU Emacs so you can know your rights and
;; responsibilities.  It should be in a file named COPYING.  Among other
;; things, the copyright notice and this notice must be preserved on all
;; copies.

;; Description of perl -d interface:

;; A facility is provided for the simultaneous display of the source code
;; in one window, while using perldb to step through a function in the
;; other.  A small arrow in the source window, indicates the current
;; line.

;; Starting up:

;; In order to use this facility, invoke the command PERLDB to obtain a
;; shell window with the appropriate command bindings.  You will be asked
;; for the name of a file to run and additional command line arguments.
;; Perldb will be invoked on this file, in a window named *perldb-foo*
;; if the file is foo.

;; M-s steps by one line, and redisplays the source file and line.

;; You may easily create additional commands and bindings to interact
;; with the display.  For example to put the perl debugger command n on \M-n
;; (def-perldb n "\M-n")

;; This causes the emacs command perldb-next to be defined, and runs
;; perldb-display-frame after the command.

;; perldb-display-frame is the basic display function.  It tries to display
;; in the other window, the file and line corresponding to the current
;; position in the perldb window.  For example after a perldb-step, it would
;; display the line corresponding to the position for the last step.  Or
;; if you have done a backtrace in the perldb buffer, and move the cursor
;; into one of the frames, it would display the position corresponding to
;; that frame.

;; perldb-display-frame is invoked automatically when a filename-and-line-number
;; appears in the output.


(require 'shell)

(defvar perldb-prompt-pattern "^  DB<[0-9]+> "
  "A regexp to recognize the prompt for perldb.") 

(defvar perldb-mode-map nil
  "Keymap for perldb-mode.")

(if perldb-mode-map
   nil
  (setq perldb-mode-map (copy-keymap shell-mode-map))
  (define-key perldb-mode-map "\C-l" 'perldb-refresh))

(define-key ctl-x-map " " 'perldb-break)
(define-key ctl-x-map "&" 'send-perldb-command)

;;Of course you may use `def-perldb' with any other perldb command, including
;;user defined ones.   

(defmacro def-perldb (name key &optional doc)
  (let* ((fun (intern (concat "perldb-" name))))
    (` (progn
	 (defun (, fun) (arg)
	   (, (or doc ""))
	   (interactive "p")
	   (perldb-call (if (not (= 1 arg))
			    (concat (, name) arg)
			  (, name))))
	 (define-key perldb-mode-map (, key) (quote (, fun)))))))

(def-perldb "s"   "\M-s" "Step one source line with display")
(def-perldb "n"   "\M-n" "Step one source line (skip functions)")
(def-perldb "c"   "\M-c" "Continue with display")
(def-perldb "r"   "\C-c\C-r" "Return from current subroutine")
(def-perldb "A"   "\C-c\C-a" "Delete all actions")

(defun perldb-mode ()
  "Major mode for interacting with an inferior Perl debugger process.
The following commands are available:

\\{perldb-mode-map}

\\[perldb-display-frame] displays in the other window
the last line referred to in the perldb buffer.

\\[perldb-s],\\[perldb-n], and \\[perldb-n] in the perldb window,
call perldb to step, next or continue and then update the other window
with the current file and position.

If you are in a source file, you may select a point to break
at, by doing \\[perldb-break].

Commands:
Many commands are inherited from shell mode. 
Additionally we have:

\\[perldb-display-frame] display frames file in other window
\\[perldb-s] advance one line in program
\\[perldb-n] advance one line in program (skip over calls).
\\[send-perldb-command] used for special printing of an arg at the current point.
C-x SPACE sets break point at current line."
  (interactive)
  (kill-all-local-variables)
  (setq major-mode 'perldb-mode)
  (setq mode-name "Inferior Perl")
  (setq mode-line-process '(": %s"))
  (use-local-map perldb-mode-map)
  (make-local-variable 'last-input-start)
  (setq last-input-start (make-marker))
  (make-local-variable 'last-input-end)
  (setq last-input-end (make-marker))
  (make-local-variable 'perldb-last-frame)
  (setq perldb-last-frame nil)
  (make-local-variable 'perldb-last-frame-displayed-p)
  (setq perldb-last-frame-displayed-p t)
  (make-local-variable 'perldb-delete-prompt-marker)
  (setq perldb-delete-prompt-marker nil)
  (make-local-variable 'perldb-filter-accumulator)
  (setq perldb-filter-accumulator nil)
  (make-local-variable 'shell-prompt-pattern)
  (setq shell-prompt-pattern perldb-prompt-pattern)
  (run-hooks 'shell-mode-hook 'perldb-mode-hook))

(defvar current-perldb-buffer nil)

(defvar perldb-command-name "perl"
  "Pathname for executing perl -d.")

(defun end-of-quoted-arg (argstr start end)
  (let* ((chr (substring argstr start (1+ start)))
	 (idx (string-match (concat "[^\\]" chr) argstr (1+ start))))
    (and idx (1+ idx))
    )
)

(defun parse-args-helper (arglist argstr start end)
  (while (and (< start end) (string-match "[ \t\n\f\r\b]"
					  (substring argstr start (1+ start))))
    (setq start (1+ start)))
  (cond
    ((= start end) arglist)
    ((string-match "[\"']" (substring argstr start (1+ start)))
     (let ((next (end-of-quoted-arg argstr start end)))
       (parse-args-helper (cons (substring argstr (1+ start) next) arglist)
			  argstr (1+ next) end)))
    (t (let ((next (string-match "[ \t\n\f\b\r]" argstr start)))
	 (if next
	     (parse-args-helper (cons (substring argstr start next) arglist)
				argstr (1+ next) end)
	   (cons (substring argstr start) arglist))))
    )
  )
    
(defun parse-args (args)
  "Extract arguments from a string ARGS.
White space separates arguments, with single or double quotes
used to protect spaces.  A list of strings is returned, e.g.,
(parse-args \"foo bar 'two args'\") => (\"foo\" \"bar\" \"two args\")."
  (nreverse (parse-args-helper '() args 0 (length args)))
)

(defun perldb (path args)
  "Run perldb on program FILE in buffer *perldb-FILE*.
The default directory for the current buffer becomes the initial
working directory, by analogy with  gdb .  If you wish to change this, use
the Perl command `chdir(DIR)'."
  (interactive "FRun perl -d on file: \nsCommand line arguments: ")
  (setq path (expand-file-name path))
  (let ((file (file-name-nondirectory path))
	(dir default-directory))
    (switch-to-buffer (concat "*perldb-" file "*"))
    (setq default-directory dir)
    (or (bolp) (newline))
    (insert "Current directory is " default-directory "\n")
    (apply 'make-shell
	   (concat "perldb-" file) perldb-command-name nil "-d" path "-emacs"
	   (parse-args args))
    (perldb-mode)
    (set-process-filter (get-buffer-process (current-buffer)) 'perldb-filter)
    (set-process-sentinel (get-buffer-process (current-buffer)) 'perldb-sentinel)
    (perldb-set-buffer)))

(defun perldb-set-buffer ()
  (cond ((eq major-mode 'perldb-mode)
	(setq current-perldb-buffer (current-buffer)))))

;; This function is responsible for inserting output from Perl
;; into the buffer.
;; Aside from inserting the text, it notices and deletes
;; each filename-and-line-number;
;; that Perl prints to identify the selected frame.
;; It records the filename and line number, and maybe displays that file.
(defun perldb-filter (proc string)
  (let ((inhibit-quit t))
    (if perldb-filter-accumulator
	(perldb-filter-accumulate-marker proc
				      (concat perldb-filter-accumulator string))
	(perldb-filter-scan-input proc string))))

(defun perldb-filter-accumulate-marker (proc string)
  (setq perldb-filter-accumulator nil)
  (if (> (length string) 1)
      (if (= (aref string 1) ?\032)
	  (let ((end (string-match "\n" string)))
	    (if end
		(progn
		  (let* ((first-colon (string-match ":" string 2))
			 (second-colon
			  (string-match ":" string (1+ first-colon))))
		    (setq perldb-last-frame
			  (cons (substring string 2 first-colon)
				(string-to-int
				 (substring string (1+ first-colon)
					    second-colon)))))
		  (setq perldb-last-frame-displayed-p nil)
		  (perldb-filter-scan-input proc
					 (substring string (1+ end))))
	      (setq perldb-filter-accumulator string)))
	(perldb-filter-insert proc "\032")
	(perldb-filter-scan-input proc (substring string 1)))
    (setq perldb-filter-accumulator string)))

(defun perldb-filter-scan-input (proc string)
  (if (equal string "")
      (setq perldb-filter-accumulator nil)
      (let ((start (string-match "\032" string)))
	(if start
	    (progn (perldb-filter-insert proc (substring string 0 start))
		   (perldb-filter-accumulate-marker proc
						 (substring string start)))
	    (perldb-filter-insert proc string)))))

(defun perldb-filter-insert (proc string)
  (let ((moving (= (point) (process-mark proc)))
	(output-after-point (< (point) (process-mark proc)))
	(old-buffer (current-buffer))
	start)
    (set-buffer (process-buffer proc))
    (unwind-protect
	(save-excursion
	  ;; Insert the text, moving the process-marker.
	  (goto-char (process-mark proc))
	  (setq start (point))
	  (insert string)
	  (set-marker (process-mark proc) (point))
	  (perldb-maybe-delete-prompt)
	  ;; Check for a filename-and-line number.
	  (perldb-display-frame
	   ;; Don't display the specified file
	   ;; unless (1) point is at or after the position where output appears
	   ;; and (2) this buffer is on the screen.
	   (or output-after-point
	       (not (get-buffer-window (current-buffer))))
	   ;; Display a file only when a new filename-and-line-number appears.
	   t))
      (set-buffer old-buffer))
    (if moving (goto-char (process-mark proc)))))

(defun perldb-sentinel (proc msg)
  (cond ((null (buffer-name (process-buffer proc)))
	 ;; buffer killed
	 ;; Stop displaying an arrow in a source file.
	 (setq overlay-arrow-position nil)
	 (set-process-buffer proc nil))
	((memq (process-status proc) '(signal exit))
	 ;; Stop displaying an arrow in a source file.
	 (setq overlay-arrow-position nil)
	 ;; Fix the mode line.
	 (setq mode-line-process
	       (concat ": "
		       (symbol-name (process-status proc))))
	 (let* ((obuf (current-buffer)))
	   ;; save-excursion isn't the right thing if
	   ;;  process-buffer is current-buffer
	   (unwind-protect
	       (progn
		 ;; Write something in *compilation* and hack its mode line,
		 (set-buffer (process-buffer proc))
		 ;; Force mode line redisplay soon
		 (set-buffer-modified-p (buffer-modified-p))
		 (if (eobp)
		     (insert ?\n mode-name " " msg)
		   (save-excursion
		     (goto-char (point-max))
		     (insert ?\n mode-name " " msg)))
		 ;; If buffer and mode line will show that the process
		 ;; is dead, we can delete it now.  Otherwise it
		 ;; will stay around until M-x list-processes.
		 (delete-process proc))
	     ;; Restore old buffer, but don't restore old point
	     ;; if obuf is the perldb buffer.
	     (set-buffer obuf))))))


(defun perldb-refresh ()
  "Fix up a possibly garbled display, and redraw the arrow."
  (interactive)
  (redraw-display)
  (perldb-display-frame))

(defun perldb-display-frame (&optional nodisplay noauto)
  "Find, obey and delete the last filename-and-line marker from PERLDB.
The marker looks like \\032\\032FILENAME:LINE:CHARPOS\\n.
Obeying it means displaying in another window the specified file and line."
  (interactive)
  (perldb-set-buffer)
  (and perldb-last-frame (not nodisplay)
       (or (not perldb-last-frame-displayed-p) (not noauto))
       (progn (perldb-display-line (car perldb-last-frame) (cdr perldb-last-frame))
	      (setq perldb-last-frame-displayed-p t))))

;; Make sure the file named TRUE-FILE is in a buffer that appears on the screen
;; and that its line LINE is visible.
;; Put the overlay-arrow on the line LINE in that buffer.

(defun perldb-display-line (true-file line)
  (let* ((buffer (find-file-noselect true-file))
	 (window (display-buffer buffer t))
	 (pos))
    (save-excursion
      (set-buffer buffer)
      (save-restriction
	(widen)
	(goto-line line)
	(setq pos (point))
	(setq overlay-arrow-string "=>")
	(or overlay-arrow-position
	    (setq overlay-arrow-position (make-marker)))
	(set-marker overlay-arrow-position (point) (current-buffer)))
      (cond ((or (< pos (point-min)) (> pos (point-max)))
	     (widen)
	     (goto-char pos))))
    (set-window-point window overlay-arrow-position)))

(defun perldb-call (command)
  "Invoke perldb COMMAND displaying source in other window."
  (interactive)
  (goto-char (point-max))
  (setq perldb-delete-prompt-marker (point-marker))
  (perldb-set-buffer)
  (send-string (get-buffer-process current-perldb-buffer)
	       (concat command "\n")))

(defun perldb-maybe-delete-prompt ()
  (if (and perldb-delete-prompt-marker
	   (> (point-max) (marker-position perldb-delete-prompt-marker)))
      (let (start)
	(goto-char perldb-delete-prompt-marker)
	(setq start (point))
	(beginning-of-line)
	(delete-region (point) start)
	(setq perldb-delete-prompt-marker nil))))

(defun perldb-break ()
  "Set PERLDB breakpoint at this source line."
  (interactive)
  (let ((line (save-restriction
		(widen)
		(1+ (count-lines 1 (point))))))
    (send-string (get-buffer-process current-perldb-buffer)
		 (concat "b " line "\n"))))

(defun perldb-read-token()
  "Return a string containing the token found in the buffer at point.
A token can be a number or an identifier.  If the token is a name prefaced
by `$', `@', or `%', the leading character is included in the token."
  (save-excursion
    (let (begin)
      (or (looking-at "[$@%]")
	  (re-search-backward "[^a-zA-Z_0-9]" (point-min) 'move))
      (setq begin (point))
      (or (looking-at "[$@%]") (setq begin (+ begin 1)))
      (forward-char 1)
      (buffer-substring begin
			(if (re-search-forward "[^a-zA-Z_0-9]"
					       (point-max) 'move)
			       (- (point) 1)
			  (point)))
)))

(defvar perldb-commands nil
  "List of strings or functions used by send-perldb-command.
It is for customization by the user.")

(defun send-perldb-command (arg)
  "Issue a Perl debugger command selected by the prefix arg.  A numeric
arg selects the ARG'th member COMMAND of the list perldb-commands.
The token under the cursor is passed to the command.  If COMMAND is a
string, (format COMMAND TOKEN) is inserted at the end of the perldb
buffer, otherwise (funcall COMMAND TOKEN) is inserted.  If there is
no such COMMAND, then the token itself is inserted.  For example,
\"p %s\" is a possible string to be a member of perldb-commands,
or \"p $ENV{%s}\"."
  (interactive "P")
  (let (comm token)
    (if arg (setq comm (nth arg perldb-commands)))
    (setq token (perldb-read-token))
    (if (eq (current-buffer) current-perldb-buffer)
	(set-mark (point)))
    (cond (comm
	   (setq comm
		 (if (stringp comm) (format comm token) (funcall comm token))))
	  (t (setq comm token)))
    (switch-to-buffer-other-window current-perldb-buffer)
    (goto-char (dot-max))
    (insert-string comm)))
