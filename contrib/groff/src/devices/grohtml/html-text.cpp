// -*- C++ -*-
/* Copyright (C) 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
 *
 *  Gaius Mulley (gaius@glam.ac.uk) wrote html-text.cpp
 *
 *  html-text.cpp
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

// #define DEBUGGING

html_text::html_text (simple_output *op) :
  stackptr(NULL), lastptr(NULL), out(op), space_emitted(TRUE),
  current_indentation(-1), pageoffset(-1), linelength(-1),
  blank_para(TRUE), start_space(FALSE)
{
}

html_text::~html_text ()
{
  flush_text();
}


#if defined(DEBUGGING)
static int debugStack = FALSE;


/*
 *  turnDebug - flip the debugStack boolean and return the new value.
 */

static int turnDebug (void)
{
  debugStack = 1-debugStack;
  return debugStack;
}

/*
 *  dump_stack_element - display an element of the html stack, p.
 */

void html_text::dump_stack_element (tag_definition *p)
{
  fprintf(stderr, " | ");
  switch (p->type) {

  case P_TAG:      if (p->indent == NULL) {
                      fprintf(stderr, "<P %s>", (char *)p->arg1); break;
                   } else {
                      fprintf(stderr, "<P %s [TABLE]>", (char *)p->arg1); break;
		   }
  case I_TAG:      fprintf(stderr, "<I>"); break;
  case B_TAG:      fprintf(stderr, "<B>"); break;
  case SUB_TAG:    fprintf(stderr, "<SUB>"); break;
  case SUP_TAG:    fprintf(stderr, "<SUP>"); break;
  case TT_TAG:     fprintf(stderr, "<TT>"); break;
  case PRE_TAG:    if (p->indent == NULL) {
                      fprintf(stderr, "<PRE>"); break;
                   } else {
                      fprintf(stderr, "<PRE [TABLE]>"); break;
		   }
  case SMALL_TAG:  fprintf(stderr, "<SMALL>"); break;
  case BIG_TAG:    fprintf(stderr, "<BIG>"); break;
  case BREAK_TAG:  fprintf(stderr, "<BREAK>"); break;
  case COLOR_TAG:  {
    if (p->col.is_default())
      fprintf(stderr, "<COLOR (default)>");
    else {
      unsigned int r, g, b;
      
      p->col.get_rgb(&r, &g, &b);
      fprintf(stderr, "<COLOR %x %x %x>", r/0x101, g/0x101, b/0x101);
    }
    break;
  }
  default: fprintf(stderr, "unknown tag");
  }
  if (p->text_emitted)
    fprintf(stderr, "[t] ");
}

/*
 *  dump_stack - debugging function only.
 */

void html_text::dump_stack (void)
{
  if (debugStack) {
    tag_definition *p = stackptr;

    while (p != NULL) {
      dump_stack_element(p);
      p = p->next;
    }
  }
  fprintf(stderr, "\n");
  fflush(stderr);
}
#else
void html_text::dump_stack (void) {}
#endif


/*
 *  end_tag - shuts down the tag.
 */

void html_text::end_tag (tag_definition *t)
{
  switch (t->type) {

  case I_TAG:      out->put_string("</i>"); break;
  case B_TAG:      out->put_string("</b>"); break;
  case P_TAG:      out->put_string("</p>");
                   if (t->indent != NULL) {
		     delete t->indent;
		     t->indent = NULL;
		   }
		   out->nl(); out->enable_newlines(FALSE);
                   blank_para = TRUE; break;
  case SUB_TAG:    out->put_string("</sub>"); break;
  case SUP_TAG:    out->put_string("</sup>"); break;
  case TT_TAG:     out->put_string("</tt>"); break;
  case PRE_TAG:    out->put_string("</pre>"); out->nl(); out->enable_newlines(TRUE);
                   blank_para = TRUE; break;
  case SMALL_TAG:  out->put_string("</small>"); break;
  case BIG_TAG:    out->put_string("</big>"); break;
  case COLOR_TAG:  out->put_string("</font>"); break;

  default:
    error("unrecognised tag");
  }
}

/*
 *  issue_tag - writes out an html tag with argument.
 */

void html_text::issue_tag (const char *tagname, const char *arg)
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
 *  issue_color_begin - writes out an html color tag.
 */

void html_text::issue_color_begin (color *c)
{
  unsigned int r, g, b;
  char buf[6+1];

  out->put_string("<font color=\"#");
  if (c->is_default())
    sprintf(buf, "000000");
  else {
    c->get_rgb(&r, &g, &b);
    // we have to scale 0..0xFFFF to 0..0xFF
    sprintf(buf, "%.2X%.2X%.2X", r/0x101, g/0x101, b/0x101);
  }
  out->put_string(buf);
  out->put_string("\">");
}

/*
 *  start_tag - starts a tag.
 */

void html_text::start_tag (tag_definition *t)
{
  switch (t->type) {

  case I_TAG:      issue_tag("<i", (char *)t->arg1); break;
  case B_TAG:      issue_tag("<b", (char *)t->arg1); break;
  case P_TAG:      if (t->indent == NULL) {
                     out->nl();
                     issue_tag("\n<p", (char *)t->arg1);
                   } else {
		     out->nl();
		     out->simple_comment("INDENTATION");
		     t->indent->begin(FALSE);
		     start_space = FALSE;
                     issue_tag("<p", (char *)t->arg1);
		   }

                   out->enable_newlines(TRUE); break;
  case SUB_TAG:    issue_tag("<sub", (char *)t->arg1); break;
  case SUP_TAG:    issue_tag("<sup", (char *)t->arg1); break;
  case TT_TAG:     issue_tag("<tt", (char *)t->arg1); break;
  case PRE_TAG:    if (t->indent != NULL) {
                     out->nl();
		     out->simple_comment("INDENTATION");
		     t->indent->begin(FALSE);
		     start_space = FALSE;
                   }
                   out->enable_newlines(TRUE);
                   out->nl(); issue_tag("<pre", (char *)t->arg1);
                   out->enable_newlines(FALSE); break;
  case SMALL_TAG:  issue_tag("<small", (char *)t->arg1); break;
  case BIG_TAG:    issue_tag("<big", (char *)t->arg1); break;
  case BREAK_TAG:  break;
  case COLOR_TAG:  issue_color_begin(&t->col); break;

  default:
    error("unrecognised tag");
  }
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
    if (t == p->type)
      return TRUE;
    p = p->next;
  }
  return FALSE;
}

extern void stop();

/*
 *  do_push - places, tag_definition, p, onto the stack
 */

void html_text::do_push (tag_definition *p)
{
  HTML_TAG t = p->type;

#if defined(DEBUGGING)
  if (t == PRE_TAG)
    stop();
  debugStack = TRUE;
  fprintf(stderr, "\nentering do_push (");
  dump_stack_element(p);
  fprintf(stderr, ")\n");
  dump_stack();
  fprintf(stderr, ")\n");
  fflush(stderr);
#endif

  /*
   *  if t is a P_TAG or PRE_TAG make sure it goes on the end of the stack.
   */

  if (((t == P_TAG) || (t == PRE_TAG)) && (lastptr != NULL)) {
    /*
     *  store, p, at the end
     */
    lastptr->next = p;
    lastptr       = p;
    p->next       = NULL;
  } else {
    p->next       = stackptr;
    if (stackptr == NULL)
      lastptr = p;
    stackptr      = p;
  }

#if defined(DEBUGGING)
  dump_stack();
  fprintf(stderr, "exiting do_push\n");
#endif
}

/*
 *  push_para - adds a new entry onto the html paragraph stack.
 */

void html_text::push_para (HTML_TAG t, void *arg, html_indent *in)
{
  tag_definition *p=(tag_definition *)malloc(sizeof(tag_definition));

  p->type         = t;
  p->arg1         = arg;
  p->text_emitted = FALSE;
  p->indent       = in;

  if (t == PRE_TAG && is_present(PRE_TAG))
    fatal("cannot have multiple PRE_TAGs");

  do_push(p);
}

void html_text::push_para (HTML_TAG t)
{
  push_para(t, (void *)"", NULL);
}

void html_text::push_para (color *c)
{
  tag_definition *p=(tag_definition *)malloc(sizeof(tag_definition));

  p->type         = COLOR_TAG;
  p->arg1         = NULL;
  p->col          = *c;
  p->text_emitted = FALSE;
  p->indent       = NULL;

  do_push(p);
}

/*
 *  do_italic - changes to italic
 */

void html_text::do_italic (void)
{
  if (! is_present(I_TAG))
    push_para(I_TAG);
}

/*
 *  do_bold - changes to bold.
 */

void html_text::do_bold (void)
{
  if (! is_present(B_TAG))
    push_para(B_TAG);
}

/*
 *  do_tt - changes to teletype.
 */

void html_text::do_tt (void)
{
  if ((! is_present(TT_TAG)) && (! is_present(PRE_TAG)))
    push_para(TT_TAG);
}

/*
 *  do_pre - changes to preformated text.
 */

void html_text::do_pre (void)
{
  done_tt();
  if (is_present(P_TAG)) {
    html_indent *i = remove_indent(P_TAG);
    (void)done_para();
    if (! is_present(PRE_TAG))
      push_para(PRE_TAG, NULL, i);
  } else if (! is_present(PRE_TAG))
    push_para(PRE_TAG, NULL, NULL);
  dump_stack();
}

/*
 *  is_in_pre - returns TRUE if we are currently within a preformatted
 *              <pre> block.
 */

int html_text::is_in_pre (void)
{
  return is_present(PRE_TAG);
}

/*
 *  do_color - initiates a new color tag.
 */

void html_text::do_color (color *c)
{
  shutdown(COLOR_TAG);   // shutdown a previous color tag, if present
  push_para(c);
}

/*
 *  done_color - shutdown an outstanding color tag, if it exists.
 */

void html_text::done_color (void)
{
  shutdown(COLOR_TAG);
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
    
    dump_stack();
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
	arg = (char *)stackptr->arg1;
      }
      p        = stackptr;
      stackptr = stackptr->next;
      if (stackptr == NULL)
	lastptr = NULL;
      if (p->indent != NULL)
	delete p->indent;
      free(p);
    }

    /*
     *  and restore unaffected tags
     */
    while (temp != NULL) {
      if (temp->type == COLOR_TAG)
	push_para(&temp->col);
      else
	push_para(temp->type, temp->arg1, temp->indent);
      p    = temp;
      temp = temp->next;
      free(p);
    }
  }
  return arg;
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
    check_emit_text(t->next);
    t->text_emitted = TRUE;
    start_tag(t);
  }
}

/*
 *  do_emittext - tells the class that text was written during the current tag.
 */

void html_text::do_emittext (const char *s, int length)
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
  blank_para = FALSE;
}

/*
 *  do_para - starts a new paragraph
 */

void html_text::do_para (const char *arg, html_indent *in)
{
  if (! is_present(P_TAG)) {
    if (is_present(PRE_TAG)) {
      html_indent *i = remove_indent(PRE_TAG);
      done_pre();    
      if (i == in || in == NULL)
	in = i;
      else
	delete i;
    }
    remove_sub_sup();
    push_para(P_TAG, (void *)arg, in);
    space_emitted = TRUE;
  }
}

void html_text::do_para (const char *arg)
{
  do_para(arg, NULL);
}

void html_text::do_para (simple_output *op, const char *arg1,
			 int indentation, int pageoffset, int linelength)
{
  html_indent *indent;

  if (indentation == 0)
    indent = NULL;
  else
    indent = new html_indent(op, indentation, pageoffset, linelength);
  do_para(arg1, indent);
}

/*
 *  done_para - shuts down a paragraph tag.
 */

char *html_text::done_para (void)
{
  space_emitted = TRUE;
  return shutdown(P_TAG);
}

/*
 *  remove_indent - returns the indent associated with, tag.
 *                  The indent associated with tag is set to NULL.
 */

html_indent *html_text::remove_indent (HTML_TAG tag)
{
  tag_definition *p=stackptr;

  while (p != NULL) {
    if (tag == p->type) {
      html_indent *i = p->indent;
      p->indent = NULL;
      return i;
    }
    p = p->next;
  }
  return NULL;
}

/*
 *  do_space - issues an end of paragraph
 */

void html_text::do_space (void)
{
  if (is_in_pre()) {
    if (blank_para)
      start_space = TRUE;
    else {
      do_emittext("", 0);
      out->nl();
      space_emitted = TRUE;
    }
  } else {
    html_indent *i = remove_indent(P_TAG);

    do_para(done_para(), i);
    space_emitted = TRUE;
    start_space = TRUE;
  }
}

/*
 *  do_break - issue a break tag.
 */

void html_text::do_break (void)
{
  if (! is_present(PRE_TAG)) {
    if (emitted_text()) {
      if (! is_present(BREAK_TAG)) {
	push_para(BREAK_TAG);
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
  return !space_emitted;
}

/*
 *  ever_emitted_text - returns TRUE if we have ever emitted text in this paragraph.
 */

int html_text::ever_emitted_text (void)
{
  return !blank_para;
}

/*
 *  starts_with_space - returns TRUE if we have start this paragraph with a .sp
 */

int html_text::starts_with_space (void)
{
  return start_space;
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
    } else if (l == 0)
      error("stack list pointers are wrong");
    else {
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
    if (q->text_emitted)
      return TRUE;
    else
      q = q->next;
  }
  return FALSE;
}

/*
 *  remove_para_align - removes a paragraph which has a text
 *                      argument. If the paragraph has no text
 *                      argument then it is left alone.
 */

void html_text::remove_para_align (void)
{
  if (is_present(P_TAG)) {
    tag_definition *p=stackptr;

    while (p != NULL) {
      if (p->type == P_TAG && p->arg1 != NULL) {
	html_indent *i = remove_indent(P_TAG);
	done_para();
	do_para("", i);
	return;
      }
      p = p->next;
    }
  }
}

/*
 *  do_small - potentially inserts a <small> tag into the html stream.
 *             However we check for a <big> tag, if present then we terminate it.
 *             Otherwise a <small> tag is inserted.
 */

void html_text::do_small (void)
{
  if (is_present(BIG_TAG))
    done_big();
  else
    push_para(SMALL_TAG);
}

/*
 *  do_big - is the mirror image of do_small.
 */

void html_text::do_big (void)
{
  if (is_present(SMALL_TAG))
    done_small();
  else
    push_para(BIG_TAG);
}

/*
 *  do_sup - save a superscript tag on the stack of tags.
 */

void html_text::do_sup (void)
{
  push_para(SUP_TAG);
}

/*
 *  do_sub - save a subscript tag on the stack of tags.
 */

void html_text::do_sub (void)
{
  push_para(SUB_TAG);
}

