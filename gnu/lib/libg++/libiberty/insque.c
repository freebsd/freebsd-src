/* insque(3C) routines
   Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/*

NAME

	insque, remque -- insert, remove an element from a queue

SYNOPSIS

	struct qelem {
	  struct qelem *q_forw;
	  struct qelem *q_back;
	  char q_data[];
	};

	void insque (struct qelem *elem, struct qelem *pred)

	void remque (struct qelem *elem)

DESCRIPTION

	Routines to manipulate queues built from doubly linked lists.
	The insque routine inserts ELEM in the queue immediately after
	PRED.  The remque routine removes ELEM from its containing queue.

BUGS

*/


struct qelem {
  struct qelem *q_forw;
  struct qelem *q_back;
};


void
insque (elem, pred)
  struct qelem *elem;
  struct qelem *pred;
{
  elem -> q_forw = pred -> q_forw;
  pred -> q_forw -> q_back = elem;
  elem -> q_back = pred;
  pred -> q_forw = elem;
}


void
remque (elem)
  struct qelem *elem;
{
  elem -> q_forw -> q_back = elem -> q_back;
  elem -> q_back -> q_forw = elem -> q_forw;
}
