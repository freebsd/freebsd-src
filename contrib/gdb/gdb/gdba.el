(defmacro gud (form)
  (` (save-excursion (set-buffer "*gud-a.out*") (, form))))

(defun dbug (foo &optional fun)
  (save-excursion
    (set-buffer (get-buffer-create "*trace*"))
    (goto-char (point-max))
    (insert "***" (symbol-name foo) "\n")
    (if fun
	(funcall fun))))


;;; gud.el --- Grand Unified Debugger mode for gdb, sdb, dbx, or xdb
;;;            under Emacs

;; Author: Eric S. Raymond <esr@snark.thyrsus.com>
;; Maintainer: FSF
;; Version: 1.3
;; Keywords: unix, tools

;; Copyright (C) 1992, 1993 Free Software Foundation, Inc.

;; This file is part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; if not, write to the Free Software
;; Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

;;; Commentary:

;; The ancestral gdb.el was by W. Schelter <wfs@rascal.ics.utexas.edu>
;; It was later rewritten by rms.  Some ideas were due to Masanobu. 
;; Grand Unification (sdb/dbx support) by Eric S. Raymond <esr@thyrsus.com>
;; The overloading code was then rewritten by Barry Warsaw <bwarsaw@cen.com>,
;; who also hacked the mode to use comint.el.  Shane Hartman <shane@spr.com>
;; added support for xdb (HPUX debugger).

;; Cygnus Support added support for gdb's --annotate=2.

;;; Code:

(require 'comint)
(require 'etags)

;; ======================================================================
;; GUD commands must be visible in C buffers visited by GUD

(defvar gud-key-prefix "\C-x\C-a"
  "Prefix of all GUD commands valid in C buffers.")

(global-set-key (concat gud-key-prefix "\C-l") 'gud-refresh)
(global-set-key "\C-x " 'gud-break)	;; backward compatibility hack

;; ======================================================================
;; the overloading mechanism

(defun gud-overload-functions (gud-overload-alist)
  "Overload functions defined in GUD-OVERLOAD-ALIST.
This association list has elements of the form
     (ORIGINAL-FUNCTION-NAME  OVERLOAD-FUNCTION)"
  (mapcar
   (function (lambda (p) (fset (car p) (symbol-function (cdr p)))))
   gud-overload-alist))

(defun gud-massage-args (file args)
  (error "GUD not properly entered."))

(defun gud-marker-filter (str)
  (error "GUD not properly entered."))

(defun gud-find-file (f)
  (error "GUD not properly entered."))

;; ======================================================================
;; command definition

;; This macro is used below to define some basic debugger interface commands.
;; Of course you may use `gud-def' with any other debugger command, including
;; user defined ones.

;; A macro call like (gud-def FUNC NAME KEY DOC) expands to a form
;; which defines FUNC to send the command NAME to the debugger, gives
;; it the docstring DOC, and binds that function to KEY in the GUD
;; major mode.  The function is also bound in the global keymap with the
;; GUD prefix.

(defmacro gud-def (func cmd key &optional doc)
  "Define FUNC to be a command sending STR and bound to KEY, with
optional doc string DOC.  Certain %-escapes in the string arguments
are interpreted specially if present.  These are:

  %f	name (without directory) of current source file. 
  %d	directory of current source file. 
  %l	number of current source line
  %e	text of the C lvalue or function-call expression surrounding point.
  %a	text of the hexadecimal address surrounding point
  %p	prefix argument to the command (if any) as a number

  The `current' source file is the file of the current buffer (if
we're in a C file) or the source file current at the last break or
step (if we're in the GUD buffer).
  The `current' line is that of the current buffer (if we're in a
source file) or the source line number at the last break or step (if
we're in the GUD buffer)."
  (list 'progn
	(list 'defun func '(arg)
	      (or doc "")
	      '(interactive "p")
	      (list 'gud-call cmd 'arg))
	(if key
	    (list 'define-key
		  '(current-local-map)
		  (concat "\C-c" key)
		  (list 'quote func)))
	(if key
	    (list 'global-set-key
		  (list 'concat 'gud-key-prefix key)
		  (list 'quote func)))))

;; Where gud-display-frame should put the debugging arrow.  This is
;; set by the marker-filter, which scans the debugger's output for
;; indications of the current program counter.
(defvar gud-last-frame nil)

;; Used by gud-refresh, which should cause gud-display-frame to redisplay
;; the last frame, even if it's been called before and gud-last-frame has
;; been set to nil.
(defvar gud-last-last-frame nil)

;; All debugger-specific information is collected here.
;; Here's how it works, in case you ever need to add a debugger to the mode.
;;
;; Each entry must define the following at startup:
;;
;;<name>
;; comint-prompt-regexp
;; gud-<name>-massage-args
;; gud-<name>-marker-filter
;; gud-<name>-find-file
;;
;; The job of the massage-args method is to modify the given list of
;; debugger arguments before running the debugger.
;;
;; The job of the marker-filter method is to detect file/line markers in
;; strings and set the global gud-last-frame to indicate what display
;; action (if any) should be triggered by the marker.  Note that only
;; whatever the method *returns* is displayed in the buffer; thus, you
;; can filter the debugger's output, interpreting some and passing on
;; the rest.
;;
;; The job of the find-file method is to visit and return the buffer indicated
;; by the car of gud-tag-frame.  This may be a file name, a tag name, or
;; something else.

;; ======================================================================
;; gdb functions

;;; History of argument lists passed to gdb.
(defvar gud-gdb-history nil)

(defun gud-gdb-massage-args (file args)
  (cons "--annotate=2" (cons file args)))


;;
;; In this world, there are gdb instance objects (of unspecified 
;; representation) and buffers associated with those objects.
;;

;; 
;; gdb-instance objects
;; 

(defun make-gdb-instance (proc)
  "Create a gdb instance object from a gdb process."
  (setq last-proc proc)
  (let ((instance (cons 'gdb-instance proc)))
    (save-excursion
      (set-buffer (process-buffer proc))
      (setq gdb-buffer-instance instance)
      (progn
	(mapcar 'make-variable-buffer-local gdb-instance-variables)
	(setq gdb-buffer-type 'gud)
	;; If we're taking over the buffer of another process,
	;; take over it's ancillery buffers as well.
	;;
	(let ((dead (or old-gdb-buffer-instance)))
	  (mapcar
	   (function
	    (lambda (b)
	      (progn
		(set-buffer b)
		(if (eq dead gdb-buffer-instance)
		    (setq gdb-buffer-instance instance)))))
	     (buffer-list)))))
    instance))

(defun gdb-instance-process (inst) (cdr inst))

;;; The list of instance variables is built up by the expansions of
;;; DEF-GDB-VARIABLE
;;;
(defvar gdb-instance-variables '()
  "A list of variables that are local to the gud buffer associated
with a gdb instance.") 

(defmacro def-gdb-variable
  (name accessor setter &optional default doc)
  (`
   (progn
     (defvar (, name) (, default) (, (or doc "undocumented")))
     (if (not (memq '(, name) gdb-instance-variables))
	 (setq gdb-instance-variables
	       (cons '(, name) gdb-instance-variables)))
     (, (and accessor
	     (`
	      (defun (, accessor) (instance)
		(let
		    ((buffer (gdb-get-instance-buffer instance 'gud)))
		  (and buffer
		       (save-excursion
			 (set-buffer buffer)
			 (, name))))))))
     (, (and setter
	     (`
	      (defun (, setter) (instance val)
		(let
		    ((buffer (gdb-get-instance-buffer instance 'gud)))
		  (and buffer
		       (save-excursion
			 (set-buffer buffer)
			 (setq (, name) val)))))))))))

(defmacro def-gdb-var (root-symbol &optional default doc)
  (let* ((root (symbol-name root-symbol))
	 (accessor (intern (concat "gdb-instance-" root)))
	 (setter (intern (concat "set-gdb-instance-" root)))
	 (var-name (intern (concat "gdb-" root))))
    (` (def-gdb-variable
	 (, var-name) (, accessor) (, setter)
	 (, default) (, doc)))))

(def-gdb-var buffer-instance nil
  "In an instance buffer, the buffer's instance.")

(def-gdb-var buffer-type nil
  "One of the symbols bound in gdb-instance-buffer-rules")

(def-gdb-var burst ""
  "A string of characters from gdb that have not yet been processed.")

(def-gdb-var input-queue ()
  "A list of high priority gdb command objects.")

(def-gdb-var idle-input-queue ()
  "A list of low priority gdb command objects.")

(def-gdb-var prompting nil
  "True when gdb is idle with no pending input.")

(def-gdb-var output-sink 'user
  "The disposition of the output of the current gdb command.
Possible values are these symbols:

    user -- gdb output should be copied to the gud buffer 
            for the user to see.

    inferior -- gdb output should be copied to the inferior-io buffer

    pre-emacs -- output should be ignored util the post-prompt
                 annotation is received.  Then the output-sink
		 becomes:...
    emacs -- output should be collected in the partial-output-buffer
	     for subsequent processing by a command.  This is the
	     disposition of output generated by commands that
	     gud mode sends to gdb on its own behalf.
    post-emacs -- ignore input until the prompt annotation is 
		  received, then go to USER disposition.
")

(def-gdb-var current-item nil
  "The most recent command item sent to gdb.")

(def-gdb-var pending-triggers '()
  "A list of trigger functions that have run later than their output
handlers.")

(defun in-gdb-instance-context (instance form)
  "Funcall `form' in the gud buffer of `instance'"
  (save-excursion
    (set-buffer (gdb-get-instance-buffer instance 'gud))
    (funcall form)))

;; end of instance vars

;;
;; finding instances
;;

(defun gdb-proc->instance (proc)
  (save-excursion
    (set-buffer (process-buffer proc))
    gdb-buffer-instance))

(defun gdb-mru-instance-buffer ()
  "Return the most recently used (non-auxiliary) gdb gud buffer."
  (save-excursion
    (gdb-goto-first-gdb-instance (buffer-list))))

(defun gdb-goto-first-gdb-instance (blist)
  "Use gdb-mru-instance-buffer -- not this."
  (and blist
       (progn
	 (set-buffer (car blist))
	 (or (and gdb-buffer-instance
		  (eq gdb-buffer-type 'gud)
		  (car blist))
	     (gdb-goto-first-gdb-instance (cdr blist))))))

(defun buffer-gdb-instance (buf)
  (save-excursion
    (set-buffer buf)
    gdb-buffer-instance))

(defun gdb-needed-default-instance ()
  "Return the most recently used gdb instance or signal an error."
  (let ((buffer (gdb-mru-instance-buffer)))
    (or (and buffer (buffer-gdb-instance buffer))
	(error "No instance of gdb found."))))

(defun gdb-instance-target-string (instance)
  "The apparent name of the program being debugged by a gdb instance.
For sure this the root string used in smashing together the gud 
buffer's name, even if that doesn't happen to be the name of a 
program."
  (in-gdb-instance-context
   instance
   (function (lambda () gud-target-name))))



;;
;; Instance Buffers.
;;

;; More than one buffer can be associated with a gdb instance.
;;
;; Each buffer has a TYPE -- a symbol that identifies the function
;; of that particular buffer.
;;
;; The usual gud interaction buffer is given the type `gud' and
;; is constructed specially.  
;;
;; Others are constructed by gdb-get-create-instance-buffer and 
;; named according to the rules set forth in the gdb-instance-buffer-rules-assoc

(defun gdb-get-instance-buffer (instance key)
  "Return the instance buffer for `instance' tagged with type `key'.
The key should be one of the cars in `gdb-instance-buffer-rules-assoc'."
  (save-excursion
    (gdb-look-for-tagged-buffer instance key (buffer-list))))

(defun gdb-get-create-instance-buffer (instance key)
  "Create a new gdb instance buffer of the type specified by `key'.
The key should be one of the cars in `gdb-instance-buffer-rules-assoc'."
  (or (gdb-get-instance-buffer instance key)
      (let* ((rules (assoc key gdb-instance-buffer-rules-assoc))
	     (name (funcall (gdb-rules-name-maker rules) instance))
	     (new (get-buffer-create name)))
	(save-excursion
	  (set-buffer new)
	  (make-variable-buffer-local 'gdb-buffer-type)
	  (setq gdb-buffer-type key)
	  (make-variable-buffer-local 'gdb-buffer-instance)
	  (setq gdb-buffer-instance instance)
	  (if (cdr (cdr rules))
	      (funcall (car (cdr (cdr rules)))))
	  new))))

(defun gdb-rules-name-maker (rules) (car (cdr rules)))

(defun gdb-look-for-tagged-buffer (instance key bufs)
  (let ((retval nil))
    (while (and (not retval) bufs)
      (set-buffer (car bufs))
      (if (and (eq gdb-buffer-instance instance)
	       (eq gdb-buffer-type key))
	  (setq retval (car bufs)))
      (setq bufs (cdr bufs))
      )
    retval))

(defun gdb-instance-buffer-p (buf)
  (save-excursion
    (set-buffer buf)
    (and gdb-buffer-type
	 (not (eq gdb-buffer-type 'gud)))))

;;
;; This assoc maps buffer type symbols to rules.  Each rule is a list of
;; at least one and possible more functions.  The functions have these
;; roles in defining a buffer type:
;;
;;     NAME - take an instance, return a name for this type buffer for that 
;;	      instance.
;; The remaining function(s) are optional:
;;
;;     MODE - called in new new buffer with no arguments, should establish
;;	      the proper mode for the buffer.
;;

(defvar gdb-instance-buffer-rules-assoc '())

(defun gdb-set-instance-buffer-rules (buffer-type &rest rules)
  (let ((binding (assoc buffer-type gdb-instance-buffer-rules-assoc)))
    (if binding
	(setcdr binding rules)
      (setq gdb-instance-buffer-rules-assoc
	    (cons (cons buffer-type rules)
		  gdb-instance-buffer-rules-assoc)))))

(gdb-set-instance-buffer-rules 'gud 'error) ; gud buffers are an exception to the rules

;;
;; partial-output buffers
;;
;; These accumulate output from a command executed on
;; behalf of emacs (rather than the user).  
;;

(gdb-set-instance-buffer-rules 'gdb-partial-output-buffer
			       'gdb-partial-output-name)

(defun gdb-partial-output-name (instance)
  (concat "*partial-output-"
	  (gdb-instance-target-string instance)
	  "*"))


(gdb-set-instance-buffer-rules 'gdb-inferior-io
			       'gdb-inferior-io-name
			       'gud-inferior-io-mode)

(defun gdb-inferior-io-name (instance)
  (concat "*input/output of "
	  (gdb-instance-target-string instance)
	  "*"))

(defvar gdb-inferior-io-mode-map (copy-keymap comint-mode-map))
(define-key comint-mode-map "\C-c\C-c" 'gdb-inferior-io-interrupt)
(define-key comint-mode-map "\C-c\C-z" 'gdb-inferior-io-stop)
(define-key comint-mode-map "\C-c\C-\\" 'gdb-inferior-io-quit)
(define-key comint-mode-map "\C-c\C-d" 'gdb-inferior-io-eof)

(defun gud-inferior-io-mode ()
  "Major mode for gud inferior-io.

\\{comint-mode-map}"
  ;; We want to use comint because it has various nifty and familiar
  ;; features.  We don't need a process, but comint wants one, so create
  ;; a dummy one.
  (make-comint (substring (buffer-name) 1 (- (length (buffer-name)) 1))
	       "/bin/cat")
  (setq major-mode 'gud-inferior-io-mode)
  (setq mode-name "Debuggee I/O")
  (setq comint-input-sender 'gud-inferior-io-sender)
)

(defun gud-inferior-io-sender (proc string)
  (save-excursion
    (set-buffer (process-buffer proc))
    (let ((instance gdb-buffer-instance))
      (set-buffer (gdb-get-instance-buffer instance 'gud))
      (let ((gud-proc (get-buffer-process (current-buffer))))
	(process-send-string gud-proc string)
	(process-send-string gud-proc "\n")
    ))
    ))

(defun gdb-inferior-io-interrupt (instance)
  "Interrupt the program being debugged."
  (interactive (list (gdb-needed-default-instance)))
  (interrupt-process
   (get-buffer-process (gdb-get-instance-buffer instance 'gud)) comint-ptyp))

(defun gdb-inferior-io-quit (instance)
  "Send quit signal to the program being debugged."
  (interactive (list (gdb-needed-default-instance)))
  (quit-process
   (get-buffer-process (gdb-get-instance-buffer instance 'gud)) comint-ptyp))

(defun gdb-inferior-io-stop (instance)
  "Stop the program being debugged."
  (interactive (list (gdb-needed-default-instance)))
  (stop-process
   (get-buffer-process (gdb-get-instance-buffer instance 'gud)) comint-ptyp))

(defun gdb-inferior-io-eof (instance)
  "Send end-of-file to the program being debugged."
  (interactive (list (gdb-needed-default-instance)))
  (process-send-eof
   (get-buffer-process (gdb-get-instance-buffer instance 'gud))))


;;
;; gdb communications
;;

;; INPUT: things sent to gdb
;;
;; Each instance has a high and low priority 
;; input queue.  Low priority input is sent only 
;; when the high priority queue is idle.
;;
;; The queues are lists.  Each element is either 
;; a string (indicating user or user-like input)
;; or a list of the form:
;;
;;    (INPUT-STRING  HANDLER-FN)
;;
;;
;; The handler function will be called from the 
;; partial-output buffer when the command completes.
;; This is the way to write commands which 
;; invoke gdb commands autonomously.
;;
;; These lists are consumed tail first.
;;

(defun gdb-send (proc string)
  "A comint send filter for gdb.
This filter may simply queue output for a later time."
  (let ((instance (gdb-proc->instance proc)))
    (gdb-instance-enqueue-input instance (concat string "\n"))))

;; Note: Stuff enqueued here will be sent to the next prompt, even if it
;; is a query, or other non-top-level prompt.  To guarantee stuff will get
;; sent to the top-level prompt, currently it must be put in the idle queue.
;;				 ^^^^^^^^^
;; [This should encourage gud extentions that invoke gdb commands to let
;;  the user go first; it is not a bug.     -t]
;;

(defun gdb-instance-enqueue-input (instance item)
  (if (gdb-instance-prompting instance)
      (progn
	(gdb-send-item instance item)
	(set-gdb-instance-prompting instance nil))
    (set-gdb-instance-input-queue
     instance
     (cons item (gdb-instance-input-queue instance)))))

(defun gdb-instance-dequeue-input (instance)
  (let ((queue (gdb-instance-input-queue instance)))
    (and queue
       (if (not (cdr queue))
	   (let ((answer (car queue)))
	     (set-gdb-instance-input-queue instance '())
	     answer)
	 (gdb-take-last-elt queue)))))

(defun gdb-instance-enqueue-idle-input (instance item)
  (if (and (gdb-instance-prompting instance)
	   (not (gdb-instance-input-queue instance)))
      (progn
	(gdb-send-item instance item)
	(set-gdb-instance-prompting instance nil))
    (set-gdb-instance-idle-input-queue
     instance
     (cons item (gdb-instance-idle-input-queue instance)))))

(defun gdb-instance-dequeue-idle-input (instance)
  (let ((queue (gdb-instance-idle-input-queue instance)))
    (and queue
       (if (not (cdr queue))
	   (let ((answer (car queue)))
	     (set-gdb-instance-idle-input-queue instance '())
	     answer)
	 (gdb-take-last-elt queue)))))

; Don't use this in general.
(defun gdb-take-last-elt (l)
  (if (cdr (cdr l))
      (gdb-take-last-elt (cdr l))
    (let ((answer (car (cdr l))))
      (setcdr l '())
      answer)))


;;
;; output -- things gdb prints to emacs
;;
;; GDB output is a stream interrupted by annotations.
;; Annotations can be recognized by their beginning
;; with \C-j\C-z\C-z<tag><opt>\C-j
;;
;; The tag is a string obeying symbol syntax.
;;
;; The optional part `<opt>' can be either the empty string
;; or a space followed by more data relating to the annotation.
;; For example, the SOURCE annotation is followed by a filename,
;; line number and various useless goo.  This data must not include
;; any newlines.
;;


(defun gud-gdb-marker-filter (string)
  "A gud marker filter for gdb."
  ;; Bogons don't tell us the process except through scoping crud.
  (let ((instance (gdb-proc->instance proc)))
    (gdb-output-burst instance string)))

(defvar gdb-annotation-rules
  '(("frames-invalid" gdb-invalidate-frames)
    ("breakpoints-invalid" gdb-invalidate-breakpoints)
    ("pre-prompt" gdb-pre-prompt)
    ("prompt" gdb-prompt)
    ("commands" gdb-subprompt)
    ("overload-choice" gdb-subprompt)
    ("query" gdb-subprompt)
    ("prompt-for-continue" gdb-subprompt)
    ("post-prompt" gdb-post-prompt)
    ("source" gdb-source)
    ("starting" gdb-starting)
    ("exited" gdb-stopping)
    ("signalled" gdb-stopping)
    ("signal" gdb-stopping)
    ("breakpoint" gdb-stopping)
    ("watchpoint" gdb-stopping)
    ("stopped" gdb-stopped)
    )
  "An assoc mapping annotation tags to functions which process them.")


(defun gdb-ignore-annotation (instance args)
  nil)

(defconst gdb-source-spec-regexp
  "\\(.*\\):\\([0-9]*\\):[0-9]*:[a-z]*:0x[a-f0-9]*")

;; Do not use this except as an annotation handler."
(defun gdb-source (instance args)
  (string-match gdb-source-spec-regexp args)
  ;; Extract the frame position from the marker.
  (setq gud-last-frame
	(cons
	 (substring args (match-beginning 1) (match-end 1))
	 (string-to-int (substring args
				   (match-beginning 2)
				   (match-end 2))))))

;; An annotation handler for `prompt'.
;; This sends the next command (if any) to gdb.
(defun gdb-prompt (instance ignored)
  (let ((sink (gdb-instance-output-sink instance)))
    (cond
     ((eq sink 'user) t)
     ((eq sink 'post-emacs)
      (set-gdb-instance-output-sink instance 'user))
     (t
      (set-gdb-instance-output-sink instance 'user)
      (error "Phase error in gdb-prompt (got %s)" sink))))
  (let ((highest (gdb-instance-dequeue-input instance)))
    (if highest
	(gdb-send-item instance highest)
      (let ((lowest (gdb-instance-dequeue-idle-input instance)))
	(if lowest
	    (gdb-send-item instance lowest)
	  (progn
	    (set-gdb-instance-prompting instance t)
	    (gud-display-frame)))))))

;; An annotation handler for non-top-level prompts.
(defun gdb-subprompt (instance ignored)
  (let ((highest (gdb-instance-dequeue-input instance)))
    (if highest
	(gdb-send-item instance highest)
      (set-gdb-instance-prompting instance t))))

(defun gdb-send-item (instance item)
  (set-gdb-instance-current-item instance item)
  (if (stringp item)
      (progn
	(set-gdb-instance-output-sink instance 'user)
	(process-send-string (gdb-instance-process instance)
			     item))
    (progn
      (gdb-clear-partial-output instance)
      (set-gdb-instance-output-sink instance 'pre-emacs)
      (process-send-string (gdb-instance-process instance)
			   (car item)))))

;; An annotation handler for `pre-prompt'.
;; This terminates the collection of output from a previous
;; command if that happens to be in effect.
(defun gdb-pre-prompt (instance ignored)
  (let ((sink (gdb-instance-output-sink instance)))
    (cond
     ((eq sink 'user) t)
     ((eq sink 'emacs)
      (set-gdb-instance-output-sink instance 'post-emacs)
      (let ((handler
	     (car (cdr (gdb-instance-current-item instance)))))
	(save-excursion
	  (set-buffer (gdb-get-create-instance-buffer
		       instance 'gdb-partial-output-buffer))
	  (funcall handler))))
     (t
      (set-gdb-instance-output-sink instance 'user)
      (error "Output sink phase error 1.")))))

;; An annotation handler for `starting'.  This says that I/O for the subprocess
;; is now the program being debugged, not GDB.
(defun gdb-starting (instance ignored)
  (let ((sink (gdb-instance-output-sink instance)))
    (cond
     ((eq sink 'user)
      (set-gdb-instance-output-sink instance 'inferior)
      ;; FIXME: need to send queued input
      )
     (t (error "Unexpected `starting' annotation")))))

;; An annotation handler for `exited' and other annotations which say that
;; I/O for the subprocess is now GDB, not the program being debugged.
(defun gdb-stopping (instance ignored)
  (let ((sink (gdb-instance-output-sink instance)))
    (cond
     ((eq sink 'inferior)
      (set-gdb-instance-output-sink instance 'user)
      )
     (t (error "Unexpected stopping annotation")))))

;; An annotation handler for `stopped'.  It is just like gdb-stopping, except
;; that if we already set the output sink to 'user in gdb-stopping, that is 
;; fine.
(defun gdb-stopped (instance ignored)
  (let ((sink (gdb-instance-output-sink instance)))
    (cond
     ((eq sink 'inferior)
      (set-gdb-instance-output-sink instance 'user)
      )
     ((eq sink 'user)
      t)
     (t (error "Unexpected stopping annotation")))))

;; An annotation handler for `post-prompt'.
;; This begins the collection of output from the current
;; command if that happens to be appropriate."
(defun gdb-post-prompt (instance ignored)
  (gdb-invalidate-registers instance ignored)
  (let ((sink (gdb-instance-output-sink instance)))
    (cond
     ((eq sink 'user) t)
     ((eq sink 'pre-emacs)
      (set-gdb-instance-output-sink instance 'emacs))

     (t
      (set-gdb-instance-output-sink instance 'user)
      (error "Output sink phase error 3.")))))

;; Handle a burst of output from a gdb instance.
;; This function is (indirectly) used as a gud-marker-filter.
;; It must return output (if any) to be insterted in the gud 
;; buffer.

(defun gdb-output-burst (instance string)
  "Handle a burst of output from a gdb instance.
This function is (indirectly) used as a gud-marker-filter.
It must return output (if any) to be insterted in the gud 
buffer."

  (save-match-data
    (let (
	  ;; Recall the left over burst from last time
	  (burst (concat (gdb-instance-burst instance) string))
	  ;; Start accumulating output for the gud buffer
	  (output ""))

      ;; Process all the complete markers in this chunk.

      (while (string-match "\n\032\032\\(.*\\)\n" burst)
	(let ((annotation (substring burst
				     (match-beginning 1)
				     (match-end 1))))
	    
	  ;; Stuff prior to the match is just ordinary output.
	  ;; It is either concatenated to OUTPUT or directed
	  ;; elsewhere.
	  (setq output
		(gdb-concat-output
		 instance
		 output
		 (substring burst 0 (match-beginning 0))))

	  ;; Take that stuff off the burst.
	  (setq burst (substring burst (match-end 0)))
	    
	  ;; Parse the tag from the annotation, and maybe its arguments.
	  (string-match "\\(\\S-*\\) ?\\(.*\\)" annotation)
	  (let* ((annotation-type (substring annotation
					     (match-beginning 1)
					     (match-end 1)))
		 (annotation-arguments (substring annotation
						  (match-beginning 2)
						  (match-end 2)))
		 (annotation-rule (assoc annotation-type
					 gdb-annotation-rules)))
	    ;; Call the handler for this annotation.
	    (if annotation-rule
		(funcall (car (cdr annotation-rule))
			 instance
			 annotation-arguments)
	      ;; Else the annotation is not recognized.  Ignore it silently,
	      ;; so that GDB can add new annotations without causing
	      ;; us to blow up.
	      ))))


      ;; Does the remaining text end in a partial line?
      ;; If it does, then keep part of the burst until we get more.
      (if (string-match "\n\\'\\|\n\032\\'\\|\n\032\032.*\\'"
			burst)
	  (progn
	    ;; Everything before the potential marker start can be output.
	    (setq output
		  (gdb-concat-output
		   instance
		   output
		   (substring burst 0 (match-beginning 0))))

	    ;; Everything after, we save, to combine with later input.
	    (setq burst (substring burst (match-beginning 0))))

	;; In case we know the burst contains no partial annotations:
	(progn
	  (setq output (gdb-concat-output instance output burst))
	  (setq burst "")))

      ;; Save the remaining burst for the next call to this function.
      (set-gdb-instance-burst instance burst)
      output)))

(defun gdb-concat-output (instance so-far new)
  (let ((sink (gdb-instance-output-sink instance)))
    (cond
     ((eq sink 'user) (concat so-far new))
     ((or (eq sink 'pre-emacs) (eq sink 'post-emacs)) so-far)
     ((eq sink 'emacs)
      (gdb-append-to-partial-output instance new)
      so-far)
     ((eq sink 'inferior)
      (gdb-append-to-inferior-io instance new)
      so-far)
     (t (error "Bogon output sink %S" sink)))))

(defun gdb-append-to-partial-output (instance string)
  (save-excursion
    (set-buffer
     (gdb-get-create-instance-buffer
      instance 'gdb-partial-output-buffer))
    (goto-char (point-max))
    (insert string)))

(defun gdb-clear-partial-output (instance)
  (save-excursion
    (set-buffer
     (gdb-get-create-instance-buffer
      instance 'gdb-partial-output-buffer))
    (delete-region (point-min) (point-max))))

(defun gdb-append-to-inferior-io (instance string)
  (save-excursion
    (set-buffer
     (gdb-get-create-instance-buffer
      instance 'gdb-inferior-io))
    (goto-char (point-max))
    (insert-before-markers string))
  (gud-display-buffer
   (gdb-get-create-instance-buffer instance
				   'gdb-inferior-io)))

(defun gdb-clear-inferior-io (instance)
  (save-excursion
    (set-buffer
     (gdb-get-create-instance-buffer
      instance 'gdb-inferior-io))
    (delete-region (point-min) (point-max))))



;; One trick is to have a command who's output is always available in
;; a buffer of it's own, and is always up to date.  We build several 
;; buffers of this type.
;;
;; There are two aspects to this: gdb has to tell us when the output
;; for that command might have changed, and we have to be able to run
;; the command behind the user's back.
;;
;; The idle input queue and the output phasing associated with 
;; the instance variable `(gdb-instance-output-sink instance)' help
;; us to run commands behind the user's back.
;; 
;; Below is the code for specificly managing buffers of output from one 
;; command.
;;


;; The trigger function is suitable for use in the assoc GDB-ANNOTATION-RULES
;; It adds an idle input for the command we are tracking.  It should be the
;; annotation rule binding of whatever gdb sends to tell us this command
;; might have changed it's output.
;;
;; NAME is the fucntion name.  DEMAND-PREDICATE tests if output is really needed.
;; GDB-COMMAND is a string of such.  OUTPUT-HANDLER is the function bound to the
;; input in the input queue (see comment about ``gdb communications'' above).
(defmacro def-gdb-auto-update-trigger (name demand-predicate gdb-command output-handler)
  (`
   (defun (, name) (instance &optional ignored)
     (if (and ((, demand-predicate) instance)
	      (not (member '(, name)
			   (gdb-instance-pending-triggers instance))))
	 (progn
	   (gdb-instance-enqueue-idle-input
	    instance
	    (list (, gdb-command) '(, output-handler)))
	   (set-gdb-instance-pending-triggers
	    instance
	    (cons '(, name)
		  (gdb-instance-pending-triggers instance))))))))
		
(defmacro def-gdb-auto-update-handler (name trigger buf-key)
  (`
   (defun (, name) ()
     (set-gdb-instance-pending-triggers
      instance
      (delq '(, trigger)
	    (gdb-instance-pending-triggers instance)))
     (let ((buf (gdb-get-instance-buffer instance
					  '(, buf-key))))
       (and buf
	    (save-excursion
	      (set-buffer buf)
	      (let ((p (point))
		    (buffer-read-only nil))
		(delete-region (point-min) (point-max))
		(insert-buffer (gdb-get-create-instance-buffer
				instance
				'gdb-partial-output-buffer))
		(goto-char p))))))))

(defmacro def-gdb-auto-updated-buffer
  (buffer-key trigger-name gdb-command output-handler-name)
  (`
   (progn
     (def-gdb-auto-update-trigger (, trigger-name)
       ;; The demand predicate:
       (lambda (instance)
	 (gdb-get-instance-buffer instance '(, buffer-key)))
       (, gdb-command)
       (, output-handler-name))
     (def-gdb-auto-update-handler (, output-handler-name)
       (, trigger-name) (, buffer-key)))))



;;
;; Breakpoint buffers
;; 
;; These display the output of `info breakpoints'.
;;

       
(gdb-set-instance-buffer-rules 'gdb-breakpoints-buffer
			       'gdb-breakpoints-buffer-name
			       'gud-breakpoints-mode)

(def-gdb-auto-updated-buffer gdb-breakpoints-buffer
  ;; This defines the auto update rule for buffers of type
  ;; `gdb-breakpoints-buffer'.
  ;;
  ;; It defines a function to serve as the annotation handler that
  ;; handles the `foo-invalidated' message.  That function is called:
  gdb-invalidate-breakpoints

  ;; To update the buffer, this command is sent to gdb.
  "server info breakpoints\n"

  ;; This also defines a function to be the handler for the output
  ;; from the command above.  That function will copy the output into
  ;; the appropriately typed buffer.  That function will be called:
  gdb-info-breakpoints-handler)

(defun gdb-breakpoints-buffer-name (instance)
  (save-excursion
    (set-buffer (process-buffer (gdb-instance-process instance)))
    (concat "*breakpoints of " (gdb-instance-target-string instance) "*")))

(defun gud-display-breakpoints-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-display-buffer
   (gdb-get-create-instance-buffer instance
				    'gdb-breakpoints-buffer)))

(defun gud-frame-breakpoints-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-frame-buffer
   (gdb-get-create-instance-buffer instance
				    'gdb-breakpoints-buffer)))

(defvar gud-breakpoints-mode-map nil)
(setq gud-breakpoints-mode-map (make-keymap))
(suppress-keymap gud-breakpoints-mode-map)
(define-key gud-breakpoints-mode-map " " 'gud-toggle-bp-this-line)
(define-key gud-breakpoints-mode-map "d" 'gud-delete-bp-this-line)

(defun gud-breakpoints-mode ()
  "Major mode for gud breakpoints.

\\{gud-breakpoints-mode-map}"
  (setq major-mode 'gud-breakpoints-mode)
  (setq mode-name "Breakpoints")
  (use-local-map gud-breakpoints-mode-map)
  (setq buffer-read-only t)
  (gdb-invalidate-breakpoints gdb-buffer-instance))

(defun gud-toggle-bp-this-line ()
  (interactive)
  (save-excursion
    (beginning-of-line 1)
    (if (not (looking-at "\\([0-9]*\\)\\s-*\\S-*\\s-*\\S-*\\s-*\\(.\\)"))
	(error "Not recognized as breakpoint line (demo foo).")
      (gdb-instance-enqueue-idle-input
       gdb-buffer-instance
       (list
	(concat
	 (if (eq ?y (char-after (match-beginning 2)))
	     "server disable "
	   "server enable ")
	 (buffer-substring (match-beginning 0)
			   (match-end 1))
	 "\n")
	'(lambda () nil)))
      )))

(defun gud-delete-bp-this-line ()
  (interactive)
  (save-excursion
    (beginning-of-line 1)
    (if (not (looking-at "\\([0-9]*\\)\\s-*\\S-*\\s-*\\S-*\\s-*\\(.\\)"))
	(error "Not recognized as breakpoint line (demo foo).")
      (gdb-instance-enqueue-idle-input
       gdb-buffer-instance
       (list
	(concat
	 "server delete "
	 (buffer-substring (match-beginning 0)
			   (match-end 1))
	 "\n")
	'(lambda () nil)))
      )))




;;
;; Frames buffers.  These display a perpetually correct bactracktrace
;; (from the command `where').
;;
;; Alas, if your stack is deep, they are costly.
;;

(gdb-set-instance-buffer-rules 'gdb-stack-buffer
			       'gdb-stack-buffer-name
			       'gud-frames-mode)

(def-gdb-auto-updated-buffer gdb-stack-buffer
  gdb-invalidate-frames
  "server where\n"
  gdb-info-frames-handler)

(defun gdb-stack-buffer-name (instance)
  (save-excursion
    (set-buffer (process-buffer (gdb-instance-process instance)))
    (concat "*stack frames of "
	    (gdb-instance-target-string instance) "*")))

(defun gud-display-stack-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-display-buffer
   (gdb-get-create-instance-buffer instance
				    'gdb-stack-buffer)))

(defun gud-frame-stack-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-frame-buffer
   (gdb-get-create-instance-buffer instance
				    'gdb-stack-buffer)))

(defvar gud-frames-mode-map nil)
(setq gud-frames-mode-map (make-keymap))
(suppress-keymap gud-frames-mode-map)
(define-key gud-frames-mode-map [mouse-2]
  'gud-frames-select-by-mouse)

(defun gud-frames-mode ()
  "Major mode for gud frames.

\\{gud-frames-mode-map}"
  (setq major-mode 'gud-frames-mode)
  (setq mode-name "Frames")
  (setq buffer-read-only t)
  (use-local-map gud-frames-mode-map)
  (gdb-invalidate-frames gdb-buffer-instance))

(defun gud-get-frame-number ()
  (save-excursion
    (let* ((pos (re-search-backward "^#\\([0-9]*\\)" nil t))
	   (n (or (and pos
		       (string-to-int
			(buffer-substring (match-beginning 1)
					  (match-end 1))))
		  0)))
      n)))

(defun gud-frames-select-by-mouse (e)
  (interactive "e")
  (let (selection)
    (save-excursion
      (set-buffer (window-buffer (posn-window (event-end e))))
      (save-excursion
	(goto-char (posn-point (event-end e)))
	(setq selection (gud-get-frame-number))))
    (select-window (posn-window (event-end e)))
    (save-excursion
      (set-buffer (gdb-get-instance-buffer (gdb-needed-default-instance) 'gud))
      (gud-call "fr %p" selection)
      (gud-display-frame))))


;;
;; Registers buffers
;;

(def-gdb-auto-updated-buffer gdb-registers-buffer
  gdb-invalidate-registers
  "server info registers\n"
  gdb-info-registers-handler)

(gdb-set-instance-buffer-rules 'gdb-registers-buffer
			       'gdb-registers-buffer-name
			       'gud-registers-mode)

(defvar gud-registers-mode-map nil)
(setq gud-registers-mode-map (make-keymap))
(suppress-keymap gud-registers-mode-map)

(defun gud-registers-mode ()
  "Major mode for gud registers.

\\{gud-registers-mode-map}"
  (setq major-mode 'gud-registers-mode)
  (setq mode-name "Registers")
  (setq buffer-read-only t)
  (use-local-map gud-registers-mode-map)
  (gdb-invalidate-registers gdb-buffer-instance))

(defun gdb-registers-buffer-name (instance)
  (save-excursion
    (set-buffer (process-buffer (gdb-instance-process instance)))
    (concat "*registers of " (gdb-instance-target-string instance) "*")))

(defun gud-display-registers-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-display-buffer
   (gdb-get-create-instance-buffer instance
				    'gdb-registers-buffer)))

(defun gud-frame-registers-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-frame-buffer
   (gdb-get-create-instance-buffer instance
				    'gdb-registers-buffer)))



;;;; Menu windows:


;; MENU-LIST is ((option option option...) (option option ...)...)
;; 
(defun gud-display-menu (menu-list)
  (setq fill-column (min 120 (- (window-width)
				(min 8 (window-width)))))
  (while menu-list
    (mapcar (function (lambda (x) (insert (symbol-name x) " "))) (car menu-list))
    (fill-paragraph nil)
    (insert "\n\n")
    (setq menu-list (cdr menu-list)))
  (goto-char (point-min))
  (while (re-search-forward "\\([^ \n]+\\)\\(\n\\| \\)" nil t)
    (put-text-property (match-beginning 1) (match-end 1)
		       'mouse-face 'highlight))
  (goto-char (point-min)))

(defun gud-goto-menu (menu)
  (setq gud-menu-position menu)
  (let ((buffer-read-only nil))
    (delete-region (point-min) (point-max))
    (gud-display-menu menu)))

(defun gud-menu-pick (event)
  "Choose an item from a gdb command menu."
  (interactive "e")
  (let (choice)
    (save-excursion
      (set-buffer (window-buffer (posn-window (event-start event))))
      (goto-char (posn-point (event-start event)))
      (let (beg end)
	(skip-chars-forward "^ \t\n")
	(setq end (point))
	(skip-chars-backward "^ \t\n")
	(setq beg (point))
	(setq choice (buffer-substring beg end))
	(message choice)
	(gud-invoke-menu (intern choice))))))

(defun gud-invoke-menu (symbol)
  (let ((meaning (assoc symbol gud-menu-rules)))
    (cond
     ((and (consp meaning)
	   (consp (car (cdr meaning))))
      (gud-goto-menu (car (cdr meaning))))
     (meaning (call-interactively (car (cdr meaning)))))))



(gdb-set-instance-buffer-rules 'gdb-command-buffer
			       'gdb-command-buffer-name
			       'gud-command-mode)

(defvar gud-command-mode-map nil)
(setq gud-command-mode-map (make-keymap))
(suppress-keymap gud-command-mode-map)
(define-key gud-command-mode-map [mouse-2] 'gud-menu-pick)

(defun gud-command-mode ()
  "Major mode for gud menu.

\\{gud-command-mode-map}" (interactive) (setq major-mode 'gud-command-mode)
  (setq mode-name "Menu") (setq buffer-read-only t) (use-local-map
  gud-command-mode-map) (make-variable-buffer-local 'gud-menu-position)
  (if (not gud-menu-position) (gud-goto-menu gud-running-menu)))

(defun gdb-command-buffer-name (instance)
  (save-excursion
    (set-buffer (process-buffer (gdb-instance-process instance)))
    (concat "*menu of " (gdb-instance-target-string instance) "*")))

(defun gud-display-command-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-display-buffer
   (gdb-get-create-instance-buffer instance
				   'gdb-command-buffer)
   6))

(defun gud-frame-command-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-frame-buffer
   (gdb-get-create-instance-buffer instance
				    'gdb-command-buffer)))

(defvar gud-selected-menu-titles ())
(setq gud-selected-menu-titles 
      '(RUNNING STACK DATA BREAKPOINTS FILES))  

(setq gud-running-menu
  (list
   '(RUNNING stack breakpoints files)
   '(target run next step continue finish stepi kill help-running)))
  
(setq gud-stack-menu
  (list
   '(running STACK breakpoints files)
   '(up down frame backtrace return help-stack)))
  
(setq gud-data-menu
  (list
   '(running stack DATA breakpoints files)
   '(whatis ptype print set display undisplay disassemble help-data)))

(setq gud-breakpoints-menu
  (list
   '(running stack BREAKPOINTS files)
   '(awatch rwatch watch break delete enable disable condition ignore help-breakpoints)))
  
(setq gud-files-menu
  (list
   '(running stack breakpoints FILES)
   '(file core-file help-files)
   '(exec-file load symbol-file add-symbol-file sharedlibrary)))

(setq gud-menu-rules
      (list
       (list 'running gud-running-menu)
       (list 'RUNNING gud-running-menu)
       (list 'stack gud-stack-menu)
       (list 'STACK gud-stack-menu)
       (list 'data gud-data-menu)
       (list 'DATA gud-data-menu)
       (list 'breakpoints gud-breakpoints-menu)
       (list 'BREAKPOINTS gud-breakpoints-menu)
       (list 'files gud-files-menu)
       (list 'FILES gud-files-menu)

       (list 'target 'gud-target)
       (list 'kill 'gud-kill)
       (list 'stepi 'gud-stepi)
       (list 'step 'gud-step)
       (list 'next 'gud-next)
       (list 'finish 'gud-finish)
       (list 'continue 'gud-cont)
       (list 'run 'gud-run)

       (list 'backtrace 'gud-backtrace)
       (list 'frame 'gud-frame)
       (list 'down 'gud-down)
       (list 'up 'gud-up)
       (list 'return 'gud-return)

       (list 'file 'gud-file)
       (list 'core-file 'gud-core-file)
       (list 'cd 'gud-cd)

       (list 'exec-file 'gud-exec-file)
       (list 'load 'gud-load)
       (list 'symbol-file 'gud-symbol-file)
       (list 'add-symbol-file 'gud-add-symbol-file)
       (list 'sharedlibrary 'gud-sharedlibrary)
       ))
       



(defun gdb-call-showing-gud (instance command)
  (gud-display-gud-buffer instance)
  (comint-input-sender (gdb-instance-process instance) command))

(defvar gud-target-history ())

(defun gud-temp-buffer-show (buf)
  (let ((ow (selected-window)))
    (unwind-protect
	(progn
	  (pop-to-buffer buf)

	  ;; This insertion works around a bug in emacs.
	  ;; The bug is that all the empty space after a
	  ;; highlighted word that terminates a buffer
	  ;; gets highlighted.  That's really ugly, so
	  ;; make sure a highlighted word can't ever
	  ;; terminate the buffer.
	  (goto-char (point-max))
	  (insert "\n")
	  (goto-char (point-min))

	  (if (< (window-height) 10)
	      (enlarge-window (- 10 (window-height)))))
      (select-window ow))))

(defun gud-target (instance command)
  (interactive 
   (let* ((instance (gdb-needed-default-instance))
	  (temp-buffer-show-function (function gud-temp-buffer-show))
	  (target-name (completing-read (format "Target type: ")
					'(("remote")
					  ("core")
					  ("child")
					  ("exec"))
					nil
					t
					nil
					'gud-target-history)))
     (list instance
	   (cond
	    ((equal target-name "child") "run")

	    ((equal target-name "core")
	     (concat "target core "
		     (read-file-name "core file: "
				     nil
				     "core"
				     t)))

	    ((equal target-name "exec")
	     (concat "target exec "
		     (read-file-name "exec file: "
				     nil
				     "a.out"
				     t)))

	    ((equal target-name "remote")
	     (concat "target remote "
		     (read-file-name "serial line for remote: "
				     "/dev/"
				     "ttya"
				     t)))

	    (t "echo No such target command!")))))

  (gud-display-gud-buffer instance)
  (apply comint-input-sender
	 (list (gdb-instance-process instance) command)))

(defun gud-backtrace ()
  (interactive)
  (let ((instance  (gdb-needed-default-instance)))
    (gud-display-gud-buffer instance)
    (apply comint-input-sender
	   (list (gdb-instance-process instance)
		 "backtrace"))))

(defun gud-frame ()
  (interactive)
  (let ((instance  (gdb-needed-default-instance)))
    (apply comint-input-sender
	   (list (gdb-instance-process instance)
		 "frame"))))

(defun gud-return (instance command)
   (interactive
    (let ((temp-buffer-show-function (function gud-temp-buffer-show)))
      (list (gdb-needed-default-instance)
	    (concat "return " (read-string "Expression to return: ")))))
   (gud-display-gud-buffer instance)
   (apply comint-input-sender
	  (list (gdb-instance-process instance) command)))


(defun gud-file (instance command)
  (interactive
   (let ((temp-buffer-show-function (function gud-temp-buffer-show)))
     (list (gdb-needed-default-instance)
	   (concat "file " (read-file-name "Executable to debug: "
					   nil
					   "a.out"
					   t)))))
  (gud-display-gud-buffer instance)
  (apply comint-input-sender
	 (list (gdb-instance-process instance) command)))

(defun gud-core-file (instance command)
  (interactive
   (let ((temp-buffer-show-function (function gud-temp-buffer-show)))
     (list (gdb-needed-default-instance)
	   (concat "core " (read-file-name "Core file to debug: "
					   nil
					   "core-file"
					   t)))))
  (gud-display-gud-buffer instance)
  (apply comint-input-sender
	 (list (gdb-instance-process instance) command)))

(defun gud-cd (dir)
  (interactive "FChange GDB's default directory: ")
  (let ((instance (gdb-needed-default-instance)))
    (save-excursion
      (set-buffer (gdb-get-instance-buffer instance 'gud))
      (cd dir))
    (gud-display-gud-buffer instance)
    (apply comint-input-sender
	   (list (gdb-instance-process instance)
		 (concat "cd " dir)))))


(defun gud-exec-file (instance command)
  (interactive
   (let ((temp-buffer-show-function (function gud-temp-buffer-show)))
     (list (gdb-needed-default-instance)
	   (concat "exec-file " (read-file-name "Init memory from executable: "
						nil
						"a.out"
						t)))))
  (gud-display-gud-buffer instance)
  (apply comint-input-sender
	 (list (gdb-instance-process instance) command)))

(defun gud-load (instance command)
  (interactive
   (let ((temp-buffer-show-function (function gud-temp-buffer-show)))
     (list (gdb-needed-default-instance)
	   (concat "load " (read-file-name "Dynamicly load from file: "
					   nil
					   "a.out"
					   t)))))
  (gud-display-gud-buffer instance)
  (apply comint-input-sender
	 (list (gdb-instance-process instance) command)))

(defun gud-symbol-file (instance command)
  (interactive
   (let ((temp-buffer-show-function (function gud-temp-buffer-show)))
     (list (gdb-needed-default-instance)
	   (concat "symbol-file " (read-file-name "Read symbol table from file: "
						  nil
						  "a.out"
						  t)))))
  (gud-display-gud-buffer instance)
  (apply comint-input-sender
	 (list (gdb-instance-process instance) command)))


(defun gud-add-symbol-file (instance command)
  (interactive
   (let ((temp-buffer-show-function (function gud-temp-buffer-show)))
     (list (gdb-needed-default-instance)
	   (concat "add-symbol-file "
		   (read-file-name "Add symbols from file: "
				   nil
				   "a.out"
				   t)))))
  (gud-display-gud-buffer instance)
  (apply comint-input-sender
	 (list (gdb-instance-process instance) command)))


(defun gud-sharedlibrary (instance command)
  (interactive
   (let ((temp-buffer-show-function (function gud-temp-buffer-show)))
     (list (gdb-needed-default-instance)
	   (concat "sharedlibrary "
		   (read-string "Load symbols for files matching regexp: ")))))
  (gud-display-gud-buffer instance)
  (apply comint-input-sender
	 (list (gdb-instance-process instance) command)))





;;;; Window management


;;; FIXME: This should only return true for buffers in the current instance
(defun gud-protected-buffer-p (buffer)
  "Is BUFFER a buffer which we want to leave displayed?"
  (save-excursion
    (set-buffer buffer)
    (or gdb-buffer-type
	overlay-arrow-position)))

;;; The way we abuse the dedicated-p flag is pretty gross, but seems
;;; to do the right thing.  Seeing as there is no way for Lisp code to
;;; get at the use_time field of a window, I'm not sure there exists a
;;; more elegant solution without writing C code.

(defun gud-display-buffer (buf &optional size)
  (let ((must-split nil)
	(answer nil))
    (unwind-protect
	(progn
	  (walk-windows
	   '(lambda (win)
	      (if (gud-protected-buffer-p (window-buffer win))
		  (set-window-dedicated-p win t))))
	  (setq answer (get-buffer-window buf))
	  (if (not answer)
	      (let ((window (get-lru-window)))
		(if window
		    (progn
		      (set-window-buffer window buf)
		      (setq answer window))
		  (setq must-split t)))))
      (walk-windows
       '(lambda (win)
	  (if (gud-protected-buffer-p (window-buffer win))
	      (set-window-dedicated-p win nil)))))
    (if must-split
	(let* ((largest (get-largest-window))
	       (cur-size (window-height largest))
	       (new-size (and size (< size cur-size) (- cur-size size))))
	  (setq answer (split-window largest new-size))
	  (set-window-buffer answer buf)))
    answer))

(defun existing-source-window (buffer)
  (catch 'found
    (save-excursion
      (walk-windows
       (function
	(lambda (win)
	  (if (and overlay-arrow-position
		   (eq (window-buffer win)
		       (marker-buffer overlay-arrow-position)))
	      (progn
		(set-window-buffer win buffer)
		(throw 'found win))))))
      nil)))
      
(defun gud-display-source-buffer (buffer)
  (or (existing-source-window buffer)
      (gud-display-buffer buffer)))

(defun gud-frame-buffer (buf)
  (save-excursion
    (set-buffer buf)
    (make-frame)))



;;; Shared keymap initialization:

(defun make-windows-menu (map)
  (define-key map [menu-bar displays]
    (cons "GDB-Windows" (make-sparse-keymap "GDB-Windows")))
  (define-key map [menu-bar displays gdb]
    '("Gdb" . gud-display-gud-buffer))
  (define-key map [menu-bar displays registers]
    '("Registers" . gud-display-registers-buffer))
  (define-key map [menu-bar displays frames]
    '("Stack" . gud-display-stack-buffer))
  (define-key map [menu-bar displays breakpoints]
    '("Breakpoints" . gud-display-breakpoints-buffer))
  (define-key map [menu-bar displays commands]
    '("Commands" . gud-display-command-buffer)))

(defun gud-display-gud-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-display-buffer
   (gdb-get-create-instance-buffer instance 'gud)))

(make-windows-menu gud-breakpoints-mode-map)
(make-windows-menu gud-frames-mode-map)
(make-windows-menu gud-registers-mode-map)



(defun make-frames-menu (map)
  (define-key map [menu-bar frames]
    (cons "GDB-Frames" (make-sparse-keymap "GDB-Frames")))
  (define-key map [menu-bar frames gdb]
    '("Gdb" . gud-frame-gud-buffer))
  (define-key map [menu-bar frames registers]
    '("Registers" . gud-frame-registers-buffer))
  (define-key map [menu-bar frames frames]
    '("Stack" . gud-frame-stack-buffer))
  (define-key map [menu-bar frames breakpoints]
    '("Breakpoints" . gud-frame-breakpoints-buffer))
  (define-key map [menu-bar displays commands]
    '("Commands" . gud-display-command-buffer)))

(defun gud-frame-gud-buffer (instance)
  (interactive (list (gdb-needed-default-instance)))
  (gud-frame-buffer
   (gdb-get-create-instance-buffer instance 'gud)))

(make-frames-menu gud-breakpoints-mode-map)
(make-frames-menu gud-frames-mode-map)
(make-frames-menu gud-registers-mode-map)


(defun gud-gdb-find-file (f)
  (find-file-noselect f))

;;;###autoload
(defun gdb (command-line)
  "Run gdb on program FILE in buffer *gud-FILE*.
The directory containing FILE becomes the initial working directory
and source-file directory for your debugger."
  (interactive
   (list (read-from-minibuffer "Run gdb (like this): "
			       (if (consp gud-gdb-history)
				   (car gud-gdb-history)
				 "gdb ")
			       nil nil
			       '(gud-gdb-history . 1))))
  (gud-overload-functions
   '((gud-massage-args . gud-gdb-massage-args)
     (gud-marker-filter . gud-gdb-marker-filter)
     (gud-find-file . gud-gdb-find-file)
     ))

  (let* ((words (gud-chop-words command-line))
	 (program (car words))
	 (file-word (let ((w (cdr words)))
		      (while (and w (= ?- (aref (car w) 0)))
			(setq w (cdr w)))
		      (car w)))
	 (args (delq file-word (cdr words)))
	 (file (expand-file-name file-word))
	 (filepart (file-name-nondirectory file))
	 (buffer-name (concat "*gud-" filepart "*")))
    (setq gdb-first-time (not (get-buffer-process buffer-name))))

  (gud-common-init command-line)

  (gud-def gud-break  "break %f:%l"  "\C-b" "Set breakpoint at current line.")
  (gud-def gud-tbreak "tbreak %f:%l" "\C-t" "Set breakpoint at current line.")
  (gud-def gud-remove "clear %l"     "\C-d" "Remove breakpoint at current line")
  (gud-def gud-kill   "kill"	     nil    "Kill the program.")
  (gud-def gud-run    "run"	     nil    "Run the program.")
  (gud-def gud-stepi  "stepi %p"     "\C-i" "Step one instruction with display.")
  (gud-def gud-step   "step %p"      "\C-s" "Step one source line with display.")
  (gud-def gud-next   "next %p"      "\C-n" "Step one line (skip functions).")
  (gud-def gud-finish "finish"       "\C-f" "Finish executing current function.")
  (gud-def gud-cont   "cont"         "\C-r" "Continue with display.")
  (gud-def gud-up     "up %p"        "<" "Up N stack frames (numeric arg).")
  (gud-def gud-down   "down %p"      ">" "Down N stack frames (numeric arg).")
  (gud-def gud-print  "print %e"     "\C-p" "Evaluate C expression at point.")

  (setq comint-prompt-regexp "^(.*gdb[+]?) *")
  (setq comint-input-sender 'gdb-send)
  (run-hooks 'gdb-mode-hook)
  (let ((instance
	 (make-gdb-instance (get-buffer-process (current-buffer)))
	 ))
    (if gdb-first-time (gdb-clear-inferior-io instance)))
  )


;; ======================================================================
;; sdb functions

;;; History of argument lists passed to sdb.
(defvar gud-sdb-history nil)

(defvar gud-sdb-needs-tags (not (file-exists-p "/var"))
  "If nil, we're on a System V Release 4 and don't need the tags hack.")

(defvar gud-sdb-lastfile nil)

(defun gud-sdb-massage-args (file args)
  (cons file args))

(defun gud-sdb-marker-filter (string)
  (cond 
   ;; System V Release 3.2 uses this format
   ((string-match "\\(^0x\\w* in \\|^\\|\n\\)\\([^:\n]*\\):\\([0-9]*\\):.*\n"
		    string)
    (setq gud-last-frame
	  (cons
	   (substring string (match-beginning 2) (match-end 2))
	   (string-to-int 
	    (substring string (match-beginning 3) (match-end 3))))))
   ;; System V Release 4.0 
   ((string-match "^\\(BREAKPOINT\\|STEPPED\\) process [0-9]+ function [^ ]+ in \\(.+\\)\n"
		       string)
    (setq gud-sdb-lastfile
	  (substring string (match-beginning 2) (match-end 2))))
   ((and gud-sdb-lastfile (string-match "^\\([0-9]+\\):" string))
	 (setq gud-last-frame
	       (cons
		gud-sdb-lastfile
		(string-to-int 
		 (substring string (match-beginning 1) (match-end 1))))))
   (t 
    (setq gud-sdb-lastfile nil)))
  string)

(defun gud-sdb-find-file (f)
  (if gud-sdb-needs-tags
      (find-tag-noselect f)
    (find-file-noselect f)))

;;;###autoload
(defun sdb (command-line)
  "Run sdb on program FILE in buffer *gud-FILE*.
The directory containing FILE becomes the initial working directory
and source-file directory for your debugger."
  (interactive
   (list (read-from-minibuffer "Run sdb (like this): "
			       (if (consp gud-sdb-history)
				   (car gud-sdb-history)
				 "sdb ")
			       nil nil
			       '(gud-sdb-history . 1))))
  (if (and gud-sdb-needs-tags
	   (not (and (boundp 'tags-file-name) (file-exists-p tags-file-name))))
      (error "The sdb support requires a valid tags table to work."))
  (gud-overload-functions '((gud-massage-args . gud-sdb-massage-args)
			    (gud-marker-filter . gud-sdb-marker-filter)
			    (gud-find-file . gud-sdb-find-file)
			    ))

  (gud-common-init command-line)

  (gud-def gud-break  "%l b" "\C-b"   "Set breakpoint at current line.")
  (gud-def gud-tbreak "%l c" "\C-t"   "Set temporary breakpoint at current line.")
  (gud-def gud-remove "%l d" "\C-d"   "Remove breakpoint at current line")
  (gud-def gud-step   "s %p" "\C-s"   "Step one source line with display.")
  (gud-def gud-stepi  "i %p" "\C-i"   "Step one instruction with display.")
  (gud-def gud-next   "S %p" "\C-n"   "Step one line (skip functions).")
  (gud-def gud-cont   "c"    "\C-r"   "Continue with display.")
  (gud-def gud-print  "%e/"  "\C-p"   "Evaluate C expression at point.")

  (setq comint-prompt-regexp  "\\(^\\|\n\\)\\*")
  (run-hooks 'sdb-mode-hook)
  )

;; ======================================================================
;; dbx functions

;;; History of argument lists passed to dbx.
(defvar gud-dbx-history nil)

(defun gud-dbx-massage-args (file args)
  (cons file args))

(defun gud-dbx-marker-filter (string)
  (if (or (string-match
         "stopped in .* at line \\([0-9]*\\) in file \"\\([^\"]*\\)\""
         string)
        (string-match
         "signal .* in .* at line \\([0-9]*\\) in file \"\\([^\"]*\\)\""
         string))
      (setq gud-last-frame
	    (cons
	     (substring string (match-beginning 2) (match-end 2))
	     (string-to-int 
	      (substring string (match-beginning 1) (match-end 1))))))
  string)

(defun gud-dbx-find-file (f)
  (find-file-noselect f))

;;;###autoload
(defun dbx (command-line)
  "Run dbx on program FILE in buffer *gud-FILE*.
The directory containing FILE becomes the initial working directory
and source-file directory for your debugger."
  (interactive
   (list (read-from-minibuffer "Run dbx (like this): "
			       (if (consp gud-dbx-history)
				   (car gud-dbx-history)
				 "dbx ")
			       nil nil
			       '(gud-dbx-history . 1))))
  (gud-overload-functions '((gud-massage-args . gud-dbx-massage-args)
			    (gud-marker-filter . gud-dbx-marker-filter)
			    (gud-find-file . gud-dbx-find-file)
			    ))

  (gud-common-init command-line)

  (gud-def gud-break  "file \"%d%f\"\nstop at %l"
	   			  "\C-b" "Set breakpoint at current line.")
;;  (gud-def gud-break  "stop at \"%f\":%l"
;;	   			  "\C-b" "Set breakpoint at current line.")
  (gud-def gud-remove "clear %l"  "\C-d" "Remove breakpoint at current line")
  (gud-def gud-step   "step %p"	  "\C-s" "Step one line with display.")
  (gud-def gud-stepi  "stepi %p"  "\C-i" "Step one instruction with display.")
  (gud-def gud-next   "next %p"	  "\C-n" "Step one line (skip functions).")
  (gud-def gud-cont   "cont"	  "\C-r" "Continue with display.")
  (gud-def gud-up     "up %p"	  "<" "Up (numeric arg) stack frames.")
  (gud-def gud-down   "down %p"	  ">" "Down (numeric arg) stack frames.")
  (gud-def gud-print  "print %e"  "\C-p" "Evaluate C expression at point.")

  (setq comint-prompt-regexp  "^[^)]*dbx) *")
  (run-hooks 'dbx-mode-hook)
  )

;; ======================================================================
;; xdb (HP PARISC debugger) functions

;;; History of argument lists passed to xdb.
(defvar gud-xdb-history nil)

(defvar gud-xdb-directories nil
  "*A list of directories that xdb should search for source code.
If nil, only source files in the program directory
will be known to xdb.

The file names should be absolute, or relative to the directory
containing the executable being debugged.")

(defun gud-xdb-massage-args (file args)
  (nconc (let ((directories gud-xdb-directories)
	       (result nil))
	   (while directories
	     (setq result (cons (car directories) (cons "-d" result)))
	     (setq directories (cdr directories)))
	   (nreverse (cons file result)))
	 args))

(defun gud-xdb-file-name (f)
  "Transform a relative pathname to a full pathname in xdb mode"
  (let ((result nil))
    (if (file-exists-p f)
        (setq result (expand-file-name f))
      (let ((directories gud-xdb-directories))
        (while directories
          (let ((path (concat (car directories) "/" f)))
            (if (file-exists-p path)
                (setq result (expand-file-name path)
                      directories nil)))
          (setq directories (cdr directories)))))
    result))

;; xdb does not print the lines all at once, so we have to accumulate them
(defvar gud-xdb-accumulation "")

(defun gud-xdb-marker-filter (string)
  (let (result)
    (if (or (string-match comint-prompt-regexp string)
            (string-match ".*\012" string))
        (setq result (concat gud-xdb-accumulation string)
              gud-xdb-accumulation "")
      (setq gud-xdb-accumulation (concat gud-xdb-accumulation string)))
    (if result
        (if (or (string-match "\\([^\n \t:]+\\): [^:]+: \\([0-9]+\\):" result)
                (string-match "[^: \t]+:[ \t]+\\([^:]+\\): [^:]+: \\([0-9]+\\):"
                              result))
            (let ((line (string-to-int 
                         (substring result (match-beginning 2) (match-end 2))))
                  (file (gud-xdb-file-name
                         (substring result (match-beginning 1) (match-end 1)))))
              (if file
                  (setq gud-last-frame (cons file line))))))
    (or result "")))    
               
(defun gud-xdb-find-file (f)
  (let ((realf (gud-xdb-file-name f)))
    (if realf (find-file-noselect realf))))

;;;###autoload
(defun xdb (command-line)
  "Run xdb on program FILE in buffer *gud-FILE*.
The directory containing FILE becomes the initial working directory
and source-file directory for your debugger.

You can set the variable 'gud-xdb-directories' to a list of program source
directories if your program contains sources from more than one directory."
  (interactive
   (list (read-from-minibuffer "Run xdb (like this): "
			       (if (consp gud-xdb-history)
				   (car gud-xdb-history)
				 "xdb ")
			       nil nil
			       '(gud-xdb-history . 1))))
  (gud-overload-functions '((gud-massage-args . gud-xdb-massage-args)
			    (gud-marker-filter . gud-xdb-marker-filter)
			    (gud-find-file . gud-xdb-find-file)))

  (gud-common-init command-line)

  (gud-def gud-break  "b %f:%l"    "\C-b" "Set breakpoint at current line.")
  (gud-def gud-tbreak "b %f:%l\\t" "\C-t"
           "Set temporary breakpoint at current line.")
  (gud-def gud-remove "db"         "\C-d" "Remove breakpoint at current line")
  (gud-def gud-step   "s %p"	   "\C-s" "Step one line with display.")
  (gud-def gud-next   "S %p"	   "\C-n" "Step one line (skip functions).")
  (gud-def gud-cont   "c"	   "\C-r" "Continue with display.")
  (gud-def gud-up     "up %p"	   "<"    "Up (numeric arg) stack frames.")
  (gud-def gud-down   "down %p"	   ">"    "Down (numeric arg) stack frames.")
  (gud-def gud-finish "bu\\t"      "\C-f" "Finish executing current function.")
  (gud-def gud-print  "p %e"       "\C-p" "Evaluate C expression at point.")

  (setq comint-prompt-regexp  "^>")
  (make-local-variable 'gud-xdb-accumulation)
  (setq gud-xdb-accumulation "")
  (run-hooks 'xdb-mode-hook))

;; ======================================================================
;; perldb functions

;;; History of argument lists passed to perldb.
(defvar gud-perldb-history nil)

(defun gud-perldb-massage-args (file args)
  (cons "-d" (cons file (cons "-emacs" args))))

;; There's no guarantee that Emacs will hand the filter the entire
;; marker at once; it could be broken up across several strings.  We
;; might even receive a big chunk with several markers in it.  If we
;; receive a chunk of text which looks like it might contain the
;; beginning of a marker, we save it here between calls to the
;; filter.
(defvar gud-perldb-marker-acc "")

(defun gud-perldb-marker-filter (string)
  (save-match-data
    (setq gud-perldb-marker-acc (concat gud-perldb-marker-acc string))
    (let ((output ""))

      ;; Process all the complete markers in this chunk.
      (while (string-match "^\032\032\\([^:\n]*\\):\\([0-9]*\\):.*\n"
			   gud-perldb-marker-acc)
	(setq

	 ;; Extract the frame position from the marker.
	 gud-last-frame
	 (cons (substring gud-perldb-marker-acc (match-beginning 1) (match-end 1))
	       (string-to-int (substring gud-perldb-marker-acc
					 (match-beginning 2)
					 (match-end 2))))

	 ;; Append any text before the marker to the output we're going
	 ;; to return - we don't include the marker in this text.
	 output (concat output
			(substring gud-perldb-marker-acc 0 (match-beginning 0)))

	 ;; Set the accumulator to the remaining text.
	 gud-perldb-marker-acc (substring gud-perldb-marker-acc (match-end 0))))

      ;; Does the remaining text look like it might end with the
      ;; beginning of another marker?  If it does, then keep it in
      ;; gud-perldb-marker-acc until we receive the rest of it.  Since we
      ;; know the full marker regexp above failed, it's pretty simple to
      ;; test for marker starts.
      (if (string-match "^\032.*\\'" gud-perldb-marker-acc)
	  (progn
	    ;; Everything before the potential marker start can be output.
	    (setq output (concat output (substring gud-perldb-marker-acc
						   0 (match-beginning 0))))

	    ;; Everything after, we save, to combine with later input.
	    (setq gud-perldb-marker-acc
		  (substring gud-perldb-marker-acc (match-beginning 0))))

	(setq output (concat output gud-perldb-marker-acc)
	      gud-perldb-marker-acc ""))

      output)))

(defun gud-perldb-find-file (f)
  (find-file-noselect f))

;;;###autoload
(defun perldb (command-line)
  "Run perldb on program FILE in buffer *gud-FILE*.
The directory containing FILE becomes the initial working directory
and source-file directory for your debugger."
  (interactive
   (list (read-from-minibuffer "Run perldb (like this): "
			       (if (consp gud-perldb-history)
				   (car gud-perldb-history)
				 "perl ")
			       nil nil
			       '(gud-perldb-history . 1))))
  (gud-overload-functions '((gud-massage-args . gud-perldb-massage-args)
			    (gud-marker-filter . gud-perldb-marker-filter)
			    (gud-find-file . gud-perldb-find-file)
			    ))

  (gud-common-init command-line)

  (gud-def gud-break  "b %l"         "\C-b" "Set breakpoint at current line.")
  (gud-def gud-remove "d %l"         "\C-d" "Remove breakpoint at current line")
  (gud-def gud-step   "s"            "\C-s" "Step one source line with display.")
  (gud-def gud-next   "n"            "\C-n" "Step one line (skip functions).")
  (gud-def gud-cont   "c"            "\C-r" "Continue with display.")
;  (gud-def gud-finish "finish"       "\C-f" "Finish executing current function.")
;  (gud-def gud-up     "up %p"        "<" "Up N stack frames (numeric arg).")
;  (gud-def gud-down   "down %p"      ">" "Down N stack frames (numeric arg).")
  (gud-def gud-print  "%e"           "\C-p" "Evaluate perl expression at point.")

  (setq comint-prompt-regexp "^  DB<[0-9]+> ")
  (run-hooks 'perldb-mode-hook)
  )

;;
;; End of debugger-specific information
;;


;;; When we send a command to the debugger via gud-call, it's annoying
;;; to see the command and the new prompt inserted into the debugger's
;;; buffer; we have other ways of knowing the command has completed.
;;;
;;; If the buffer looks like this:
;;; --------------------
;;; (gdb) set args foo bar
;;; (gdb) -!-
;;; --------------------
;;; (the -!- marks the location of point), and we type `C-x SPC' in a
;;; source file to set a breakpoint, we want the buffer to end up like
;;; this:
;;; --------------------
;;; (gdb) set args foo bar
;;; Breakpoint 1 at 0x92: file make-docfile.c, line 49.
;;; (gdb) -!-
;;; --------------------
;;; Essentially, the old prompt is deleted, and the command's output
;;; and the new prompt take its place.
;;;
;;; Not echoing the command is easy enough; you send it directly using
;;; comint-input-sender, and it never enters the buffer.  However,
;;; getting rid of the old prompt is trickier; you don't want to do it
;;; when you send the command, since that will result in an annoying
;;; flicker as the prompt is deleted, redisplay occurs while Emacs
;;; waits for a response from the debugger, and the new prompt is
;;; inserted.  Instead, we'll wait until we actually get some output
;;; from the subprocess before we delete the prompt.  If the command
;;; produced no output other than a new prompt, that prompt will most
;;; likely be in the first chunk of output received, so we will delete
;;; the prompt and then replace it with an identical one.  If the
;;; command produces output, the prompt is moving anyway, so the
;;; flicker won't be annoying.
;;;
;;; So - when we want to delete the prompt upon receipt of the next
;;; chunk of debugger output, we position gud-delete-prompt-marker at
;;; the start of the prompt; the process filter will notice this, and
;;; delete all text between it and the process output marker.  If
;;; gud-delete-prompt-marker points nowhere, we leave the current
;;; prompt alone.
(defvar gud-delete-prompt-marker nil)


(defvar gdbish-comint-mode-map (copy-keymap comint-mode-map))
(define-key gdbish-comint-mode-map "\C-c\M-\C-r" 'gud-display-registers-buffer)
(define-key gdbish-comint-mode-map "\C-c\M-\C-f" 'gud-display-stack-buffer)
(define-key gdbish-comint-mode-map "\C-c\M-\C-b" 'gud-display-breakpoints-buffer)

(make-windows-menu gdbish-comint-mode-map)
(make-frames-menu gdbish-comint-mode-map)

(defun gud-mode ()
  "Major mode for interacting with an inferior debugger process.

   You start it up with one of the commands M-x gdb, M-x sdb, M-x dbx,
or M-x xdb.  Each entry point finishes by executing a hook; `gdb-mode-hook',
`sdb-mode-hook', `dbx-mode-hook' or `xdb-mode-hook' respectively.

After startup, the following commands are available in both the GUD
interaction buffer and any source buffer GUD visits due to a breakpoint stop
or step operation:

\\[gud-break] sets a breakpoint at the current file and line.  In the
GUD buffer, the current file and line are those of the last breakpoint or
step.  In a source buffer, they are the buffer's file and current line.

\\[gud-remove] removes breakpoints on the current file and line.

\\[gud-refresh] displays in the source window the last line referred to
in the gud buffer.

\\[gud-step], \\[gud-next], and \\[gud-stepi] do a step-one-line,
step-one-line (not entering function calls), and step-one-instruction
and then update the source window with the current file and position.
\\[gud-cont] continues execution.

\\[gud-print] tries to find the largest C lvalue or function-call expression
around point, and sends it to the debugger for value display.

The above commands are common to all supported debuggers except xdb which
does not support stepping instructions.

Under gdb, sdb and xdb, \\[gud-tbreak] behaves exactly like \\[gud-break],
except that the breakpoint is temporary; that is, it is removed when
execution stops on it.

Under gdb, dbx, and xdb, \\[gud-up] pops up through an enclosing stack
frame.  \\[gud-down] drops back down through one.

If you are using gdb or xdb, \\[gud-finish] runs execution to the return from
the current function and stops.

All the keystrokes above are accessible in the GUD buffer
with the prefix C-c, and in all buffers through the prefix C-x C-a.

All pre-defined functions for which the concept make sense repeat
themselves the appropriate number of times if you give a prefix
argument.

You may use the `gud-def' macro in the initialization hook to define other
commands.

Other commands for interacting with the debugger process are inherited from
comint mode, which see."
  (interactive)
  (comint-mode)
  (setq major-mode 'gud-mode)
  (setq mode-name "Debugger")
  (setq mode-line-process '(": %s"))
  (use-local-map (copy-keymap gdbish-comint-mode-map))
  (setq gud-last-frame nil)
  (make-local-variable 'comint-prompt-regexp)
  (make-local-variable 'gud-delete-prompt-marker)
  (setq gud-delete-prompt-marker (make-marker))
  (run-hooks 'gud-mode-hook)
)

(defvar gud-comint-buffer nil)

;; Chop STRING into words separated by SPC or TAB and return a list of them.
(defun gud-chop-words (string)
  (let ((i 0) (beg 0)
	(len (length string))
	(words nil))
    (while (< i len)
      (if (memq (aref string i) '(?\t ? ))
	  (progn
	    (setq words (cons (substring string beg i) words)
		  beg (1+ i))
	    (while (and (< beg len) (memq (aref string beg) '(?\t ? )))
	      (setq beg (1+ beg)))
	    (setq i (1+ beg)))
	(setq i (1+ i))))
    (if (< beg len)
	(setq words (cons (substring string beg) words)))
    (nreverse words)))

(defvar gud-target-name "--unknown--"
  "The apparent name of the program being debugged in a gud buffer.
For sure this the root string used in smashing together the gud 
buffer's name, even if that doesn't happen to be the name of a 
program.")

;; Perform initializations common to all debuggers.
(defun gud-common-init (command-line)
  (let* ((words (gud-chop-words command-line))
	 (program (car words))
	 (file-word (let ((w (cdr words)))
		      (while (and w (= ?- (aref (car w) 0)))
			(setq w (cdr w)))
		      (car w)))
	 (args (delq file-word (cdr words)))
	 (file (expand-file-name file-word))
	 (filepart (file-name-nondirectory file))
	 (buffer-name (concat "*gud-" filepart "*")))
    (switch-to-buffer buffer-name)
    (setq default-directory (file-name-directory file))
    (or (bolp) (newline))
    (insert "Current directory is " default-directory "\n")
    (let ((old-instance gdb-buffer-instance))
      (apply 'make-comint (concat "gud-" filepart) program nil
	     (gud-massage-args file args))
      (gud-mode)
      (make-variable-buffer-local 'old-gdb-buffer-instance)
      (setq old-gdb-buffer-instance old-instance))
    (make-variable-buffer-local 'gud-target-name)
    (setq gud-target-name filepart))
  (set-process-filter (get-buffer-process (current-buffer)) 'gud-filter)
  (set-process-sentinel (get-buffer-process (current-buffer)) 'gud-sentinel)
  (gud-set-buffer)
  )

(defun gud-set-buffer ()
  (cond ((eq major-mode 'gud-mode)
	(setq gud-comint-buffer (current-buffer)))))

;; These functions are responsible for inserting output from your debugger
;; into the buffer.  The hard work is done by the method that is
;; the value of gud-marker-filter.

(defun gud-filter (proc string)
  ;; Here's where the actual buffer insertion is done
  (let ((inhibit-quit t))
    (save-excursion
      (set-buffer (process-buffer proc))
      (let (moving output-after-point)
	(save-excursion
	  (goto-char (process-mark proc))
	  ;; If we have been so requested, delete the debugger prompt.
	  (if (marker-buffer gud-delete-prompt-marker)
	      (progn
		(delete-region (point) gud-delete-prompt-marker)
		(set-marker gud-delete-prompt-marker nil)))
	  (insert-before-markers (gud-marker-filter string))
	  (setq moving (= (point) (process-mark proc)))
	  (setq output-after-point (< (point) (process-mark proc)))
	  ;; Check for a filename-and-line number.
	  ;; Don't display the specified file
	  ;; unless (1) point is at or after the position where output appears
	  ;; and (2) this buffer is on the screen.
	  (if (and gud-last-frame
		   (not output-after-point)
		   (get-buffer-window (current-buffer)))
	      (gud-display-frame)))
	(if moving (goto-char (process-mark proc)))))))

(defun gud-proc-died (proc)
  ;; Stop displaying an arrow in a source file.
  (setq overlay-arrow-position nil)

  ;; Kill the dummy process, so that C-x C-c won't worry about it.
  (save-excursion
    (set-buffer (process-buffer proc))
    (kill-process
     (get-buffer-process
      (gdb-get-instance-buffer gdb-buffer-instance 'gdb-inferior-io))))
  )

(defun gud-sentinel (proc msg)
  (cond ((null (buffer-name (process-buffer proc)))
	 ;; buffer killed
	 (gud-proc-died proc)
	 (set-process-buffer proc nil))
	((memq (process-status proc) '(signal exit))
	 (gud-proc-died proc)

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
	     ;; if obuf is the gud buffer.
	     (set-buffer obuf))))))

(defun gud-display-frame ()
  "Find and obey the last filename-and-line marker from the debugger.
Obeying it means displaying in another window the specified file and line."
  (interactive)
  (if gud-last-frame
   (progn
;     (gud-set-buffer)
     (gud-display-line (car gud-last-frame) (cdr gud-last-frame))
     (setq gud-last-last-frame gud-last-frame
	   gud-last-frame nil))))

;; Make sure the file named TRUE-FILE is in a buffer that appears on the screen
;; and that its line LINE is visible.
;; Put the overlay-arrow on the line LINE in that buffer.
;; Most of the trickiness in here comes from wanting to preserve the current
;; region-restriction if that's possible.  We use an explicit display-buffer
;; to get around the fact that this is called inside a save-excursion.

(defun gud-display-line (true-file line)
  (let* ((buffer (gud-find-file true-file))
	 (window (gud-display-source-buffer buffer))
	 (pos))
    (if (not window)
	(error "foo bar baz"))
;;;    (if (equal buffer (current-buffer))
;;;	nil
;;;      (setq buffer-read-only nil))
    (save-excursion
;;;      (setq buffer-read-only t)
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

;;; The gud-call function must do the right thing whether its invoking
;;; keystroke is from the GUD buffer itself (via major-mode binding)
;;; or a C buffer.  In the former case, we want to supply data from
;;; gud-last-frame.  Here's how we do it:

(defun gud-format-command (str arg)
  (let ((insource (not (eq (current-buffer) gud-comint-buffer))))
    (if (string-match "\\(.*\\)%f\\(.*\\)" str)
	(setq str (concat
		   (substring str (match-beginning 1) (match-end 1))
		   (file-name-nondirectory (if insource
					       (buffer-file-name)
					     (car gud-last-frame)))
		   (substring str (match-beginning 2) (match-end 2)))))
    (if (string-match "\\(.*\\)%d\\(.*\\)" str)
	(setq str (concat
		   (substring str (match-beginning 1) (match-end 1))
		   (file-name-directory (if insource
					    (buffer-file-name)
					  (car gud-last-frame)))
		   (substring str (match-beginning 2) (match-end 2)))))
    (if (string-match "\\(.*\\)%l\\(.*\\)" str)
	(setq str (concat
		   (substring str (match-beginning 1) (match-end 1))
		   (if insource
		       (save-excursion
			 (beginning-of-line)
			 (save-restriction (widen) 
					   (1+ (count-lines 1 (point)))))
		     (cdr gud-last-frame))
		   (substring str (match-beginning 2) (match-end 2)))))
    (if (string-match "\\(.*\\)%e\\(.*\\)" str)
	(setq str (concat
		   (substring str (match-beginning 1) (match-end 1))
		   (find-c-expr)
		   (substring str (match-beginning 2) (match-end 2)))))
    (if (string-match "\\(.*\\)%a\\(.*\\)" str)
	(setq str (concat
		   (substring str (match-beginning 1) (match-end 1))
		   (gud-read-address)
		   (substring str (match-beginning 2) (match-end 2)))))
    (if (string-match "\\(.*\\)%p\\(.*\\)" str)
	(setq str (concat
		   (substring str (match-beginning 1) (match-end 1))
		   (if arg (int-to-string arg) "")
		   (substring str (match-beginning 2) (match-end 2)))))
    )
  str
  )

(defun gud-read-address ()
  "Return a string containing the core-address found in the buffer at point."
  (save-excursion
    (let ((pt (point)) found begin)
      (setq found (if (search-backward "0x" (- pt 7) t) (point)))
      (cond
       (found (forward-char 2)
	      (buffer-substring found
				(progn (re-search-forward "[^0-9a-f]")
				       (forward-char -1)
				       (point))))
       (t (setq begin (progn (re-search-backward "[^0-9]") 
			     (forward-char 1)
			     (point)))
	  (forward-char 1)
	  (re-search-forward "[^0-9]")
	  (forward-char -1)
	  (buffer-substring begin (point)))))))

(defun gud-call (fmt &optional arg)
  (let ((msg (gud-format-command fmt arg)))
    (message "Command: %s" msg)
    (sit-for 0)
    (gud-basic-call msg)))

(defun gud-basic-call (command)
  "Invoke the debugger COMMAND displaying source in other window."
  (interactive)
  (gud-set-buffer)
  (let ((proc (get-buffer-process gud-comint-buffer)))

    ;; Arrange for the current prompt to get deleted.
    (save-excursion
      (set-buffer gud-comint-buffer)
      (goto-char (process-mark proc))
      (beginning-of-line)
      (if (looking-at comint-prompt-regexp)
	  (set-marker gud-delete-prompt-marker (point)))
      (apply comint-input-sender (list proc command)))))

(defun gud-refresh (&optional arg)
  "Fix up a possibly garbled display, and redraw the arrow."
  (interactive "P")
  (recenter arg)
  (or gud-last-frame (setq gud-last-frame gud-last-last-frame))
  (gud-display-frame))

;;; Code for parsing expressions out of C code.  The single entry point is
;;; find-c-expr, which tries to return an lvalue expression from around point.
;;;
;;; The rest of this file is a hacked version of gdbsrc.el by
;;; Debby Ayers <ayers@asc.slb.com>,
;;; Rich Schaefer <schaefer@asc.slb.com> Schlumberger, Austin, Tx.

(defun find-c-expr ()
  "Returns the C expr that surrounds point."
  (interactive)
  (save-excursion
    (let ((p) (expr) (test-expr))
      (setq p (point))
      (setq expr (expr-cur))
      (setq test-expr (expr-prev))
      (while (expr-compound test-expr expr)
	(setq expr (cons (car test-expr) (cdr expr)))
	(goto-char (car expr))
	(setq test-expr (expr-prev)))
      (goto-char p)
      (setq test-expr (expr-next))
      (while (expr-compound expr test-expr)
	(setq expr (cons (car expr) (cdr test-expr)))
	(setq test-expr (expr-next))
	)
      (buffer-substring (car expr) (cdr expr)))))

(defun expr-cur ()
  "Returns the expr that point is in; point is set to beginning of expr.
The expr is represented as a cons cell, where the car specifies the point in
the current buffer that marks the beginning of the expr and the cdr specifies 
the character after the end of the expr."
  (let ((p (point)) (begin) (end))
    (expr-backward-sexp)
    (setq begin (point))
    (expr-forward-sexp)
    (setq end (point))
    (if (>= p end) 
	(progn
	 (setq begin p)
	 (goto-char p)
	 (expr-forward-sexp)
	 (setq end (point))
	 )
      )
    (goto-char begin)
    (cons begin end)))

(defun expr-backward-sexp ()
  "Version of `backward-sexp' that catches errors."
  (condition-case nil
      (backward-sexp)
    (error t)))

(defun expr-forward-sexp ()
  "Version of `forward-sexp' that catches errors."
  (condition-case nil
     (forward-sexp)
    (error t)))

(defun expr-prev ()
  "Returns the previous expr, point is set to beginning of that expr.
The expr is represented as a cons cell, where the car specifies the point in
the current buffer that marks the beginning of the expr and the cdr specifies 
the character after the end of the expr"
  (let ((begin) (end))
    (expr-backward-sexp)
    (setq begin (point))
    (expr-forward-sexp)
    (setq end (point))
    (goto-char begin)
    (cons begin end)))

(defun expr-next ()
  "Returns the following expr, point is set to beginning of that expr.
The expr is represented as a cons cell, where the car specifies the point in
the current buffer that marks the beginning of the expr and the cdr specifies 
the character after the end of the expr."
  (let ((begin) (end))
    (expr-forward-sexp)
    (expr-forward-sexp)
    (setq end (point))
    (expr-backward-sexp)
    (setq begin (point))
    (cons begin end)))

(defun expr-compound-sep (span-start span-end)
  "Returns '.' for '->' & '.', returns ' ' for white space,
returns '?' for other punctuation."
  (let ((result ? )
	(syntax))
    (while (< span-start span-end)
      (setq syntax (char-syntax (char-after span-start)))
      (cond
       ((= syntax ? ) t)
       ((= syntax ?.) (setq syntax (char-after span-start))
	(cond 
	 ((= syntax ?.) (setq result ?.))
	 ((and (= syntax ?-) (= (char-after (+ span-start 1)) ?>))
	  (setq result ?.)
	  (setq span-start (+ span-start 1)))
	 (t (setq span-start span-end)
	    (setq result ??)))))
      (setq span-start (+ span-start 1)))
    result))

(defun expr-compound (first second)
  "Non-nil if concatenating FIRST and SECOND makes a single C token.
The two exprs are represented as a cons cells, where the car 
specifies the point in the current buffer that marks the beginning of the 
expr and the cdr specifies the character after the end of the expr.
Link exprs of the form:
      Expr -> Expr
      Expr . Expr
      Expr (Expr)
      Expr [Expr]
      (Expr) Expr
      [Expr] Expr"
  (let ((span-start (cdr first))
	(span-end (car second))
	(syntax))
    (setq syntax (expr-compound-sep span-start span-end))
    (cond
     ((= (car first) (car second)) nil)
     ((= (cdr first) (cdr second)) nil)
     ((= syntax ?.) t)
     ((= syntax ? )
	 (setq span-start (char-after (- span-start 1)))
	 (setq span-end (char-after span-end))
	 (cond
	  ((= span-start ?) ) t )
	  ((= span-start ?] ) t )
          ((= span-end ?( ) t )
	  ((= span-end ?[ ) t )
	  (t nil))
	 )
     (t nil))))

(provide 'gud)

;;; gud.el ends here
