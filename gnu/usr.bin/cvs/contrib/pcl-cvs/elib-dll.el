;;; elib-dll.el,v 1.2 1992/04/07 20:49:15 berliner Exp
;;; elib-dll.el -- Some primitives for Doubly linked lists.
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

;;; Mail bug reports to ceder@lysator.liu.se.

(require 'elib-node)
(provide 'elib-dll)

;;;
;;; A doubly linked list consists of one cons cell which holds the tag
;;; 'DL-LIST in the car cell and a pointer to a dummy node in the cdr
;;; cell. The doubly linked list is implemented as a circular list
;;; with the dummy node first and last. The dummy node is recognized
;;; by comparing it to the node which the cdr of the cons cell points
;;; to.
;;;

;;; ================================================================
;;;      Internal functions for use in the doubly linked list package

(defun dll-get-dummy-node (dll)

  ;; Return the dummy node.   INTERNAL USE ONLY.
  (cdr dll))

(defun dll-list-nodes (dll)

  ;; Return a list of all nodes in DLL.   INTERNAL USE ONLY.

  (let* ((result nil)
	 (dummy  (dll-get-dummy-node dll))
	 (node   (elib-node-left dummy)))

    (while (not (eq node dummy))
      (setq result (cons node result))
      (setq node (elib-node-left node)))

    result))

(defun dll-set-from-node-list (dll list)

  ;; Set the contents of DLL to the nodes in LIST.
  ;; INTERNAL USE ONLY.

  (dll-clear dll)
  (let* ((dummy (dll-get-dummy-node dll))
	 (left  dummy))
    (while list
      (elib-node-set-left (car list) left)
      (elib-node-set-right left (car list))
      (setq left (car list))
      (setq list (cdr list)))

    (elib-node-set-right left dummy)
    (elib-node-set-left dummy left)))


;;; ===================================================================
;;;       The public functions which operate on doubly linked lists.

(defmacro dll-element (dll node)

  "Get the element of a NODE in a doubly linked list DLL.
Args: DLL NODE."

  (` (elib-node-data (, node))))


(defun dll-create ()
  "Create an empty doubly linked list."
  (let ((dummy-node (elib-node-create nil nil nil)))
    (elib-node-set-right dummy-node dummy-node)
    (elib-node-set-left dummy-node dummy-node)
    (cons 'DL-LIST dummy-node)))

(defun dll-p (object)
  "Return t if OBJECT is a doubly linked list, otherwise return nil."
  (eq (car-safe object) 'DL-LIST))

(defun dll-enter-first (dll element)
  "Add an element first on a doubly linked list.
Args: DLL ELEMENT."
  (dll-enter-after
   dll
   (dll-get-dummy-node dll)
   element))


(defun dll-enter-last (dll element)
  "Add an element last on a doubly linked list.
Args: DLL ELEMENT."
  (dll-enter-before
   dll
   (dll-get-dummy-node dll)
   element))


(defun dll-enter-after (dll node element)
  "In the doubly linked list DLL, insert a node containing ELEMENT after NODE.
Args: DLL NODE ELEMENT."

  (let ((new-node (elib-node-create
		   node (elib-node-right node)
		   element)))
    (elib-node-set-left (elib-node-right node) new-node)
    (elib-node-set-right node new-node)))


(defun dll-enter-before (dll node element)
  "In the doubly linked list DLL, insert a node containing ELEMENT before NODE.
Args: DLL NODE ELEMENT."
  
  (let ((new-node (elib-node-create
		   (elib-node-left node) node
		   element)))
    (elib-node-set-right (elib-node-left node) new-node)
    (elib-node-set-left node new-node)))



(defun dll-next (dll node)
  "Return the node after NODE, or nil if NODE is the last node.
Args: DLL NODE."

  (if (eq (elib-node-right node) (dll-get-dummy-node dll))
      nil
    (elib-node-right node)))


(defun dll-previous (dll node)
  "Return the node before NODE, or nil if NODE is the first node.
Args: DLL NODE."

  (if (eq (elib-node-left node) (dll-get-dummy-node dll))
      nil
    (elib-node-left node)))


(defun dll-delete (dll node)

  "Delete NODE from the doubly linked list DLL.
Args: DLL NODE. Return the element of node."

  ;; This is a no-op when applied to the dummy node. This will return
  ;; nil if applied to the dummy node since it always contains nil.

  (elib-node-set-right (elib-node-left node) (elib-node-right node))
  (elib-node-set-left (elib-node-right node) (elib-node-left node))
  (dll-element dll node))



(defun dll-delete-first (dll)

  "Delete the first NODE from the doubly linked list DLL.
Return the element. Args: DLL. Returns nil if the DLL was empty."

  ;; Relies on the fact that dll-delete does nothing and
  ;; returns nil if given the dummy node.

  (dll-delete dll (elib-node-right (dll-get-dummy-node dll))))


(defun dll-delete-last (dll)

  "Delete the last NODE from the doubly linked list DLL.
Return the element. Args: DLL. Returns nil if the DLL was empty."

  ;; Relies on the fact that dll-delete does nothing and
  ;; returns nil if given the dummy node.

  (dll-delete dll (elib-node-left (dll-get-dummy-node dll))))


(defun dll-first (dll)

  "Return the first element on the doubly linked list DLL.
Return nil if the list is empty. The element is not removed."

  (if (eq (elib-node-right (dll-get-dummy-node dll))
	  (dll-get-dummy-node dll))
      nil
    (elib-node-data (elib-node-right (dll-get-dummy-node dll)))))




(defun dll-last (dll)

  "Return the last element on the doubly linked list DLL.
Return nil if the list is empty. The element is not removed."

  (if (eq (elib-node-left (dll-get-dummy-node dll))
	  (dll-get-dummy-node dll))
      nil
    (elib-node-data (elib-node-left (dll-get-dummy-node dll)))))



(defun dll-nth (dll n)

  "Return the Nth node from the doubly linked list DLL.
 Args: DLL N
N counts from zero. If DLL is not that long, nil is returned.
If N is negative, return the -(N+1)th last element.
Thus, (dll-nth dll 0) returns the first node,
and (dll-nth dll -1) returns the last node."

  ;; Branch 0 ("follow left pointer") is used when n is negative.
  ;; Branch 1 ("follow right pointer") is used otherwise.

  (let* ((dummy  (dll-get-dummy-node dll))
	 (branch (if (< n 0) 0 1))
	 (node   (elib-node-branch dummy branch)))
	 
    (if (< n 0)
	(setq n (- -1 n)))

    (while (and (not (eq dummy node))
		(> n 0))
      (setq node (elib-node-branch node branch))
      (setq n (1- n)))

    (if (eq dummy node)
	nil
      node)))


(defun dll-empty (dll)

  "Return t if the doubly linked list DLL is empty, nil otherwise"

  (eq (elib-node-left (dll-get-dummy-node dll))
      (dll-get-dummy-node dll)))

(defun dll-length (dll)

  "Returns the number of elements in the doubly linked list DLL."

  (let*  ((dummy (dll-get-dummy-node dll))
	  (node  (elib-node-right dummy))
	  (n     0))

    (while (not (eq node dummy))
      (setq node (elib-node-right node))
      (setq n (1+ n)))

    n))



(defun dll-copy (dll &optional element-copy-fnc)

  "Return a copy of the doubly linked list DLL.
If optional second argument ELEMENT-COPY-FNC is non-nil it should be
a function that takes one argument, an element, and returns a copy of it.
If ELEMENT-COPY-FNC is not given the elements are not copied."

  (let ((result (dll-create))
	(node (dll-nth dll 0)))
    (if element-copy-fnc

	;; Copy the elements with the user-supplied function.
	(while node
	  (dll-enter-last result
			  (funcall element-copy-fnc
				   (dll-element dll node)))
	  (setq node (dll-next dll node)))

      ;; Don't try to copy the elements - they might be
      ;; circular lists, or anything at all...
      (while node
	(dll-enter-last result (dll-element dll node))
	(setq node (dll-next dll node))))
    
    result))



(defun dll-all (dll)

  "Return all elements on the double linked list DLL as an ordinary list."

  (let* ((result nil)
	 (dummy  (dll-get-dummy-node dll))
	 (node   (elib-node-left dummy)))

    (while (not (eq node dummy))
      (setq result (cons (dll-element dll node) result))
      (setq node (elib-node-left node)))

    result))


(defun dll-clear (dll)

  "Clear the doubly linked list DLL, i.e. make it completely empty."

  (elib-node-set-left (dll-get-dummy-node dll) (dll-get-dummy-node dll))
  (elib-node-set-right (dll-get-dummy-node dll) (dll-get-dummy-node dll)))


(defun dll-map (map-function dll)

  "Apply MAP-FUNCTION to all elements in the doubly linked list DLL.
The function is applied to the first element first."

  (let*  ((dummy (dll-get-dummy-node dll))
	  (node  (elib-node-right dummy)))

    (while (not (eq node dummy))
      (funcall map-function (dll-element dll node))
      (setq node (elib-node-right node)))))


(defun dll-map-reverse (map-function dll)

  "Apply MAP-FUNCTION to all elements in the doubly linked list DLL.
The function is applied to the last element first."

  (let*  ((dummy (dll-get-dummy-node dll))
	  (node  (elib-node-left dummy)))

    (while (not (eq node dummy))
      (funcall map-function (dll-element dll node))
      (setq node (elib-node-left node)))))


(defun dll-create-from-list (list)

  "Given an elisp LIST create a doubly linked list with the same elements."

  (let ((dll (dll-create)))
    (while list
      (dll-enter-last dll (car list))
      (setq list (cdr list)))
    dll))



(defun dll-sort (dll predicate)

  "Sort the doubly linked list DLL, stably, comparing elements using PREDICATE.
Returns the sorted list. DLL is modified by side effects.
PREDICATE is called with two elements of DLL, and should return T
if the first element is \"less\" than the second."

  (dll-set-from-node-list
   dll (sort (dll-list-nodes dll)
	     (function (lambda (x1 x2)
			 (funcall predicate
				  (dll-element dll x1)
				  (dll-element dll x2))))))
  dll)


(defun dll-filter (dll predicate)

  "Remove all elements in the doubly linked list DLL for which PREDICATE
return nil."

  (let* ((dummy (dll-get-dummy-node dll))
	 (node  (elib-node-right dummy))
	 next)

    (while (not (eq node dummy))
      (setq next (elib-node-right node))
      (if (funcall predicate (dll-element dll node))
	  nil
	(dll-delete dll node))
      (setq node next))))
