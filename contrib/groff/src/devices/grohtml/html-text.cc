// -*- C++ -*-
/* Copyright (C) 2000, 2001 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote html-text.cc
 *
 *  html-text.cc
 *
 *  provide a troff like state machine interface which
 *  generates html text.
 */

/*
This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "driver.h"
#include "stringclass.h"
#include "cset.h"

#if !defined(TRUE)
#   define TRUE  (1==1)
#endif
#if !defined(FALSE)
#   define FALSE (1==0)
#endif


#include "html-text.h"


html_text::html_text (simple_output *op) :
  stackptr(NULL), lastptr(NULL), out(op), space_emitted(TRUE),
  current_indentation(-1), pageoffset(-1), linelength(-1)
{
}

html_text::~html_text ()
{
  flush_text();
}

/*
 *  end_tag - shuts down the tag.
 */

void html_text::end_tag (tag_definition *t)
{
  switch (t->type) {

  case I_TAG:      out->put_string("</i>"); break;
  case B_TAG:      out->put_string("</b>"); break;
  case P_TAG:      out->put_string("</p>").nl().enable_newlines(FALSE); break;
  case SUB_TAG:    out->put_string("</sub>"); break;
  case SUP_TAG:    out->put_string("</sup>"); break;
  case TT_TAG:     out->put_string("</tt>"); break;
  case PRE_TAG:    out->put_string("</pre>");
                   if (! is_present(TABLE_TAG)) {
		     out->nl();
		     out->enable_newlines(TRUE);
		   }
		   break;
  case SMALL_TAG:  out->put_string("</small>"); break;
  case BIG_TAG:    out->put_string("</big>"); break;
  case TABLE_TAG:  issue_table_end(); break;

  default:
    error("unrecognised tag");
  }
}

/*
 *  issue_tag - writes out an html tag with argument.
 */

void html_text::issue_tag (char *tagname, char *arg)
{
  if ((arg == 0) || (strlen(arg) == 0)) {
    out->put_string(tagname);
    out->put_string(">");
  } else {
    out->put_string(tagname);
    out->put_string(" ");
    out->put_string(arg);
    out->put_string(">");
  }
}

/*
 *  start_tag - starts a tag.
 */

void html_text::start_tag (tag_definition *t)
{
  switch (t->type) {

  case I_TAG:      issue_tag("<i", t->arg1); break;
  case B_TAG:      issue_tag("<b", t->arg1); break;
  case P_TAG:      issue_tag("\n<p", t->arg1);
                   out->enable_newlines(TRUE); break;
  case SUB_TAG:    issue_tag("<sub", t->arg1); break;
  case SUP_TAG:    issue_tag("<sup", t->arg1); break;
  case TT_TAG:     issue_tag("<tt", t->arg1); break;
  case PRE_TAG:    out->nl(); issue_tag("<pre", t->arg1);
                   out->enable_newlines(FALSE); break;
  case SMALL_TAG:  issue_tag("<small", t->arg1); break;
  case BIG_TAG:    issue_tag("<big", t->arg1); break;
  case TABLE_TAG:  issue_table_begin(t); break;
  case BREAK_TAG:  break;

  default:
    error("unrecognised tag");
  }
}

int html_text::table_is_void (tag_definition *t)
{
  if (linelength > 0) {
    return( current_indentation*100/linelength <= 0 );
  } else {
    return( FALSE );
  }
}

void html_text::issue_table_begin (tag_definition *t)
{
  if (linelength > 0) {
    int width=current_indentation*100/linelength;

    if (width > 0) {
      out->put_string("<table width=\"100%\" border=0 rules=\"none\" frame=\"void\"\n       cols=\"2\" cellspacing=\"0\" cellpadding=\"0\">").nl();
      out->put_string("<tr valign=\"top\" align=\"left\">").nl();
      if ((t->arg1 == 0) || (strcmp(t->arg1, "") == 0))
	out->put_string("<td width=\"").put_number(width).put_string("%\"></td>");
      else {
	out->put_string("<td width=\"").put_number(width).put_string("%\">").nl();
	out->put_string(t->arg1).put_string("</td>");
	t->arg1[0] = (char)0;
      }
      out->put_string("<td width=\"").put_number(100-width).put_string("%\">").nl();
    }
  }
}

void html_text::issue_table_end (void)
{
  out->put_string("</td></table>").nl();
  out->enable_newlines(TRUE);
}

/*
 *  flush_text - flushes html tags which are outstanding on the html stack.
 */

void html_text::flush_text (void)
{
  int notext=TRUE;
  tag_definition *p=stackptr;

  while (stackptr != 0) {
    notext = (notext && (! stackptr->text_emitted));
    if (! notext) {
      end_tag(stackptr);
    }
    p = stackptr;
    stackptr = stackptr->next;
    free(p);
  }
  lastptr = NULL;
}

/*
 *  is_present - returns TRUE if tag is already present on the stack.
 */

int html_text::is_present (HTML_TAG t)
{
  tag_definition *p=stackptr;

  while (p != NULL) {
    if (t == p->type) {
      return( TRUE );
    }
    p = p->next;
  }
  return( FALSE );
}

/*
 *  push_para - adds a new entry onto the html paragraph stack.
 */

void html_text::push_para (HTML_TAG t, char *arg)
{
  tag_definition *p=(tag_definition *)malloc(sizeof(tag_definition));

  p->type         = t;
  p->arg1         = arg;
  p->text_emitted = FALSE;

  /*
   *  if t is a P_TAG or TABLE_TAG or PRE_TAG make sure it goes on the end of the stack.
   *  But we insist that a TABLE_TAG is always after a PRE_TAG
   *  and that a P_TAG is always after a TABLE_TAG
   */

  if (((t == P_TAG) || (t == PRE_TAG) || (t == TABLE_TAG)) &&
      (lastptr != NULL)) {
    if (((lastptr->type == TABLE_TAG) && (t == PRE_TAG)) ||
	((lastptr->type == P_TAG) && (t == TABLE_TAG))) {
      /*
       *  insert p before the lastptr
       */
      if (stackptr == lastptr) {
	/*
	 *  only on element of the stack
	 */
	p->next       = stackptr;
	stackptr      = p;	
      } else {
	/*
	 *  more than one element is on the stack
	 */
	tag_definition *q = stackptr;

	while (q->next != lastptr) {
	  q = q->next;
	}
	q->next       = p;
	p->next       = lastptr;
      }
    } else {
      /*
       *  store, p, at the end
       */
      lastptr->next = p;
      lastptr       = p;
      p->next       = NULL;
    }
  } else {
    p->next       = stackptr;
    if (stackptr == NULL)
      lastptr = p;
    stackptr      = p;
  }
}

/*
 *  do_indent - remember the indent parameters and if
 *              indent is > pageoff and indent has changed
 *              then we start a html table to implement the indentation.
 */

void html_text::do_indent (char *arg, int indent, int pageoff, int linelen)
{
  if ((current_indentation != -1) &&
      (pageoffset+current_indentation != indent+pageoff)) {
      /*
       *  actual indentation of text has changed, we need to put
       *  a table tag onto the stack.
       */
    do_table(arg);
  }
  current_indentation = indent;
  pageoffset          = pageoff;
  linelength          = linelen;
}

void html_text::do_table (char *arg)
{
  int in_pre = is_in_pre();
  // char *para_type = done_para();
  done_pre();
  shutdown(TABLE_TAG);   // shutdown a previous table, if present
  remove_break();
  if (in_pre) {
    do_pre();
  }
  // do_para(para_type);
  push_para(TABLE_TAG, arg);
}

/*
 *  done_table - terminates a possibly existing table.
 */

void html_text::done_table (void)
{
  shutdown(TABLE_TAG);
  space_emitted = TRUE;
}

/*
 *  do_italic - changes to italic
 */

void html_text::do_italic (void)
{
  done_bold();
  done_tt();
  if (! is_present(I_TAG)) {
    push_para(I_TAG, "");
  }
}

/*
 *  do_bold - changes to bold.
 */

void html_text::do_bold (void)
{
  done_italic();
  done_tt();
  if (! is_present(B_TAG)) {
    push_para(B_TAG, "");
  }
}

/*
 *  do_tt - changes to teletype.
 */

void html_text::do_tt (void)
{
  done_bold();
  done_italic();
  if ((! is_present(TT_TAG)) && (! is_present(PRE_TAG))) {
    push_para(TT_TAG, "");
  }
}

/*
 *  do_pre - changes to preformated text.
 */

void html_text::do_pre (void)
{
  done_bold();
  done_italic();
  done_tt();
  (void)done_para();
  if (! is_present(PRE_TAG)) {
    push_para(PRE_TAG, "");
  }
}

/*
 *  is_in_pre - returns TRUE if we are currently within a preformatted
 *              <pre> block.
 */

int html_text::is_in_pre (void)
{
  return( is_present(PRE_TAG) );
}

/*
 *  is_in_table - returns TRUE if we are currently within a table.
 */

int html_text::is_in_table (void)
{
  return( is_present(TABLE_TAG) );
}

/*
 *  shutdown - shuts down an html tag.
 */

char *html_text::shutdown (HTML_TAG t)
{
  char *arg=NULL;

  if (is_present(t)) {
    tag_definition *p    =stackptr;
    tag_definition *temp =NULL;
    int notext           =TRUE;
    
    while ((stackptr != NULL) && (stackptr->type != t)) {
      notext = (notext && (! stackptr->text_emitted));
      if (! notext) {
	end_tag(stackptr);
      }

      /*
       *  pop tag
       */
      p        = stackptr;
      stackptr = stackptr->next;
      if (stackptr == NULL)
	lastptr = NULL;
    
      /*
       *  push tag onto temp stack
       */
      p->next  = temp;
      temp     = p;
    }

    /*
     *  and examine stackptr
     */
    if ((stackptr != NULL) && (stackptr->type == t)) {
      if (stackptr->text_emitted) {
	end_tag(stackptr);
      }
      if (t == P_TAG) {
	arg = stackptr->arg1;
      }
      p        = stackptr;
      stackptr = stackptr->next;
      if (stackptr == NULL)
	lastptr = NULL;
      free(p);
    }

    /*
     *  and restore unaffected tags
     */
    while (temp != NULL) {
      push_para(temp->type, temp->arg1);
      p    = temp;
      temp = temp->next;
      free(p);
    }
  }
  return( arg );
}

/*
 *  done_bold - shuts downs a bold tag.
 */

void html_text::done_bold (void)
{
  shutdown(B_TAG);
}

/*
 *  done_italic - shuts downs an italic tag.
 */

void html_text::done_italic (void)
{
  shutdown(I_TAG);
}

/*
 *  done_sup - shuts downs a sup tag.
 */

void html_text::done_sup (void)
{
  shutdown(SUP_TAG);
}

/*
 *  done_sub - shuts downs a sub tag.
 */

void html_text::done_sub (void)
{
  shutdown(SUB_TAG);
}

/*
 *  done_tt - shuts downs a tt tag.
 */

void html_text::done_tt (void)
{
  shutdown(TT_TAG);
}

/*
 *  done_pre - shuts downs a pre tag.
 */

void html_text::done_pre (void)
{
  shutdown(PRE_TAG);
}

/*
 *  done_small - shuts downs a small tag.
 */

void html_text::done_small (void)
{
  shutdown(SMALL_TAG);
}

/*
 *  done_big - shuts downs a big tag.
 */

void html_text::done_big (void)
{
  shutdown(BIG_TAG);
}

/*
 *  check_emit_text - ensures that all previous tags have been emitted (in order)
 *                    before the text is written.
 */

void html_text::check_emit_text (tag_definition *t)
{
  if ((t != NULL) && (! t->text_emitted)) {
    /*
     *  we peep and see whether there is a <p> before the <table>
     * in which case we skip the <p>
     */
    if (t->type == TABLE_TAG) {
      if (table_is_void(t)) {
	tag_definition *n = t->next;
	remove_def(t);
	check_emit_text(n);
      } else {
	/*
	 *  a table which will be emitted, is there a <p> succeeding it?
	 */
	if ((t->next != NULL) &&
	    (t->next->type == P_TAG) &&
	    ((t->next->arg1 == 0) || strcmp(t->next->arg1, "") == 0)) {
	  /*
	   *  yes skip the <p>
	   */
	  check_emit_text(t->next->next);
	} else {
	  check_emit_text(t->next);
	}
	t->text_emitted = TRUE;
	start_tag(t);
      }
    } else {
      check_emit_text(t->next);
      t->text_emitted = TRUE;
      start_tag(t);
    }
  }
}

/*
 *  do_emittext - tells the class that text was written during the current tag.
 */

void html_text::do_emittext (char *s, int length)
{
  if ((! is_present(P_TAG)) && (! is_present(PRE_TAG)))
    do_para("");

  if (is_present(BREAK_TAG)) {
    int text = remove_break();
    check_emit_text(stackptr);
    if (text) {
      if (is_present(PRE_TAG)) {
	out->nl();
      } else {
	out->put_string("<br>").nl();
      }
    }
  } else {
    check_emit_text(stackptr);
  }
  out->put_string(s, length);
  space_emitted = FALSE;
}

/*
 *  do_para- starts a new paragraph
 */

void html_text::do_para (char *arg)
{
  done_pre();
  if (! is_present(P_TAG)) {
    remove_sub_sup();
    if ((arg != 0) && (strcmp(arg, "") != 0)) {
      remove_tag(TABLE_TAG);
    }
    push_para(P_TAG, arg);
    space_emitted = TRUE;
  }
}

/*
 *  done_para - shuts down a paragraph tag.
 */

char *html_text::done_para (void)
{
  space_emitted = TRUE;
  return( shutdown(P_TAG) );
}

/*
 *  do_space - issues an end of paragraph
 */

void html_text::do_space (void)
{
  if (is_in_pre()) {
    do_emittext("", 0);
  } else {
    do_para(done_para());
  }
  space_emitted = TRUE;
}

/*
 *  do_break - issue a break tag.
 */

void html_text::do_break (void)
{
  if (! is_present(PRE_TAG)) {
    if (emitted_text()) {
      if (! is_present(BREAK_TAG)) {
	push_para(BREAK_TAG, "");
      }
    }
  }
  space_emitted = TRUE;
}

/*
 *  do_newline - issue a newline providing that we are inside a <pre> tag.
 */

void html_text::do_newline (void)
{
  if (is_present(PRE_TAG)) {
    do_emittext("\n", 1);
    space_emitted = TRUE;
  }
}

/*
 *  emitted_text - returns FALSE if white space has just been written.
 */

int html_text::emitted_text (void)
{
  return( ! space_emitted);
}

/*
 *  emit_space - writes a space providing that text was written beforehand.
 */

void html_text::emit_space (void)
{
  if (space_emitted) {
    if (is_present(PRE_TAG)) {
      do_emittext(" ", 1);
    }
  } else {
    out->space_or_newline();
    space_emitted = TRUE;
  }
}

/*
 *  remove_def - removes a definition, t, from the stack.
 */

void html_text::remove_def (tag_definition *t)
{
  tag_definition *p    = stackptr;
  tag_definition *l    = 0;
  tag_definition *q    = 0;
    
  while ((p != 0) && (p != t)) {
    l = p;
    p = p->next;
  }
  if ((p != 0) && (p == t)) {
    if (p == stackptr) {
      stackptr = stackptr->next;
      if (stackptr == NULL)
	lastptr = NULL;
      q = stackptr;
    } else if (l == 0) {
      error("stack list pointers are wrong");
    } else {
      l->next = p->next;
      q = p->next;
      if (l->next == NULL)
	lastptr = l;
    }
    free(p);
  }
}

/*
 *  remove_tag - removes a tag from the stack.
 */

void html_text::remove_tag (HTML_TAG tag)
{
  tag_definition *p = stackptr;
    
  while ((p != 0) && (p->type != tag)) {
    p = p->next;
  }
  if ((p != 0) && (p->type == tag))
    remove_def(p);
}

/*
 *  remove_sub_sup - removes a sub or sup tag, should either exist on the stack.
 */

void html_text::remove_sub_sup (void)
{
  if (is_present(SUB_TAG)) {
    remove_tag(SUB_TAG);
  }
  if (is_present(SUP_TAG)) {
    remove_tag(SUP_TAG);
  }
  if (is_present(PRE_TAG)) {
    remove_tag(PRE_TAG);
  }
}

/*
 *  remove_break - break tags are not balanced thus remove it once it has been emitted.
 *                 It returns TRUE if text was emitted before the <br> was issued.
 */

int html_text::remove_break (void)
{
  tag_definition *p    = stackptr;
  tag_definition *l    = 0;
  tag_definition *q    = 0;

  while ((p != 0) && (p->type != BREAK_TAG)) {
    l = p;
    p = p->next;
  }
  if ((p != 0) && (p->type == BREAK_TAG)) {
    if (p == stackptr) {
      stackptr = stackptr->next;
      if (stackptr == NULL)
	lastptr = NULL;
      q = stackptr;
    } else if (l == 0) {
      error("stack list pointers are wrong");
    } else {
      l->next = p->next;
      q = p->next;
      if (l->next == NULL)
	lastptr = l;
    }
    free(p);
  }
  /*
   *  now determine whether text was issued before <br>
   */
  while (q != 0) {
    if (q->text_emitted) {
      return( TRUE );
    } else {
      q = q->next;
    }
  }
  return( FALSE );
}

/*
 *  do_small - potentially inserts a <small> tag into the html stream.
 *             However we check for a <big> tag, if present then we terminate it.
 *             Otherwise a <small> tag is inserted.
 */

void html_text::do_small (void)
{
  if (is_present(BIG_TAG)) {
    done_big();
  } else {
    push_para(SMALL_TAG, "");
  }
}

/*
 *  do_big - is the mirror image of do_small.
 */

void html_text::do_big (void)
{
  if (is_present(SMALL_TAG)) {
    done_small();
  } else {
    push_para(BIG_TAG, "");
  }
}

/*
 *  do_sup - save a superscript tag on the stack of tags.
 */

void html_text::do_sup (void)
{
  push_para(SUP_TAG, "");
}

/*
 *  do_sub - save a subscript tag on the stack of tags.
 */

void html_text::do_sub (void)
{
  push_para(SUB_TAG, "");
}

