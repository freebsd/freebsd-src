;;; pcl-cvs-startup.el,v 1.2 1992/04/07 20:49:17 berliner Exp
(autoload 'cvs-update "pcl-cvs"
	  "Run a 'cvs update' in the current working directory. Feed the
output to a *cvs* buffer and run cvs-mode on it.
If optional prefix argument LOCAL is non-nil, 'cvs update -l' is run."
	  t)
