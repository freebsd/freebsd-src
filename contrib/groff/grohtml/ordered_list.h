/*
 * Copyright (C) 1999 Free Software Foundation, Inc.
 *
 *  Ordered list, a template module for simple ordered list manipulation.
 *
 *  Gaius Mulley (gaius@glam.ac.uk)
 */

template <class T> class list_element
{
 public:
  list_element *right;
  list_element *left;

                list_element (T *in);
  T            *data;
};

template <class T> class ordered_list
{
 private:
  list_element<T> *head;
  list_element<T> *tail;
  list_element<T> *ptr;
 public:
                   ordered_list        (void);
  ~                ordered_list        (void);
  void             add                 (T* in);
  void             sub_move_right      (void);
  void             move_right          (void);
  void             move_left           (void);
  int              is_empty            (void);
  int              is_equal_to_tail    (void);
  int              is_equal_to_head    (void);
  void             start_from_head     (void);
  void             start_from_tail     (void);
  T               *move_right_get_data (void);
  T               *move_left_get_data  (void);
  T               *get_data            (void);
};


template <class T> ordered_list<T>::ordered_list()
  : head(0), tail(0), ptr(0)
{
}

template <class T> ordered_list<T>::~ordered_list()
{
  list_element<T> *temp=head;

  do {
    temp = head;
    if (temp != 0) {
      head = head->right;
      delete temp;
    }
  } while ((head != 0) && (head != tail));
}

template <class T> list_element<T>::list_element(T *in)
  : right(0), left(0)
{
  data = in;
}

template <class T> void ordered_list<T>::add(T *in)
{
  list_element<T> *t    = new list_element<T>(in);   // create a new list element with data field initialized
  list_element<T> *last;

  if (in == 0) {
    fatal("cannot add NULL to ordered list");
  }

  if (head == 0) {
    head     = t;
    tail     = t;
    t->left  = t;
    t->right = t;
  } else {
    last = tail;

    while ((last != head) && (in->is_less(in, last->data))) {
      last = last->left;
    }

    if (in->is_less(in, last->data)) {
      t->right          = last;
      last->left->right = t;
      t->left           = last->left;
      last->left        = t;
      // now check for a new head
      if (last == head) {
	head = t;
      }
    } else {
      // add t onto beyond last
      t->right          = last->right;
      t->left           = last;
      last->right->left = t;
      last->right       = t;
      // now check for a new tail
      if (last == tail) {
	tail = t;
      }
    }
  }
}

template <class T> void ordered_list<T>::sub_move_right (void)
{
  list_element<T> *t=ptr->right;

  if (head == tail) {
    head = 0;
    if (tail != 0) {
      delete tail;
    }
    tail = 0;
    ptr  = 0;
  } else {
    if (head == ptr) {
      head = head->right;
    }
    if (tail == ptr) {
      tail = tail->left;
    }
    ptr->left->right = ptr->right;
    ptr->right->left = ptr->left;
    ptr=t;
  }
}

template <class T> void ordered_list<T>::start_from_head (void)
{
  ptr = head;
}

template <class T> void ordered_list<T>::start_from_tail (void)
{
  ptr = tail;
}

template <class T> int ordered_list<T>::is_empty (void)
{
  return( head == 0 );
}

template <class T> int ordered_list<T>::is_equal_to_tail (void)
{
  return( ptr == tail );
}

template <class T> int ordered_list<T>::is_equal_to_head (void)
{
  return( ptr == head );
}

template <class T> void ordered_list<T>::move_left (void)
{
  ptr = ptr->left;
}

template <class T> void ordered_list<T>::move_right (void)
{
  ptr = ptr->right;
}

template <class T> T* ordered_list<T>::get_data (void)
{
  return( ptr->data );
}

template <class T> T* ordered_list<T>::move_right_get_data (void)
{
  ptr = ptr->right;
  if (ptr == head) {
    return( 0 );
  } else {
    return( ptr->data );
  }
}

template <class T> T* ordered_list<T>::move_left_get_data (void)
{
  ptr = ptr->left;
  if (ptr == tail) {
    return( 0 );
  } else {
    return( ptr->data );
  }
}
