;;; elib-dll-debug -- A slow implementation of elib-dll for debugging.
;;; elib-dll-debug.el,v 1.2 1992/04/07 20:49:13 berliner Exp
;;; Copyright (C) 1991,1992  Per Cederqvist
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


;;; This is a plug-in replacement for elib-dll.el.  It is dreadfully
;;; slow, but it facilitates debugging.  Don't trust the comments in
;;; this file too much.
(provide 'elib-dll)

;;;
;;; A doubly linked list consists of one cons cell which holds the tag
;;; 'DL-LIST in the car cell and the list in the cdr 
;;; cell. The doubly linked list is implemented as a normal list. You
;;; should use elib-dll.el and not this package in debugged code. This
;;; package is not written for speed...
;;;

;;; ================================================================
;;;      Internal functions for use in the doubly linked list package

(defun dll-get-dummy-node (dll)

  ;; Return the dummy node.   INTERNAL USE ONLY.
  dll)

(defun dll-list-nodes (dll)

  ;; Return a list of all nodes in DLL.   INTERNAL USE ONLY.

  (cdr dll))

(defun dll-set-from-node-list (dll list)

  ;; Set the contents of DLL to the nodes in LIST.
  ;; INTERNAL USE ONLY.

  (setcdr dll list))

(defun dll-get-node-before (dll node)
  ;; Return the node in DLL that points to NODE. Use
  ;; (dll-get-node-before some-list nil) to get the last node.
  ;; INTERNAL USE ONLY.
  (while (and dll (not (eq (cdr dll) node)))
    (setq dll (cdr dll)))
  (if (not dll)
      (error "Node not on list"))
  dll)

(defmacro dll-insert-after (node element)
  (let ((node-v (make-symbol "node"))
	(element-v (make-symbol "element")))
    (` (let (((, node-v) (, node))
	     ((, element-v) (, element)))
	 (setcdr (, node-v) (cons (, element-v) (cdr (, node-v))))))))

;;; ===================================================================
;;;       The public functions which operate on doubly linked lists.

(defmacro dll-element (dll node)

  "Get the element of a NODE in a doubly linked list DLL.
Args: DLL NODE."

  (` (car (, node))))


(defun dll-create ()
  "Create an empty doubly linked list."
  (cons 'DL-LIST nil))


(defun dll-p (object)
  "Return t if OBJECT is a doubly linked list, otherwise return nil."
  (eq (car-safe object) 'DL-LIST))


(defun dll-enter-first (dll element)
  "Add an element first on a doubly linked list.
Args: DLL ELEMENT."
  (setcdr dll (cons element (cdr dll))))


(defun dll-enter-last (dll element)
  "Add an element last on a doubly linked list.
Args: DLL ELEMENT."
  (dll-insert-after (dll-get-node-before dll nil) element))


(defun dll-enter-after (dll node element)
  "In the doubly linked list DLL, insert a node containing ELEMENT after NODE.
Args: DLL NODE ELEMENT."

  (dll-get-node-before dll node)
  (dll-insert-after node element))


(defun dll-enter-before (dll node element)
  "In the doubly linked list DLL, insert a node containing ELEMENT before NODE.
Args: DLL NODE ELEMENT."

  (dll-insert-after (dll-get-node-before dll node) element))



(defun dll-next (dll node)
  "Return the node after NODE, or nil if NODE is the last node.
Args: DLL NODE."

  (dll-get-node-before dll node)
  (cdr node))


(defun dll-previous (dll node)
  "Return the node before NODE, or nil if NODE is the first node.
Args: DLL NODE."

  (dll-get-node-before dll node))


(defun dll-delete (dll node)

  "Delete NODE from the doubly linked list DLL.
Args: DLL NODE. Return the element of node."

  ;; This is a no-op when applied to the dummy node. This will return
  ;; nil if applied to the dummy node since it always contains nil.

  (setcdr (dll-get-node-before dll node) (cdr node)))


(defun dll-delete-first (dll)

  "Delete the first NODE from the doubly linked list DLL.
Return the element. Args: DLL. Returns nil if the DLL was empty."

  ;; Relies on the fact that dll-delete does nothing and
  ;; returns nil if given the dummy node.

  (setcdr dll (cdr (cdr dll))))


(defun dll-delete-last (dll)

  "Delete the last NODE from the doubly linked list DLL.
Return the element. Args: DLL. Returns nil if the DLL was empty."

  ;; Relies on the fact that dll-delete does nothing and
  ;; returns nil if given the dummy node.

  (setcdr dll (dll-get-node-before dll nil) nil))


(defun dll-first (dll)

  "Return the first element on the doubly linked list DLL.
Return nil if the list is empty. The element is not removed."

  (car (cdr dll)))




(defun dll-last (dll)

  "Return the last element on the doubly linked list DLL.
Return nil if the list is empty. The element is not removed."

  (car (dll-get-node-before dll nil)))



(defun dll-nth (dll n)

  "Return the Nth node from the doubly linked list DLL.
 Args: DLL N
N counts from zero. If DLL is not that long, nil is returned.
If N is negative, return the -(N+1)th last element.
Thus, (dll-nth dll 0) returns the first node,
and (dll-nth dll -1) returns the last node."

  ;; Branch 0 ("follow left pointer") is used when n is negative.
  ;; Branch 1 ("follow right pointer") is used otherwise.

  (if (>= n 0)
      (nthcdr n (cdr dll))
    (unwind-protect
	(progn (setcdr dll (nreverse (cdr dll)))
	       (nthcdr (- n) dll))
      (setcdr dll (nreverse (cdr dll))))))

(defun dll-empty (dll)

  "Return t if the doubly linked list DLL is empty, nil otherwise"

  (not (cdr dll)))

(defun dll-length (dll)

  "Returns the number of elements in the doubly linked list DLL."

  (length (cdr dll)))



(defun dll-copy (dll &optional element-copy-fnc)

  "Return a copy of the doubly linked list DLL.
If optional second argument ELEMENT-COPY-FNC is non-nil it should be
a function that takes one argument, an element, and returns a copy of it.
If ELEMENT-COPY-FNC is not given the elements are not copied."

  (if element-copy-fnc
      (cons 'DL-LIST (mapcar element-copy-fnc (cdr dll)))
    (copy-sequence dll)))


(defun dll-all (dll)

  "Return all elements on the double linked list DLL as an ordinary list."

  (cdr dll))


(defun dll-clear (dll)

  "Clear the doubly linked list DLL, i.e. make it completely empty."

  (setcdr dll nil))


(defun dll-map (map-function dll)

  "Apply MAP-FUNCTION to all elements in the doubly linked list DLL.
The function is applied to the first element first."

  (mapcar map-function (cdr dll)))


(defun dll-map-reverse (map-function dll)

  "Apply MAP-FUNCTION to all elements in the doubly linked list DLL.
The function is applied to the last element first."

  (unwind-protect
      (setcdr dll (nreverse (cdr dll)))
    (mapcar map-function (cdr dll))
    (setcdr dll (nreverse (cdr dll)))))


(defun dll-create-from-list (list)

  "Given an elisp LIST create a doubly linked list with the same elements."

  (cons 'DL-LIST list))



(defun dll-sort (dll predicate)

  "Sort the doubly linked list DLL, stably, comparing elements using PREDICATE.
Returns the sorted list. DLL is modified by side effects.
PREDICATE is called with two elements of DLL, and should return T
if the first element is \"less\" than the second."

  (setcdr dll (sort (cdr dll) predicate))
  dll)


(defun dll-filter (dll predicate)

  "Remove all elements in the doubly linked list DLL for which PREDICATE
return nil."

  (let* ((prev dll)
	 (node (cdr dll)))

    (while node
      (cond
       ((funcall predicate (car node))
	(setq prev node))
       (t
	(setcdr prev (cdr node))))
      (setq node (cdr node)))))
