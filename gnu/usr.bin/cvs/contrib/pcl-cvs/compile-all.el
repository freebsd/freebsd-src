;;;; compile-all.el,v 1.2 1992/04/07 20:49:10 berliner Exp
;;;; This file byte-compiles all .el files in pcl-cvs release 1.02.
;;;;
;;;; Copyright (C) 1991 Inge Wallin
;;;;
;;;; This file is part of the GNU Emacs lisp library, Elib.
;;;;
;;;; GNU Elib is free software; you can redistribute it and/or modify
;;;; it under the terms of the GNU General Public License as published by
;;;; the Free Software Foundation; either version 1, or (at your option)
;;;; any later version.
;;;;
;;;; GNU Elib is distributed in the hope that it will be useful,
;;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;;; GNU General Public License for more details.
;;;;
;;;; You should have received a copy of the GNU General Public License
;;;; along with GNU Emacs; see the file COPYING.  If not, write to
;;;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
;;;;


(setq elib-files '("elib-node"
		   "elib-dll"
		   "cookie"
		   "pcl-cvs"))


(defun compile-file-if-necessary (file)
  "Compile the Elib file FILE if necessary.

This is done if FILE.el is newer than FILE.elc or if FILE.elc doesn't exist."
  (let ((el-name (concat file ".el"))
	(elc-name (concat file ".elc")))
    (if (or (not (file-exists-p elc-name))
	    (file-newer-than-file-p el-name elc-name))
	(progn
	  (message (format "Byte-compiling %s..." el-name))
	  (byte-compile-file el-name)))))


(defun compile-pcl-cvs ()
  "Byte-compile all uncompiled files of elib.
Be sure to have . in load-path since a number of files in elib
depend on other files and we always want the newer one even if
a previous version of elib exists."

  (interactive)
  (setq load-path (append '(".") load-path))
  (mapcar (function compile-file-if-necessary)
	  elib-files))
