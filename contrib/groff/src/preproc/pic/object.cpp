// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2001, 2002, 2003
     Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

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

#include "pic.h"
#include "ptable.h"
#include "object.h"

void print_object_list(object *);

line_type::line_type()
: type(solid), thickness(1.0)
{
}

output::output() : args(0), desired_height(0.0), desired_width(0.0)
{
}

output::~output()
{
  a_delete args;
}

void output::set_desired_width_height(double wid, double ht)
{
  desired_width = wid;
  desired_height = ht;
}

void output::set_args(const char *s)
{
  a_delete args;
  if (s == 0 || *s == '\0')
    args = 0;
  else
    args = strsave(s);
}

int output::supports_filled_polygons()
{
  return 0;
}

void output::begin_block(const position &, const position &)
{
}

void output::end_block()
{
}

double output::compute_scale(double sc, const position &ll, const position &ur)
{
  distance dim = ur - ll;
  if (desired_width != 0.0 || desired_height != 0.0) {
    sc = 0.0;
    if (desired_width != 0.0) {
      if (dim.x == 0.0)
	error("width specified for picture with zero width");
      else
	sc = dim.x/desired_width;
    }
    if (desired_height != 0.0) {
      if (dim.y == 0.0)
	error("height specified for picture with zero height");
      else {
	double tem = dim.y/desired_height;
	if (tem > sc)
	  sc = tem;
      }
    }
    return sc == 0.0 ? 1.0 : sc;
  }
  else {
    if (sc <= 0.0)
      sc = 1.0;
    distance sdim = dim/sc;
    double max_width = 0.0;
    lookup_variable("maxpswid", &max_width);
    double max_height = 0.0;
    lookup_variable("maxpsht", &max_height);
    if ((max_width > 0.0 && sdim.x > max_width)
	|| (max_height > 0.0 && sdim.y > max_height)) {
      double xscale = dim.x/max_width;
      double yscale = dim.y/max_height;
      return xscale > yscale ? xscale : yscale;
    }
    else
      return sc;
  }
}

position::position(const place &pl)
{
  if (pl.obj != 0) {
    // Use two statements to work around bug in SGI C++.
    object *tem = pl.obj;
    *this = tem->origin();
  }
  else {
    x = pl.x;
    y = pl.y;
  }
}

position::position() : x(0.0), y(0.0)
{
}

position::position(double a, double b) : x(a), y(b)
{
}


int operator==(const position &a, const position &b)
{
  return a.x == b.x && a.y == b.y;
}

int operator!=(const position &a, const position &b)
{
  return a.x != b.x || a.y != b.y;
}

position &position::operator+=(const position &a)
{
  x += a.x;
  y += a.y;
  return *this;
}

position &position::operator-=(const position &a)
{
  x -= a.x;
  y -= a.y;
  return *this;
}

position &position::operator*=(double a)
{
  x *= a;
  y *= a;
  return *this;
}

position &position::operator/=(double a)
{
  x /= a;
  y /= a;
  return *this;
}

position operator-(const position &a)
{
  return position(-a.x, -a.y);
}

position operator+(const position &a, const position &b)
{
  return position(a.x + b.x, a.y + b.y);
}

position operator-(const position &a, const position &b)
{
  return position(a.x - b.x, a.y - b.y);
}

position operator/(const position &a, double n)
{
  return position(a.x/n, a.y/n);
}

position operator*(const position &a, double n)
{
  return position(a.x*n, a.y*n);
}

// dot product

double operator*(const position &a, const position &b)
{
  return a.x*b.x + a.y*b.y;
}

double hypot(const position &a)
{
  return hypot(a.x, a.y);
}

struct arrow_head_type {
  double height;
  double width;
  int solid;
};

void draw_arrow(const position &pos, const distance &dir,
		const arrow_head_type &aht, const line_type &lt,
		char *outline_color_for_fill)
{
  double hyp = hypot(dir);
  if (hyp == 0.0) {
    error("cannot draw arrow on object with zero length");
    return;
  }
  position base = -dir;
  base *= aht.height/hyp;
  position n(dir.y, -dir.x);
  n *= aht.width/(hyp*2.0);
  line_type slt = lt;
  slt.type = line_type::solid;
  if (aht.solid && out->supports_filled_polygons()) {
    position v[3];
    v[0] = pos;
    v[1] = pos + base + n;
    v[2] = pos + base - n;
    // fill with outline color
    out->set_color(outline_color_for_fill, outline_color_for_fill);
    out->polygon(v, 3, slt, 1);
  }
  else {
    position v[2];
    v[0] = pos;
    v[1] = pos + base + n;
    out->line(pos + base - n, v, 2, slt);
  }
}

object::object() : prev(0), next(0)
{
}

object::~object()
{
}

void object::move_by(const position &)
{
}

void object::print()
{
}

void object::print_text()
{
}

int object::blank()
{
  return 0;
}

struct bounding_box {
  int blank;
  position ll;
  position ur;

  bounding_box();
  void encompass(const position &);
};

bounding_box::bounding_box()
: blank(1)
{
}

void bounding_box::encompass(const position &pos)
{
  if (blank) {
    ll = pos;
    ur = pos;
    blank = 0;
  }
  else {
    if (pos.x < ll.x)
      ll.x = pos.x;
    if (pos.y < ll.y)
      ll.y = pos.y;
    if (pos.x > ur.x)
      ur.x = pos.x;
    if (pos.y > ur.y)
      ur.y = pos.y;
  }
}

void object::update_bounding_box(bounding_box *)
{
}

position object::origin()
{
  return position(0.0,0.0);
}

position object::north()
{
  return origin();
}

position object::south()
{
  return origin();
}

position object::east()
{
  return origin();
}

position object::west()
{
  return origin();
}

position object::north_east()
{
  return origin();
}

position object::north_west()
{
  return origin();
}

position object::south_east()
{
  return origin();
}

position object::south_west()
{
  return origin();
}

position object::start()
{
  return origin();
}

position object::end()
{
  return origin();
}

position object::center()
{
  return origin();
}

double object::width()
{
  return 0.0;
}

double object::radius()
{
  return 0.0;
}

double object::height()
{
  return 0.0;
}

place *object::find_label(const char *)
{
  return 0;
}

segment::segment(const position &a, int n, segment *p)
: is_absolute(n), pos(a), next(p)
{
}

text_item::text_item(char *t, const char *fn, int ln)
: next(0), text(t), filename(fn), lineno(ln) 
{
  adj.h = CENTER_ADJUST;
  adj.v = NONE_ADJUST;
}

text_item::~text_item()
{
  a_delete text;
}

object_spec::object_spec(object_type t) : type(t)
{
  flags = 0;
  tbl = 0;
  segment_list = 0;
  segment_width = segment_height = 0.0;
  segment_is_absolute = 0;
  text = 0;
  shaded = 0;
  outlined = 0;
  with = 0;
  dir = RIGHT_DIRECTION;
}

object_spec::~object_spec()
{
  delete tbl;
  while (segment_list != 0) {
    segment *tem = segment_list;
    segment_list = segment_list->next;
    delete tem;
  }
  object *p = oblist.head;
  while (p != 0) {
    object *tem = p;
    p = p->next;
    delete tem;
  }
  while (text != 0) {
    text_item *tem = text;
    text = text->next;
    delete tem;
  }
  delete with;
  a_delete shaded;
  a_delete outlined;
}

class command_object : public object {
  char *s;
  const char *filename;
  int lineno;
public:
  command_object(char *, const char *, int);
  ~command_object();
  object_type type() { return OTHER_OBJECT; }
  void print();
};

command_object::command_object(char *p, const char *fn, int ln)
: s(p), filename(fn), lineno(ln)
{
}

command_object::~command_object()
{
  a_delete s;
}

void command_object::print()
{
  out->command(s, filename, lineno);
}

object *make_command_object(char *s, const char *fn, int ln)
{
  return new command_object(s, fn, ln);
}

class mark_object : public object {
public:
  mark_object();
  object_type type();
};

object *make_mark_object()
{
  return new mark_object();
}

mark_object::mark_object()
{
}

object_type mark_object::type()
{
  return MARK_OBJECT;
}

object_list::object_list() : head(0), tail(0)
{
}

void object_list::append(object *obj)
{
  if (tail == 0) {
    obj->next = obj->prev = 0;
    head = tail = obj;
  }
  else {
    obj->prev = tail;
    obj->next = 0;
    tail->next = obj;
    tail = obj;
  }
}

void object_list::wrap_up_block(object_list *ol)
{
  object *p;
  for (p = tail; p && p->type() != MARK_OBJECT; p = p->prev)
    ;
  assert(p != 0);
  ol->head = p->next;
  if (ol->head) {
    ol->tail = tail;
    ol->head->prev = 0;
  }
  else
    ol->tail = 0;
  tail = p->prev;
  if (tail)
    tail->next = 0;
  else
    head = 0;
  delete p;
}

text_piece::text_piece()
: text(0), filename(0), lineno(-1)
{
  adj.h = CENTER_ADJUST;
  adj.v = NONE_ADJUST;
}

text_piece::~text_piece()
{
  a_delete text;
}

class graphic_object : public object {
  int ntext;
  text_piece *text;
  int aligned;
protected:
  line_type lt;
  char *outline_color;
  char *color_fill;
public:
  graphic_object();
  ~graphic_object();
  object_type type() = 0;
  void print_text();
  void add_text(text_item *, int);
  void set_dotted(double);
  void set_dashed(double);
  void set_thickness(double);
  void set_invisible();
  void set_outline_color(char *);
  char *get_outline_color();
  virtual void set_fill(double);
  virtual void set_fill_color(char *);
};

graphic_object::graphic_object()
: ntext(0), text(0), aligned(0), outline_color(0), color_fill(0)
{
}

void graphic_object::set_dotted(double wid)
{
  lt.type = line_type::dotted;
  lt.dash_width = wid;
}

void graphic_object::set_dashed(double wid)
{
  lt.type = line_type::dashed;
  lt.dash_width = wid;
}

void graphic_object::set_thickness(double th)
{
  lt.thickness = th;
}

void graphic_object::set_fill(double)
{
}

void graphic_object::set_fill_color(char *c)
{
  color_fill = strsave(c);
}

void graphic_object::set_outline_color(char *c)
{
  outline_color = strsave(c);
}

char *graphic_object::get_outline_color()
{
  return outline_color;
}

void graphic_object::set_invisible()
{
  lt.type = line_type::invisible;
}

void graphic_object::add_text(text_item *t, int a)
{
  aligned = a;
  int len = 0;
  text_item *p;
  for (p = t; p; p = p->next)
    len++;
  if (len == 0)
    text = 0;
  else {
    text = new text_piece[len];
    for (p = t, len = 0; p; p = p->next, len++) {
      text[len].text = p->text;
      p->text = 0;
      text[len].adj = p->adj;
      text[len].filename = p->filename;
      text[len].lineno = p->lineno;
    }
  }
  ntext = len;
}

void graphic_object::print_text()
{
  double angle = 0.0;
  if (aligned) {
    position d(end() - start());
    if (d.x != 0.0 || d.y != 0.0)
      angle = atan2(d.y, d.x);
  }
  if (text != 0) {
    out->set_color(color_fill, get_outline_color());
    out->text(center(), text, ntext, angle);
    out->reset_color();
  }
}

graphic_object::~graphic_object()
{
  if (text)
    ad_delete(ntext) text;
}

class rectangle_object : public graphic_object {
protected:
  position cent;
  position dim;
public:
  rectangle_object(const position &);
  double width() { return dim.x; }
  double height() { return dim.y; }
  position origin() { return cent; }
  position center() { return cent; }
  position north() { return position(cent.x, cent.y + dim.y/2.0); }
  position south() { return position(cent.x, cent.y - dim.y/2.0); }
  position east() { return position(cent.x + dim.x/2.0, cent.y); }
  position west() { return position(cent.x - dim.x/2.0, cent.y); }
  position north_east() { return position(cent.x + dim.x/2.0, cent.y + dim.y/2.0); }
  position north_west() { return position(cent.x - dim.x/2.0, cent.y + dim.y/2.0); }
  position south_east() { return position(cent.x + dim.x/2.0, cent.y - dim.y/2.0); }
  position south_west() { return position(cent.x - dim.x/2.0, cent.y - dim.y/2.0); }
  object_type type() = 0;
  void update_bounding_box(bounding_box *);
  void move_by(const position &);
};

rectangle_object::rectangle_object(const position &d)
: dim(d)
{
}

void rectangle_object::update_bounding_box(bounding_box *p)
{
  p->encompass(cent - dim/2.0);
  p->encompass(cent + dim/2.0);
}

void rectangle_object::move_by(const position &a)
{
  cent += a;
}

class closed_object : public rectangle_object {
public:
  closed_object(const position &);
  object_type type() = 0;
  void set_fill(double);
  void set_fill_color(char *fill);
protected:
  double fill;			// < 0 if not filled
  char *color_fill;		// = 0 if not colored
};

closed_object::closed_object(const position &pos)
: rectangle_object(pos), fill(-1.0), color_fill(0)
{
}

void closed_object::set_fill(double f)
{
  assert(f >= 0.0);
  fill = f;
}

void closed_object::set_fill_color(char *fill)
{
  color_fill = strsave(fill);
}

class box_object : public closed_object {
  double xrad;
  double yrad;
public:
  box_object(const position &, double);
  object_type type() { return BOX_OBJECT; }
  void print();
  position north_east();
  position north_west();
  position south_east();
  position south_west();
};

box_object::box_object(const position &pos, double r)
: closed_object(pos), xrad(dim.x > 0 ? r : -r), yrad(dim.y > 0 ? r : -r)
{
}

const double CHOP_FACTOR = 1.0 - 1.0/M_SQRT2;

position box_object::north_east()
{
  return position(cent.x + dim.x/2.0 - CHOP_FACTOR*xrad,
		  cent.y + dim.y/2.0 - CHOP_FACTOR*yrad);
}

position box_object::north_west()
{
  return position(cent.x - dim.x/2.0 + CHOP_FACTOR*xrad,
		  cent.y + dim.y/2.0 - CHOP_FACTOR*yrad);
}

position box_object::south_east()
{
  return position(cent.x + dim.x/2.0 - CHOP_FACTOR*xrad,
		  cent.y - dim.y/2.0 + CHOP_FACTOR*yrad);
}

position box_object::south_west()
{
  return position(cent.x - dim.x/2.0 + CHOP_FACTOR*xrad,
		  cent.y - dim.y/2.0 + CHOP_FACTOR*yrad);
}

void box_object::print()
{
  if (lt.type == line_type::invisible && fill < 0.0 && color_fill == 0)
    return;
  out->set_color(color_fill, graphic_object::get_outline_color());
  if (xrad == 0.0) {
    distance dim2 = dim/2.0;
    position vec[4];
    vec[0] = cent + position(dim2.x, -dim2.y);
    vec[1] = cent + position(dim2.x, dim2.y);
    vec[2] = cent + position(-dim2.x, dim2.y);
    vec[3] = cent + position(-dim2.x, -dim2.y);
    out->polygon(vec, 4, lt, fill);
  }
  else {
    distance abs_dim(fabs(dim.x), fabs(dim.y));
    out->rounded_box(cent, abs_dim, fabs(xrad), lt, fill);
  }
  out->reset_color();
}

graphic_object *object_spec::make_box(position *curpos, direction *dirp)
{
  static double last_box_height;
  static double last_box_width;
  static double last_box_radius;
  static int have_last_box = 0;
  if (!(flags & HAS_HEIGHT)) {
    if ((flags & IS_SAME) && have_last_box)
      height = last_box_height;
    else
      lookup_variable("boxht", &height);
  }
  if (!(flags & HAS_WIDTH)) {
    if ((flags & IS_SAME) && have_last_box)
      width = last_box_width;
    else
      lookup_variable("boxwid", &width);
  }
  if (!(flags & HAS_RADIUS)) {
    if ((flags & IS_SAME) && have_last_box)
      radius = last_box_radius;
    else
      lookup_variable("boxrad", &radius);
  }
  last_box_width = width;
  last_box_height = height;
  last_box_radius = radius;
  have_last_box = 1;
  radius = fabs(radius);
  if (radius*2.0 > fabs(width))
    radius = fabs(width/2.0);
  if (radius*2.0 > fabs(height))
    radius = fabs(height/2.0);
  box_object *p = new box_object(position(width, height), radius);
  if (!position_rectangle(p, curpos, dirp)) {
    delete p;
    p = 0;
  }
  return p;
}

// return non-zero for success

int object_spec::position_rectangle(rectangle_object *p,
				    position *curpos, direction *dirp)
{
  position pos;
  dir = *dirp;			// ignore any direction in attribute list
  position motion;
  switch (dir) {
  case UP_DIRECTION:
    motion.y = p->height()/2.0;
    break;
  case DOWN_DIRECTION:
    motion.y = -p->height()/2.0;
    break;
  case LEFT_DIRECTION:
    motion.x = -p->width()/2.0;
    break;
  case RIGHT_DIRECTION:
    motion.x = p->width()/2.0;
    break;
  default:
    assert(0);
  }
  if (flags & HAS_AT) {
    pos = at;
    if (flags & HAS_WITH) {
      place offset;
      place here;
      here.obj = p;
      if (!with->follow(here, &offset))
	return 0;
      pos -= offset;
    }
  }
  else {
    pos = *curpos;
    pos += motion;
  }
  p->move_by(pos);
  pos += motion;
  *curpos = pos;
  return 1;
}

class block_object : public rectangle_object {
  object_list oblist;
  PTABLE(place) *tbl;
public:
  block_object(const position &, const object_list &ol, PTABLE(place) *t);
  ~block_object();
  place *find_label(const char *);
  object_type type();
  void move_by(const position &);
  void print();
};

block_object::block_object(const position &d, const object_list &ol,
			   PTABLE(place) *t)
: rectangle_object(d), oblist(ol), tbl(t)
{
}

block_object::~block_object()
{
  delete tbl;
  object *p = oblist.head;
  while (p != 0) {
    object *tem = p;
    p = p->next;
    delete tem;
  }
}

void block_object::print()
{
  out->begin_block(south_west(), north_east());
  print_object_list(oblist.head);
  out->end_block();
}

static void adjust_objectless_places(PTABLE(place) *tbl, const position &a)
{
  // Adjust all the labels that aren't attached to objects.
  PTABLE_ITERATOR(place) iter(tbl);
  const char *key;
  place *pl;
  while (iter.next(&key, &pl))
    if (key && csupper(key[0]) && pl->obj == 0) {
      pl->x += a.x;
      pl->y += a.y;
    }
}

void block_object::move_by(const position &a)
{
  cent += a;
  for (object *p = oblist.head; p; p = p->next)
    p->move_by(a);
  adjust_objectless_places(tbl, a);
}


place *block_object::find_label(const char *name)
{
  return tbl->lookup(name);
}

object_type block_object::type()
{
  return BLOCK_OBJECT;
}

graphic_object *object_spec::make_block(position *curpos, direction *dirp)
{
  bounding_box bb;
  for (object *p = oblist.head; p; p = p->next)
    p->update_bounding_box(&bb);
  position dim;
  if (!bb.blank) {
    position m = -(bb.ll + bb.ur)/2.0;
    for (object *p = oblist.head; p; p = p->next)
      p->move_by(m);
    adjust_objectless_places(tbl, m);
    dim = bb.ur - bb.ll;
  }
  if (flags & HAS_WIDTH)
    dim.x = width;
  if (flags & HAS_HEIGHT)
    dim.y = height;
  block_object *block = new block_object(dim, oblist, tbl);
  if (!position_rectangle(block, curpos, dirp)) {
    delete block;
    block = 0;
  }
  tbl = 0;
  oblist.head = oblist.tail = 0;
  return block;
}

class text_object : public rectangle_object {
public:
  text_object(const position &);
  object_type type() { return TEXT_OBJECT; }
};

text_object::text_object(const position &d)
: rectangle_object(d)
{
}

graphic_object *object_spec::make_text(position *curpos, direction *dirp)
{
  if (!(flags & HAS_HEIGHT)) {
    lookup_variable("textht", &height);
    int nitems = 0;
    for (text_item *t = text; t; t = t->next)
      nitems++;
    height *= nitems;
  }
  if (!(flags & HAS_WIDTH))
    lookup_variable("textwid", &width);
  text_object *p = new text_object(position(width, height));
  if (!position_rectangle(p, curpos, dirp)) {
    delete p;
    p = 0;
  }
  return p;
}


class ellipse_object : public closed_object {
public:
  ellipse_object(const position &);
  position north_east() { return position(cent.x + dim.x/(M_SQRT2*2.0),
					  cent.y + dim.y/(M_SQRT2*2.0)); }
  position north_west() { return position(cent.x - dim.x/(M_SQRT2*2.0),
					  cent.y + dim.y/(M_SQRT2*2.0)); }
  position south_east() { return position(cent.x + dim.x/(M_SQRT2*2.0),
					  cent.y - dim.y/(M_SQRT2*2.0)); }
  position south_west() { return position(cent.x - dim.x/(M_SQRT2*2.0),
					  cent.y - dim.y/(M_SQRT2*2.0)); }
  double radius() { return dim.x/2.0; }
  object_type type() { return ELLIPSE_OBJECT; }
  void print();
};

ellipse_object::ellipse_object(const position &d)
: closed_object(d)
{
}

void ellipse_object::print()
{
  if (lt.type == line_type::invisible && fill < 0.0 && color_fill == 0)
    return;
  out->set_color(color_fill, graphic_object::get_outline_color());
  out->ellipse(cent, dim, lt, fill);
  out->reset_color();
}

graphic_object *object_spec::make_ellipse(position *curpos, direction *dirp)
{
  static double last_ellipse_height;
  static double last_ellipse_width;
  static int have_last_ellipse = 0;
  if (!(flags & HAS_HEIGHT)) {
    if ((flags & IS_SAME) && have_last_ellipse)
      height = last_ellipse_height;
    else
      lookup_variable("ellipseht", &height);
  }
  if (!(flags & HAS_WIDTH)) {
    if ((flags & IS_SAME) && have_last_ellipse)
      width = last_ellipse_width;
    else
      lookup_variable("ellipsewid", &width);
  }
  last_ellipse_width = width;
  last_ellipse_height = height;
  have_last_ellipse = 1;
  ellipse_object *p = new ellipse_object(position(width, height));
  if (!position_rectangle(p, curpos, dirp)) {
    delete p;
    return 0;
  }
  return p;
}

class circle_object : public ellipse_object {
public:
  circle_object(double);
  object_type type() { return CIRCLE_OBJECT; }
  void print();
};

circle_object::circle_object(double diam)
: ellipse_object(position(diam, diam))
{
}

void circle_object::print()
{
  if (lt.type == line_type::invisible && fill < 0.0 && color_fill == 0)
    return;
  out->set_color(color_fill, graphic_object::get_outline_color());
  out->circle(cent, dim.x/2.0, lt, fill);
  out->reset_color();
}

graphic_object *object_spec::make_circle(position *curpos, direction *dirp)
{
  static double last_circle_radius;
  static int have_last_circle = 0;
  if (!(flags & HAS_RADIUS)) {
    if ((flags & IS_SAME) && have_last_circle)
      radius = last_circle_radius;
    else
      lookup_variable("circlerad", &radius);
  }
  last_circle_radius = radius;
  have_last_circle = 1;
  circle_object *p = new circle_object(radius*2.0);
  if (!position_rectangle(p, curpos, dirp)) {
    delete p;
    return 0;
  }
  return p;
}

class move_object : public graphic_object {
  position strt;
  position en;
public:
  move_object(const position &s, const position &e);
  position origin() { return en; }
  object_type type() { return MOVE_OBJECT; }
  void update_bounding_box(bounding_box *);
  void move_by(const position &);
};

move_object::move_object(const position &s, const position &e)
: strt(s), en(e)
{
}

void move_object::update_bounding_box(bounding_box *p)
{
  p->encompass(strt);
  p->encompass(en);
}

void move_object::move_by(const position &a)
{
  strt += a;
  en += a;
}

graphic_object *object_spec::make_move(position *curpos, direction *dirp)
{
  static position last_move;
  static int have_last_move = 0;
  *dirp = dir;
  // No need to look at at since `at' attribute sets `from' attribute.
  position startpos = (flags & HAS_FROM) ? from : *curpos;
  if (!(flags & HAS_SEGMENT)) {
    if ((flags & IS_SAME) && have_last_move)
      segment_pos = last_move;
    else {
      switch (dir) {
      case UP_DIRECTION:
	segment_pos.y = segment_height;
	break;
      case DOWN_DIRECTION:
	segment_pos.y = -segment_height;
	break;
      case LEFT_DIRECTION:
	segment_pos.x = -segment_width;
	break;
      case RIGHT_DIRECTION:
	segment_pos.x = segment_width;
	break;
      default:
	assert(0);
      }
    }
  }
  segment_list = new segment(segment_pos, segment_is_absolute, segment_list);
  // Reverse the segment_list so that it's in forward order.
  segment *old = segment_list;
  segment_list = 0;
  while (old != 0) {
    segment *tem = old->next;
    old->next = segment_list;
    segment_list = old;
    old = tem;
  }
  // Compute the end position.
  position endpos = startpos;
  for (segment *s = segment_list; s; s = s->next)
    if (s->is_absolute)
      endpos = s->pos;
    else 
      endpos += s->pos;
  have_last_move = 1;
  last_move = endpos - startpos;
  move_object *p = new move_object(startpos, endpos);
  *curpos = endpos;
  return p;
}

class linear_object : public graphic_object {
protected:
  char arrow_at_start;
  char arrow_at_end;
  arrow_head_type aht;
  position strt;
  position en;
public:
  linear_object(const position &s, const position &e);
  position start() { return strt; }
  position end() { return en; }
  void move_by(const position &);
  void update_bounding_box(bounding_box *) = 0;
  object_type type() = 0;
  void add_arrows(int at_start, int at_end, const arrow_head_type &);
};

class line_object : public linear_object {
protected:
  position *v;
  int n;
public:
  line_object(const position &s, const position &e, position *, int);
  ~line_object();
  position origin() { return strt; }
  position center() { return (strt + en)/2.0; }
  position north() { return (en.y - strt.y) > 0 ? en : strt; }
  position south() { return (en.y - strt.y) < 0 ? en : strt; }
  position east() { return (en.x - strt.x) > 0 ? en : strt; }
  position west() { return (en.x - strt.x) < 0 ? en : strt; }
  object_type type() { return LINE_OBJECT; }
  void update_bounding_box(bounding_box *);
  void print();
  void move_by(const position &);
};

class arrow_object : public line_object {
public:
  arrow_object(const position &, const position &, position *, int);
  object_type type() { return ARROW_OBJECT; }
};

class spline_object : public line_object {
public:
  spline_object(const position &, const position &, position *, int);
  object_type type() { return SPLINE_OBJECT; }
  void print();
  void update_bounding_box(bounding_box *);
};

linear_object::linear_object(const position &s, const position &e)
: arrow_at_start(0), arrow_at_end(0), strt(s), en(e)
{
}

void linear_object::move_by(const position &a)
{
  strt += a;
  en += a;
}

void linear_object::add_arrows(int at_start, int at_end,
			       const arrow_head_type &a)
{
  arrow_at_start = at_start;
  arrow_at_end = at_end;
  aht = a;
}

line_object::line_object(const position &s, const position &e,
			 position *p, int i)
: linear_object(s, e), v(p), n(i)
{
}

void line_object::print()
{
  if (lt.type == line_type::invisible)
    return;
  out->set_color(0, graphic_object::get_outline_color());
  out->line(strt, v, n, lt);
  if (arrow_at_start)
    draw_arrow(strt, strt-v[0], aht, lt, graphic_object::get_outline_color());
  if (arrow_at_end)
    draw_arrow(en, v[n-1] - (n > 1 ? v[n - 2] : strt), aht, lt,
	       graphic_object::get_outline_color());
  out->reset_color();
}

void line_object::update_bounding_box(bounding_box *p)
{
  p->encompass(strt);
  for (int i = 0; i < n; i++)
    p->encompass(v[i]);
}

void line_object::move_by(const position &pos)
{
  linear_object::move_by(pos);
  for (int i = 0; i < n; i++)
    v[i] += pos;
}
  
void spline_object::update_bounding_box(bounding_box *p)
{
  p->encompass(strt);
  p->encompass(en);
  /*

  If

  p1 = q1/2 + q2/2
  p2 = q1/6 + q2*5/6
  p3 = q2*5/6 + q3/6
  p4 = q2/2 + q3/2
  [ the points for the Bezier cubic ]

  and

  t = .5

  then

  (1-t)^3*p1 + 3*t*(t - 1)^2*p2 + 3*t^2*(1-t)*p3 + t^3*p4
  [ the equation for the Bezier cubic ]

  = .125*q1 + .75*q2 + .125*q3

  */
  for (int i = 1; i < n; i++)
    p->encompass((i == 1 ? strt : v[i-2])*.125 + v[i-1]*.75 + v[i]*.125);
}

arrow_object::arrow_object(const position &s, const position &e,
			   position *p, int i)
: line_object(s, e, p, i)
{
}

spline_object::spline_object(const position &s, const position &e,
			     position *p, int i)
: line_object(s, e, p, i)
{
}

void spline_object::print()
{
  if (lt.type == line_type::invisible)
    return;
  out->set_color(0, graphic_object::get_outline_color());
  out->spline(strt, v, n, lt);
  if (arrow_at_start)
    draw_arrow(strt, strt-v[0], aht, lt, graphic_object::get_outline_color());
  if (arrow_at_end)
    draw_arrow(en, v[n-1] - (n > 1 ? v[n - 2] : strt), aht, lt,
	       graphic_object::get_outline_color());
  out->reset_color();
}

line_object::~line_object()
{
  a_delete v;
}

linear_object *object_spec::make_line(position *curpos, direction *dirp)
{
  static position last_line;
  static int have_last_line = 0;
  *dirp = dir;
  // No need to look at at since `at' attribute sets `from' attribute.
  position startpos = (flags & HAS_FROM) ? from : *curpos;
  if (!(flags & HAS_SEGMENT)) {
    if ((flags & IS_SAME) && (type == LINE_OBJECT || type == ARROW_OBJECT)
	&& have_last_line)
      segment_pos = last_line;
    else 
      switch (dir) {
      case UP_DIRECTION:
	segment_pos.y = segment_height;
	break;
      case DOWN_DIRECTION:
	segment_pos.y = -segment_height;
	break;
      case LEFT_DIRECTION:
	segment_pos.x = -segment_width;
	break;
      case RIGHT_DIRECTION:
	segment_pos.x = segment_width;
	break;
      default:
	assert(0);
      }
  }
  segment_list = new segment(segment_pos, segment_is_absolute, segment_list);
  // reverse the segment_list so that it's in forward order
  segment *old = segment_list;
  segment_list = 0;
  while (old != 0) {
    segment *tem = old->next;
    old->next = segment_list;
    segment_list = old;
    old = tem;
  }
  // Absolutise all movements
  position endpos = startpos;
  int nsegments = 0;
  segment *s;
  for (s = segment_list; s; s = s->next, nsegments++)
    if (s->is_absolute)
      endpos = s->pos;
    else {
      endpos += s->pos;
      s->pos = endpos;
      s->is_absolute = 1;	// to avoid confusion
    }
  // handle chop
  line_object *p = 0;
  position *v = new position[nsegments];
  int i = 0;
  for (s = segment_list; s; s = s->next, i++)
    v[i] = s->pos;
  if (flags & IS_DEFAULT_CHOPPED) {
    lookup_variable("circlerad", &start_chop);
    end_chop = start_chop;
    flags |= IS_CHOPPED;
  }
  if (flags & IS_CHOPPED) {
    position start_chop_vec, end_chop_vec;
    if (start_chop != 0.0) {
      start_chop_vec = v[0] - startpos;
      start_chop_vec *= start_chop / hypot(start_chop_vec);
    }
    if (end_chop != 0.0) {
      end_chop_vec = (v[nsegments - 1]
		      - (nsegments > 1 ? v[nsegments - 2] : startpos));
      end_chop_vec *= end_chop / hypot(end_chop_vec);
    }
    startpos += start_chop_vec;
    v[nsegments - 1] -= end_chop_vec;
    endpos -= end_chop_vec;
  }
  switch (type) {
  case SPLINE_OBJECT:
    p = new spline_object(startpos, endpos, v, nsegments);
    break;
  case ARROW_OBJECT:
    p = new arrow_object(startpos, endpos, v, nsegments);
    break;
  case LINE_OBJECT:
    p = new line_object(startpos, endpos, v, nsegments);
    break;
  default:
    assert(0);
  }
  have_last_line = 1;
  last_line = endpos - startpos;
  *curpos = endpos;
  return p;
}

class arc_object : public linear_object {
  int clockwise;
  position cent;
  double rad;
public:
  arc_object(int, const position &, const position &, const position &);
  position origin() { return cent; }
  position center() { return cent; }
  double radius() { return rad; }
  position north();
  position south();
  position east();
  position west();
  position north_east();
  position north_west();
  position south_east();
  position south_west();
  void update_bounding_box(bounding_box *);
  object_type type() { return ARC_OBJECT; }
  void print();
  void move_by(const position &pos);
};

arc_object::arc_object(int cw, const position &s, const position &e,
		       const position &c)
: linear_object(s, e), clockwise(cw), cent(c)
{
  rad = hypot(c - s);
}

void arc_object::move_by(const position &pos)
{
  linear_object::move_by(pos);
  cent += pos;
}

// we get arc corners from the corresponding circle

position arc_object::north()
{
  position result(cent);
  result.y += rad;
  return result;
}

position arc_object::south()
{
  position result(cent);
  result.y -= rad;
  return result;
}

position arc_object::east()
{
  position result(cent);
  result.x += rad;
  return result;
}

position arc_object::west()
{
  position result(cent);
  result.x -= rad;
  return result;
}

position arc_object::north_east()
{
  position result(cent);
  result.x += rad/M_SQRT2;
  result.y += rad/M_SQRT2;
  return result;
}

position arc_object::north_west()
{
  position result(cent);
  result.x -= rad/M_SQRT2;
  result.y += rad/M_SQRT2;
  return result;
}

position arc_object::south_east()
{
  position result(cent);
  result.x += rad/M_SQRT2;
  result.y -= rad/M_SQRT2;
  return result;
}

position arc_object::south_west()
{
  position result(cent);
  result.x -= rad/M_SQRT2;
  result.y -= rad/M_SQRT2;
  return result;
}


void arc_object::print()
{
  if (lt.type == line_type::invisible)
    return;
  out->set_color(0, graphic_object::get_outline_color());
  if (clockwise)
    out->arc(en, cent, strt, lt);
  else
    out->arc(strt, cent, en, lt);
  if (arrow_at_start) {
    position c = cent - strt;
    draw_arrow(strt,
	       (clockwise ? position(c.y, -c.x) : position(-c.y, c.x)),
	       aht, lt, graphic_object::get_outline_color());
  }
  if (arrow_at_end) {
    position e = en - cent;
    draw_arrow(en,
	       (clockwise ? position(e.y, -e.x) : position(-e.y, e.x)),
	       aht, lt, graphic_object::get_outline_color());
  }
  out->reset_color();
}

inline double max(double a, double b)
{
  return a > b ? a : b;
}

void arc_object::update_bounding_box(bounding_box *p)
{
  p->encompass(strt);
  p->encompass(en);
  position start_offset = strt - cent;
  if (start_offset.x == 0.0 && start_offset.y == 0.0)
    return;
  position end_offset = en  - cent;
  if (end_offset.x == 0.0 && end_offset.y == 0.0)
    return;
  double start_quad = atan2(start_offset.y, start_offset.x)/(M_PI/2.0);
  double end_quad = atan2(end_offset.y, end_offset.x)/(M_PI/2.0);
  if (clockwise) {
    double temp = start_quad;
    start_quad = end_quad;
    end_quad = temp;
  }
  if (start_quad < 0.0)
    start_quad += 4.0;
  while (end_quad <= start_quad)
    end_quad += 4.0;
  double radius = max(hypot(start_offset), hypot(end_offset));
  for (int q = int(start_quad) + 1; q < end_quad; q++) {
    position offset;
    switch (q % 4) {
    case 0:
      offset.x = radius;
      break;
    case 1:
      offset.y = radius;
      break;
    case 2:
      offset.x = -radius;
      break;
    case 3:
      offset.y = -radius;
      break;
    }
    p->encompass(cent + offset);
  }
}

// We ignore the with attribute. The at attribute always refers to the center.

linear_object *object_spec::make_arc(position *curpos, direction *dirp)
{
  *dirp = dir;
  int cw = (flags & IS_CLOCKWISE) != 0;
  // compute the start
  position startpos;
  if (flags & HAS_FROM)
    startpos = from;
  else
    startpos = *curpos;
  if (!(flags & HAS_RADIUS))
    lookup_variable("arcrad", &radius);
  // compute the end
  position endpos;
  if (flags & HAS_TO)
    endpos = to;
  else {
    position m(radius, radius);
    // Adjust the signs.
    if (cw) {
      if (dir == DOWN_DIRECTION || dir == LEFT_DIRECTION)
	m.x = -m.x;
      if (dir == DOWN_DIRECTION || dir == RIGHT_DIRECTION)
	m.y = -m.y;
      *dirp = direction((dir + 3) % 4);
    }
    else {
      if (dir == UP_DIRECTION || dir == LEFT_DIRECTION)
	m.x = -m.x;
      if (dir == DOWN_DIRECTION || dir == LEFT_DIRECTION)
	m.y = -m.y;
      *dirp = direction((dir + 1) % 4);
    }
    endpos = startpos + m;
  }
  // compute the center
  position centerpos;
  if (flags & HAS_AT)
    centerpos = at;
  else if (startpos == endpos)
    centerpos = startpos;
  else {
    position h = (endpos - startpos)/2.0;
    double d = hypot(h);
    if (radius <= 0)
      radius = .25;
    // make the radius big enough
    while (radius < d)
      radius *= 2.0;
    double alpha = acos(d/radius);
    double theta = atan2(h.y, h.x);
    if (cw)
      theta -= alpha;
    else
      theta += alpha;
    centerpos = position(cos(theta), sin(theta))*radius + startpos;
  }
  arc_object *p = new arc_object(cw, startpos, endpos, centerpos);
  *curpos = endpos;
  return p;
}

graphic_object *object_spec::make_linear(position *curpos, direction *dirp)
{
  linear_object *obj;
  if (type == ARC_OBJECT)
    obj = make_arc(curpos, dirp);
  else
    obj = make_line(curpos, dirp);
  if (type == ARROW_OBJECT
      && (flags & (HAS_LEFT_ARROW_HEAD|HAS_RIGHT_ARROW_HEAD)) == 0)
    flags |= HAS_RIGHT_ARROW_HEAD;
  if (obj && (flags & (HAS_LEFT_ARROW_HEAD|HAS_RIGHT_ARROW_HEAD))) {
    arrow_head_type a;
    int at_start = (flags & HAS_LEFT_ARROW_HEAD) != 0;
    int at_end = (flags & HAS_RIGHT_ARROW_HEAD) != 0;
    if (flags & HAS_HEIGHT)
      a.height = height;
    else
      lookup_variable("arrowht", &a.height);
    if (flags & HAS_WIDTH)
      a.width = width;
    else
      lookup_variable("arrowwid", &a.width);
    double solid;
    lookup_variable("arrowhead", &solid);
    a.solid = solid != 0.0;
    obj->add_arrows(at_start, at_end, a);
  }
  return obj;
}

object *object_spec::make_object(position *curpos, direction *dirp)
{
  graphic_object *obj = 0;
  switch (type) {
  case BLOCK_OBJECT:
    obj = make_block(curpos, dirp);
    break;
  case BOX_OBJECT:
    obj = make_box(curpos, dirp);
    break;
  case TEXT_OBJECT:
    obj = make_text(curpos, dirp);
    break;
  case ELLIPSE_OBJECT:
    obj = make_ellipse(curpos, dirp);
    break;
  case CIRCLE_OBJECT:
    obj = make_circle(curpos, dirp);
    break;
  case MOVE_OBJECT:
    obj = make_move(curpos, dirp);
    break;
  case ARC_OBJECT:
  case LINE_OBJECT:
  case SPLINE_OBJECT:
  case ARROW_OBJECT:
    obj = make_linear(curpos, dirp);
    break;
  case MARK_OBJECT:
  case OTHER_OBJECT:
  default:
    assert(0);
    break;
  }
  if (obj) {
    if (flags & IS_INVISIBLE)
      obj->set_invisible();
    if (text != 0)
      obj->add_text(text, (flags & IS_ALIGNED) != 0);
    if (flags & IS_DOTTED)
      obj->set_dotted(dash_width);
    else if (flags & IS_DASHED)
      obj->set_dashed(dash_width);
    double th;
    if (flags & HAS_THICKNESS)
      th = thickness;
    else
      lookup_variable("linethick", &th);
    obj->set_thickness(th);
    if (flags & IS_OUTLINED)
      obj->set_outline_color(outlined);
    if (flags & (IS_DEFAULT_FILLED|IS_FILLED)) {
      if (flags & IS_SHADED)
	obj->set_fill_color(shaded);
      else {
	if (flags & IS_DEFAULT_FILLED)
	  lookup_variable("fillval", &fill);
	if (fill < 0.0)
	  error("bad fill value %1", fill);
	else
	  obj->set_fill(fill);
      }
    }
  }
  return obj;
}

struct string_list {
  string_list *next;
  char *str;
  string_list(char *);
  ~string_list();
};

string_list::string_list(char *s)
: next(0), str(s)
{
}

string_list::~string_list()
{
  a_delete str;
}
  
/* A path is used to hold the argument to the `with' attribute.  For
   example, `.nw' or `.A.s' or `.A'.  The major operation on a path is to
   take a place and follow the path through the place to place within the
   place.  Note that `.A.B.C.sw' will work.

   For compatibility with DWB pic, `with' accepts positions also (this
   is incorrectly documented in CSTR 116). */

path::path(corner c)
: crn(c), label_list(0), ypath(0), is_position(0)
{
}

path::path(position p)
: crn(0), label_list(0), ypath(0), is_position(1)
{
  pos.x = p.x;
  pos.y = p.y;
}

path::path(char *l, corner c)
: crn(c), ypath(0), is_position(0)
{
  label_list = new string_list(l);
}

path::~path()
{
  while (label_list) {
    string_list *tem = label_list;
    label_list = label_list->next;
    delete tem;
  }
  delete ypath;
}

void path::append(corner c)
{
  assert(crn == 0);
  crn = c;
}

void path::append(char *s)
{
  string_list **p;
  for (p = &label_list; *p; p = &(*p)->next)
    ;
  *p = new string_list(s);
}

void path::set_ypath(path *p)
{
  ypath = p;
}

// return non-zero for success

int path::follow(const place &pl, place *result) const
{
  if (is_position) {
    result->x = pos.x;
    result->y = pos.y;
    result->obj = 0;
    return 1;
  }
  const place *p = &pl;
  for (string_list *lb = label_list; lb; lb = lb->next)
    if (p->obj == 0 || (p = p->obj->find_label(lb->str)) == 0) {
      lex_error("object does not contain a place `%1'", lb->str);
      return 0;
    }
  if (crn == 0 || p->obj == 0)
    *result = *p;
  else {
    position ps = ((p->obj)->*(crn))();
    result->x = ps.x;
    result->y = ps.y;
    result->obj = 0;
  }
  if (ypath) {
    place tem;
    if (!ypath->follow(pl, &tem))
      return 0;
    result->y = tem.y;
    if (result->obj != tem.obj)
      result->obj = 0;
  }
  return 1;
}

void print_object_list(object *p)
{
  for (; p; p = p->next) {
    p->print();
    p->print_text();
  }
}

void print_picture(object *obj)
{
  bounding_box bb;
  for (object *p = obj; p; p = p->next)
    p->update_bounding_box(&bb);
  double scale;
  lookup_variable("scale", &scale);
  out->start_picture(scale, bb.ll, bb.ur);
  print_object_list(obj);
  out->finish_picture();
}

