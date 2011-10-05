

(defun generate-obj-macro (mname &optional postfix &rest slist)
  "Generates a macro definition for an OBJs dependency based on a list of source definitions"

  (let*
      ((replist (apply 'append (mapcar (lambda (sdef)
                                         (goto-char 0)
                                         (let*
                                             ((def (buffer-substring-no-properties
                                                    (search-forward (concat sdef " = \\\n") nil t)
                                                    (search-forward "\n\n" nil t)))
                                              (st (split-string
                                                   (replace-regexp-in-string "^.*\\.h.*\n" "" def)
                                                   "\\s-+\\\\?\\|\n" t)))
                                           st)) slist)))
       (def-start (search-forward (concat mname " = \\\n") nil t))
       (def-end (search-forward "\n\n" nil t))

       (repl (mapconcat
              (lambda (s)
                (concat "\t"
                        (replace-regexp-in-string
                         "\\(\\s-*\\)\\(.*\\)\\.c" "\\1$(OBJ)\\\\\\2.obj" s)
                        " \\"))
              replist "\n"))
       (erepl (if postfix
                  (concat repl "\n" postfix "\n\n")
                (concat repl "\n\n")))
       )
    (delete-region def-start def-end)
    (insert erepl))
  )

