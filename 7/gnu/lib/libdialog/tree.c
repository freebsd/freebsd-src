/*
 * tree.c -- implements the 'tree' interface element for libdialog
 *
 * Author: Anatoly A. Orehovsky (tolik@mpeks.tomsk.su)
 *
 * Copyright (c) 1997, Anatoly A. Orehovsky
 * 09/28/98 - patched by Anatoly A. Orehovsky (smart_tree())
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <dialog.h>
#include "dialog.priv.h"
#include <ncurses.h>

/* static utils for make tree */
struct leaf {
	unsigned char *name;		/* name of leaf */
	unsigned char *branches;	/* branches that going by leaf */
	unsigned char slip;		/* slip of leaf*/
	int shift;			/* shift relative root of tree */
};

static int	mk_slip(struct leaf array[], int arr_size, 
			int number, int shift);

/* make tree from file
 *
 * filename	- name of file with like find(1) output
 * p_names	- pointer to array of strings
 * p_size	- pointer to size of array
 * FS		- fields separator
 * p_array	- pointer to array of leafs
 *
 * return values:
 * 0		- ok and names by p_names, size by p_size, array by p_array set
 * -1		- memory allocation error (errno set)
 */  

static int	mk_ftree(char *filename, 
		unsigned char ***p_names, int *p_size, unsigned char FS, 
			struct leaf **p_array);
		
/* make tree from array
 *
 * names	- array of strings
 * size		- size of array
 * FS		- fields separator
 * p_array	- pointer to array of leafs
 *
 * return values:
 * 0		- ok and array by p_array set
 * -1		- memory allocation error (errno set)
 */  
 
static int	mk_tree(unsigned char **names, int size, unsigned char FS, 
			struct leaf **p_array);

/* free memory from tree (leafs)
 *
 * return values:
 * nothing
 */

static void	free_leafs(struct leaf *array, int size);

/* free memory from source data for tree (names)
 *
 * return values:
 * if 0 <= choice <= size - pointer to name from names, 
 *	and memory for name not released (must be freed later)
 * else - NULL (recomended choice -1 for it)
 */

static unsigned char	*free_names(unsigned char **names, 
					int size, int choice);

/* end of static utils for make tree */

/* static utils for ftree */

/* control struct for queue */
struct queue {
	int size;			/* size of queue */
	struct m_queue *first;		/* begin of queue */
	struct m_queue *last;		/* end of queue */
};

/* queue member */
struct m_queue {
	void *pointer;			/* queue member */
	struct m_queue *next;		/* next queue member */
};

/* init struct queue by zeros */
static void	init_queue(struct queue *queue);

/* add pointer to queue */
/* return - pointer or NULL if error */
static void	*p2_queue(struct queue *queue, void *pointer);

/* get first from queue */
/* return - pointer or NULL if queue is empty */
static void	*first_queue(struct queue *queue);

/* make zero terminated array from queue */
/* return - pointer to array or NULL if error */
static void	**q2arr(struct queue *queue, int depth);

/* smart_tree (for like find(1) with -d flag output compliance) */
/* return - not NULL or NULL if malloc error */
static unsigned char	*smart_tree(struct queue *queue, unsigned char FS,
					unsigned char *current,
					unsigned char *prev);

/* end of static utils for ftree */

/* static utils for saved_tree */

/* saved values for unique tree */
struct saved_tree {
	unsigned char **names;	/* names + */ 
	int size;		/* size + */
	unsigned char FS;	/* FS + */
	int height;		/* height + */
	int width;		/* width + */
	int menu_height;	/* menu_height - unique for treebox ? */
	int ch;			/* saved ch - choice */
	int sc;			/* saved sc - scroll */
};

/* search saved tree within queue */
/* return - struct saved_tree * or NULL if not found */
static struct saved_tree *search_saved_tree(struct queue *queue, 
					unsigned char **names,
					int size,
					unsigned char FS,
					int height,
					int width,
					int menu_height);

/* end of static utils for saved_tree */

static void print_item(WINDOW *win, struct leaf item, int choice, int selected);

static void print_position(WINDOW *win, int x, int y,
				int cur_pos, int size);

static int menu_width, item_x;

static int dialog_treemenu(unsigned char *title, unsigned char *prompt, 
			int height, int width, int menu_height, 
				int item_no, struct leaf items[], 
					int *result, 
						int *ch, int *sc);

/*
 * Display a menu for choosing among a number of options
 */
static
int dialog_treemenu(unsigned char *title, unsigned char *prompt, 
			int height, int width, int menu_height, 
				int item_no, struct leaf items[], 
					int *result, 
						int *ch, int *sc)
{
  int i, j, x, y, cur_x, cur_y, box_x, box_y, key = 0, button = 0, choice = 0,
      l, scroll = 0, max_choice, redraw_menu = FALSE;
  WINDOW *dialog, *menu;

  if (ch)  /* restore menu item info */
      choice = *ch;
  if (sc)
      scroll = *sc;

  max_choice = MIN(menu_height, item_no);

  item_x = 0;
  /* Find length of longest item in order to center menu */
  for (i = 0; i < item_no; i++) {
    l = strlen(items[i].name) + strlen(items[i].branches) * 4 + 4;
    item_x = MAX(item_x, l);
  }
  
  if (height < 0)
	height = strheight(prompt)+menu_height+4+2;
  if (width < 0) {
	i = strwidth(prompt);
	j = ((title != NULL) ? strwidth(title) : 0);
	width = MAX(i,j);
	width = MAX(width,item_x+4)+4;
  }
  width = MAX(width,24);

  if (width > COLS)
	width = COLS;
  if (height > LINES)
	height = LINES;
  /* center dialog box on screen */
  x = (COLS - width)/2;
  y = (LINES - height)/2;

#ifdef HAVE_NCURSES
  if (use_shadow)
    draw_shadow(stdscr, y, x, height, width);
#endif
  dialog = newwin(height, width, y, x);
  if (dialog == NULL) {
    endwin();
    fprintf(stderr, "\nnewwin(%d,%d,%d,%d) failed, maybe wrong dims\n", height,width,y,x);
    exit(1);
  }
  keypad(dialog, TRUE);

  draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
  wattrset(dialog, border_attr);
  wmove(dialog, height-3, 0);
  waddch(dialog, ACS_LTEE);
  for (i = 0; i < width-2; i++)
    waddch(dialog, ACS_HLINE);
  wattrset(dialog, dialog_attr);
  waddch(dialog, ACS_RTEE);
  wmove(dialog, height-2, 1);
  for (i = 0; i < width-2; i++)
    waddch(dialog, ' ');

  if (title != NULL) {
    wattrset(dialog, title_attr);
    wmove(dialog, 0, (width - strlen(title))/2 - 1);
    waddch(dialog, ' ');
    waddstr(dialog, title);
    waddch(dialog, ' ');
  }
  wattrset(dialog, dialog_attr);
  wmove(dialog, 1, 2);
  print_autowrap(dialog, prompt, height-1, width-2, width, 1, 2, TRUE, FALSE);

  menu_width = width-6;
  getyx(dialog, cur_y, cur_x);
  box_y = cur_y + 1;
  box_x = (width - menu_width)/2 - 1;

  /* create new window for the menu */
  menu = subwin(dialog, menu_height, menu_width, y + box_y + 1, x + box_x + 1);
  if (menu == NULL) {
    endwin();
    fprintf(stderr, "\nsubwin(dialog,%d,%d,%d,%d) failed, maybe wrong dims\n", menu_height,menu_width,y+box_y+1,x+box_x+1);
    exit(1);
  }
  keypad(menu, TRUE);

  /* draw a box around the menu items */
  draw_box(dialog, box_y, box_x, menu_height+2, menu_width+2, menubox_border_attr, menubox_attr);

  item_x = 1;

  /* Print the menu */
  for (i = 0; i < max_choice; i++)
    print_item(menu, items[(scroll+i)], i, i == choice);
  wnoutrefresh(menu);
  print_arrows(dialog, scroll, menu_height, item_no, box_x, box_y, item_x, cur_x, cur_y);
  print_position(dialog, box_x+menu_width, box_y+menu_height, scroll+choice, item_no);

  display_helpline(dialog, height-1, width);

  x = width/2-11;
  y = height-2;
  print_button(dialog, "Cancel", y, x+14, FALSE);
  print_button(dialog, "  OK  ", y, x, TRUE);

  wrefresh(dialog);

  while (key != ESC) {
    key = wgetch(dialog);
    /* Check if key pressed matches first character of any item tag in menu */

    if (key == KEY_UP || key == KEY_DOWN || key == '-' || key == '+') {
     if (key == KEY_UP || key == '-') {
        if (!choice) {
          if (scroll) {
#ifdef BROKEN_WSCRL
    /* wscrl() in ncurses 1.8.1 seems to be broken, causing a segmentation
       violation when scrolling windows of height = 4, so scrolling is not
       used for now */
            scroll--;
            getyx(dialog, cur_y, cur_x);    /* Save cursor position */
            /* Reprint menu to scroll down */
            for (i = 0; i < max_choice; i++)
              print_item(menu, items[(scroll+i)], i, i == choice);

#else

            /* Scroll menu down */
            getyx(dialog, cur_y, cur_x);    /* Save cursor position */
            if (menu_height > 1) {
              /* De-highlight current first item before scrolling down */
              print_item(menu, items[scroll], 0, FALSE);
              scrollok(menu, TRUE);
              wscrl(menu, -1);
              scrollok(menu, FALSE);
            }
            scroll--;
            print_item(menu, items[scroll], 0, TRUE);
#endif
            wnoutrefresh(menu);
	    print_arrows(dialog, scroll, menu_height, item_no, box_x, box_y, item_x, cur_x, cur_y);
  	    print_position(dialog, box_x+menu_width, box_y+menu_height, scroll+choice, item_no);	    
  	    wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */        
            wrefresh(dialog);
          }
          continue;    /* wait for another key press */
        }
        else
          i = choice - 1;
      }
      else if (key == KEY_DOWN || key == '+') {
        if (choice == max_choice - 1) {
          if (scroll+choice < item_no-1) {
#ifdef BROKEN_WSCRL
    /* wscrl() in ncurses 1.8.1 seems to be broken, causing a segmentation
       violation when scrolling windows of height = 4, so scrolling is not
       used for now */
            scroll++;
            getyx(dialog, cur_y, cur_x);    /* Save cursor position */
            /* Reprint menu to scroll up */
            for (i = 0; i < max_choice; i++)
              print_item(menu, items[(scroll+i)], i, i == choice);

#else

            /* Scroll menu up */
            getyx(dialog, cur_y, cur_x);    /* Save cursor position */
            if (menu_height > 1) {
              /* De-highlight current last item before scrolling up */
              print_item(menu, items[(scroll+max_choice-1)], max_choice-1, FALSE);
              scrollok(menu, TRUE);
              scroll(menu);
              scrollok(menu, FALSE);
            }
            scroll++;
              print_item(menu, items[(scroll+max_choice-1)], max_choice-1, TRUE);
#endif
            wnoutrefresh(menu);
	    print_arrows(dialog, scroll, menu_height, item_no, box_x, box_y, item_x, cur_x, cur_y);
  	    print_position(dialog, box_x+menu_width, box_y+menu_height, scroll+choice, item_no);	    
  	    wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */        
            wrefresh(dialog);
          }
          continue;    /* wait for another key press */
        }
        else
          i = choice + 1;
      }

      if (i != choice) {
        /* De-highlight current item */
        getyx(dialog, cur_y, cur_x);    /* Save cursor position */
        print_item(menu, items[(scroll+choice)], choice, FALSE);

        /* Highlight new item */
        choice = i;
        print_item(menu, items[(scroll+choice)], choice, TRUE);
        wnoutrefresh(menu);
        print_position(dialog, box_x+menu_width, box_y+menu_height, scroll+choice, item_no);
        wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */        
        wrefresh(dialog);
      }
      continue;    /* wait for another key press */
    }

    /* save info about menu item position */
    if (ch)
	*ch = choice;
    if (sc)
	*sc = scroll;

    switch (key) {
    case KEY_PPAGE:
    case 'B' :
    case 'b' :
	if (scroll > menu_height) {	/* can we go up? */
	    scroll -= (menu_height);
	} else {
	    scroll = 0;
	}
	redraw_menu = TRUE;
	break;
    case KEY_NPAGE:
    case 'F' :
    case 'f' :
	if (scroll + menu_height >= item_no-1 - menu_height) { /* can we go down a full page? */
	    scroll = item_no - menu_height;
	    if (scroll < 0) scroll = 0;
	} else {
	    scroll += menu_height;
	}
	redraw_menu = TRUE;
	break;
    case KEY_HOME:
    case 'g' :
	scroll = 0;
	choice = 0;
	redraw_menu = TRUE;
	break;
    case KEY_END:
    case 'G' :
	scroll = item_no - menu_height;
	if (scroll < 0) scroll = 0;
	choice = max_choice - 1;
	redraw_menu = TRUE;
	break;
    case 'O':
    case 'o':
        delwin(dialog);
	*result = scroll+choice;
        return 0;
    case 'C':
    case 'c':
        delwin(dialog);
        return 1;
    case KEY_BTAB:
    case TAB:
    case KEY_LEFT:
    case KEY_RIGHT:
        if (!button) {
          button = 1;    /* Indicates "Cancel" button is selected */
          print_button(dialog, "  OK  ", y, x, FALSE);
          print_button(dialog, "Cancel", y, x+14, TRUE);
        }
        else {
          button = 0;    /* Indicates "OK" button is selected */
          print_button(dialog, "Cancel", y, x+14, FALSE);
          print_button(dialog, "  OK  ", y, x, TRUE);
        }
        wrefresh(dialog);
        break;
    case ' ':
    case '\r':
    case '\n':
        delwin(dialog);
        if (!button)
	  *result = scroll+choice;
        return button;
    case ESC:
        break;
    case KEY_F(1):
    case '?':
	display_helpfile();
	break;
    }
    if (redraw_menu) {
	for (i = 0; i < max_choice; i++) {
	    print_item(menu, items[(scroll+i)],
		       i, i == choice);
	}
	wnoutrefresh(menu);
        getyx(dialog, cur_y, cur_x);    /* Save cursor position */	
	print_arrows(dialog, scroll, menu_height, item_no, box_x, box_y, item_x, cur_x, cur_y);
  	print_position(dialog, box_x+menu_width, box_y+menu_height, scroll+choice, item_no);	
  	wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */        
	wrefresh(dialog);
	redraw_menu = FALSE;
    }
  }

  delwin(dialog);
  return -1;    /* ESC pressed */
}
/* End of dialog_treemenu() */


/*
 * Print menu item
 */
static void print_item(WINDOW *win, struct leaf item, int choice, int selected)
{
  int i, j = menu_width - 2;
  char *branches = item.branches;

  /* Clear 'residue' of last item */
  wattrset(win, menubox_attr);
  wmove(win, choice, 0);
  for (i = 0; i < menu_width; i++)
    waddch(win, ' ');
  wmove(win, choice, item_x);

  while(*branches && j)
  {
  	switch (*branches++) {
  	case ' ' : waddch(win, ' ');
  		break;
  	case '|' : waddch(win, ACS_VLINE);
  	}
  	
  	j--;
  	i = 3;
  	while(i-- && j)
  	{
	  	waddch(win, ' ');
	  	j--;
	}
  }	
  
  if (j)
  {
	  switch (item.slip) {
	  case '+' : waddch(win, ACS_LTEE);
  		break;
	  case '`' : waddch(win, ACS_LLCORNER);
	  }
	  j--;
  }

  i = 3;
  while(i-- && j)
  {
  	waddch(win, ACS_HLINE);
  	j--;
  }
  
  wattrset(win, selected ? item_selected_attr : item_attr);
  if (j)
	  waddnstr(win, item.name, j);
}
/* End of print_item() */

/*
 * Print current position
 */
static void print_position(WINDOW *win, int x, int y, 
					int cur_pos, int size)
{
  int percent;

  wattrset(win, position_indicator_attr);
  percent = cur_pos == size - 1 ? 100 : (cur_pos * 100)/(size - 1);
  wmove(win, y + 1, x - 6);
  wprintw(win, "(%3d%%)", percent);
}
/* End of print_position() */

/*
 * Display a tree menu from file
 *
 * filename	- file with like find(1) output
 * FS		- fields separator
 * title	- title of dialog box
 * prompt	- prompt text into dialog box
 * height	- height of dialog box
 * width	- width of dialog box
 * menu_height	- height of menu box
 * result	- pointer to char array
 *
 * return values:
 * -1		- ESC pressed
 * 0		- Ok, result set (must be freed later)
 * 1		- Cancel
 */

int dialog_ftree(unsigned char *filename, unsigned char FS,
		unsigned char *title, unsigned char *prompt, 
			int height, int width, int menu_height, 
					unsigned char **result)
{
	int retcode, choice, size;
	struct leaf *items;
	unsigned char **names;
	
	if (mk_ftree(filename, &names, &size, FS, &items))
	{
		perror("dialog_ftree");
		end_dialog();
		exit(-1);
	}
	
	if (!size)
	{
		fprintf(stderr, "\ndialog_ftree: file %s is empty\n", filename);
		end_dialog();
		exit(-1);
	}
	
	retcode = dialog_treemenu(title, prompt, height, width, menu_height,
					size, items, &choice, NULL, NULL);
					
	free_leafs(items, size);
	
	if (!retcode)
		*result = free_names(names, size, choice);
	else
		(void)free_names(names, size, -1);	
					
	return retcode;
}
/* End of dialog_ftree() */

/*
 * Display a tree menu from array
 *
 * names	- array with like find(1) output
 * size		- size of array
 * FS		- fields separator
 * title	- title of dialog box
 * prompt	- prompt text into dialog box
 * height	- height of dialog box
 * width	- width of dialog box
 * menu_height	- height of menu box
 * result	- pointer to char array
 *
 * return values:
 * -1		- ESC pressed
 * 0		- Ok, result set
 * 1		- Cancel
 */

int dialog_tree(unsigned char **names, int size, unsigned char FS,
		unsigned char *title, unsigned char *prompt, 
			int height, int width, int menu_height, 
					unsigned char **result)
{
	int retcode, choice;
	struct leaf *items;
	struct saved_tree *st;
	static struct queue *q_saved_tree = NULL;
	
	if (!size)
	{
		fprintf(stderr, "\ndialog_tree: source array is empty\n");
		end_dialog();
		exit(-1);
	}	
	
	if (mk_tree(names, size, FS, &items))
	{
		perror("dialog_tree");
		end_dialog();
		exit(-1);
	}

/* is tree saved ? */
	if (!(st = search_saved_tree(q_saved_tree, names, 
					size, FS,
					height, width, menu_height))) {
		if (!q_saved_tree) {
			if (!(q_saved_tree = 
				calloc(sizeof (struct queue), 1))) {
				perror("dialog_tree");
				end_dialog();
				exit(-1);
			}
		}

		if (!(st = calloc(sizeof (struct saved_tree), 1))) {
			perror("dialog_tree");
			end_dialog();
			exit(-1);
		}
		
		st->names = names;
		st->size = size;
		st->FS = FS;
		st->height = height;
		st->width = width;
		st->menu_height = menu_height;
		
		if (!p2_queue(q_saved_tree, st)) {
			perror("dialog_tree");
			end_dialog();
			exit(-1);
		}
	}
	
	retcode = dialog_treemenu(title, prompt, height, width, menu_height,
					size, items, &choice, 
					&(st->ch), &(st->sc));
		
	free_leafs(items, size);
				
	if (!retcode)
		*result = names[choice];
		
	return retcode;
}
/* End of dialog_tree() */

/* utils for ftree */

/* init struct queue by zeros */
static void
init_queue(struct queue *queue)
{
	bzero((void *)queue, sizeof(struct queue));
}

/* add pointer to queue */
/* return - pointer or NULL if error */
static void	*
p2_queue(struct queue *queue, void *pointer)
{
	if (!queue)
		return NULL;

	if (!queue->first)
	{
		if (!(queue->first = queue->last = 
			calloc(1, sizeof(struct m_queue))))
				return NULL;

	}
	else 
	{
	if (!(queue->last->next = 
			calloc(1, sizeof(struct m_queue))))
				return NULL;	
	
	queue->last = queue->last->next;
	}
		
	queue->size++;
	return queue->last->pointer = pointer;		
}

/* get first from queue */
/* return - pointer or NULL if queue is empty */
static void	*
first_queue(struct queue *queue)
{
	void *retval;
	struct m_queue *new_first;
	
	if (!queue ||
		!queue->first ||
			!queue->size)
				return NULL;
	
	retval = queue->first->pointer;
	new_first = queue->first->next;
	free(queue->first);
	queue->first = new_first;
	queue->size--;
	
	return retval;
		
}

/* make zero terminated array from queue */
/* return - pointer to array or NULL if error */
static void	**
q2arr(struct queue *queue, int depth)
{
	void **mono, **end;

	if (!queue ||
		!queue->first ||
			!queue->size)
			return NULL;

	/* memory allocation for array */
	if (!(mono = end = malloc(depth * sizeof(void *) + 1)))
		return NULL;
	
	while(depth--)
	{
		if (!(*end++ = first_queue(queue)))
			break;
	}
	
	*end = NULL;
	
	return mono;
	
}

/*
 * smart_tree (for like find(1) with -d flag output compliance)
 *
 * return values:
 * NULL - malloc error
 * not NULL - ok
 *
 */
static
unsigned char *
smart_tree(struct queue *queue,
		unsigned char FS, 
		unsigned char *current, 
		unsigned char *prev) {
	unsigned char *pcurrent = current, *pprev = prev, *toqueue;
	register char break_flag = 0;
	
	while(*pcurrent && *pprev) {
		if (*pcurrent == *pprev) {
			pcurrent++;
			pprev++;
		}
		else {
			break_flag = 1;
			break;
		}
	}

	if (!*pprev || break_flag) {
		if (*pcurrent == FS) {
			pcurrent++;
			
			if ((!*prev) && (*pcurrent)) {
				unsigned char tchar = *pcurrent;
			
				*pcurrent = '\0';
				if (!(toqueue = strdup(current))) {
					*pcurrent = tchar;
					return NULL;
				}
				if (!p2_queue(queue, toqueue)) {
					*pcurrent = tchar;
					return NULL;
				}
				*pcurrent = tchar;			
			}
		}

		while(*pcurrent) {
			if (*pcurrent == FS) {
				*pcurrent = '\0';
				if (!(toqueue = strdup(current))) {
					*pcurrent = FS;
					return NULL;
				}
				if (!p2_queue(queue, toqueue)) {
					*pcurrent = FS;
					return NULL;
				}
				*pcurrent = FS;
			}
			pcurrent++;
		}
		if (!p2_queue(queue, current))
			return NULL;	
	} 
	return current;
}

/* end of utils for ftree */

/* utils for make tree */

/* if error - return -1 */
static
int
mk_slip(struct leaf array[], int arr_size, int number, int shift)
{
	int t_number;
	int t_shift;
	
	if (number > arr_size - 1)
		return number - 1;
	
	t_shift = shift;
	
	if (!(array[number].branches = calloc(1, t_shift + 1)))
			return -1;
			
	(void)memset(array[number].branches, ' ', t_shift);
	
	t_number = number;
	
	while (array[number].shift < array[t_number + 1].shift)
	{
		t_number = mk_slip(array, arr_size, t_number + 1, t_shift + 1);
		if (t_number < 0) 
				return -1;
		if (t_number == arr_size - 1) 
				break;
	}
	
	if (array[number].shift == array[t_number + 1].shift)
		array[number].slip = '+';
	
	if ((array[number].shift > array[t_number + 1].shift) || 
				t_number == arr_size - 1)
		array[number].slip = '`';
	
	return t_number;

} /* mk_slip() */

/* make tree from file
 *
 * filename	- name of file with like find(1) output
 * p_names	- pointer to array of strings
 * p_size	- pointer to size of array
 * FS		- fields separator
 * p_array	- pointer to array of leafs
 *
 * return values:
 * 0		- ok and names by p_names, size by p_size, array by p_array set
 * -1		- memory allocation error (errno set)
 */  

static
int
mk_ftree(char *filename, 
	unsigned char ***p_names, int *p_size, unsigned char FS, 
		struct leaf **p_array)
{
	int NR;	/* number of input records */	
	struct queue queue;
	unsigned char *string, *sstring = "";
	unsigned char **names;
	
	FILE *input_file;
	
	if (!(input_file = fopen(filename, "r")))
				return -1;

	init_queue(&queue);	
	
	if (!(string = malloc(BUFSIZ)))
			return -1;
	
	/* read input file into queue */
	while(fgets(string, BUFSIZ, input_file))
	{
		if (strchr(string, '\n'))
			*strchr(string, '\n') = '\0';
	
		if (!(string = realloc(string, strlen(string) + 1)))
				return -1;
				
		if (!smart_tree(&queue, FS, string, sstring))
				return -1;
		sstring = string;
				
		if (!(string = malloc(BUFSIZ)))
				return -1;
	} /* read input file into queue */	
	
	if (fclose(input_file) == EOF)
			return -1;
	
	if (!(NR = queue.size))
	{
		*p_size = 0;
		return 0;
	}	

	/* make array from queue */
	if (!(names = (unsigned char **)q2arr(&queue, NR)))
			return -1;
			
	*p_names = names;
	*p_size = NR;
	
	/* make tree from array */
	return mk_tree(names, NR, FS, p_array);
	
} /* mk_ftree */

/* make tree from array
 *
 * names	- array of strings
 * size		- size of array
 * FS		- fields separator
 * p_array	- pointer to array of leafs
 *
 * return values:
 * 0		- ok and array by p_array set
 * -1		- memory allocation error (errno set)
 */  
 
static
int
mk_tree(unsigned char **names, int size, unsigned char FS, 
		struct leaf **p_array)
{
	int i;
	struct leaf *array;

	/* make array of leafs */
	if (!(array = calloc(size, sizeof(struct leaf))))
			return -1;
	
	/* init leafs */
	for (i = 0; i < size; i++)
	{
		unsigned char *in_string, *name;
		int shift = 0;
	
		in_string = name = names[i];
		while(*in_string)
		{
			if (*in_string == FS) {
				if (!i && !*(in_string + 1))
					name = in_string;
				else
				{
					shift++;
					name = in_string + 1;
				}
			}
			in_string++;
		}
		array[i].name = name;
		array[i].shift = shift;
		array[i].slip = '\0';
		array[i].branches = NULL;
	} /* init leafs */
	
	/* make slips */
	for (i = 0;i < size; i++)
	{
		i = mk_slip(array, size, i, 0);
		if (i < 0) 
			return -1;
	} /* make slips */

	/* make branches */
	for (i = 1;i < size; i++)
	{
		unsigned char *src = array[i - 1].branches;
		unsigned char *dst = array[i].branches;
	
		while(*src && *dst)
			*dst++ = *src++;
		
		if (*dst)
			switch (array[i - 1].slip) {
			case '+' : *dst = '|'; 
				break;
			case '`' : *dst = ' ';
			}
	} /* make branches */
	
	*p_array = array;
	return 0;

} /* mk_tree() */

/* free memory from tree (leafs)
 *
 * return values:
 * nothing
 */

static
void
free_leafs(struct leaf *array, int size)
{
	struct leaf *p_array = array;
	
	while (size--)
		free(array++->branches);

	free(p_array);
} /* free_leafs() */

/* free memory from source data for tree (names)
 *
 * return values:
 * if 0 <= choice <= size - pointer to name from names, 
 *	and memory for name not released (must be freed later)
 * else - NULL (recomended choice -1 for it)
 */

static
unsigned char *
free_names(unsigned char **names, int size, int choice)
{
	unsigned char *retval = NULL;
	unsigned char **p_names = names;
	
	while (size--)
	{
		if (!choice--)
			retval = *names++;
		else	
			free(*names++);
	}
	free(p_names);
	return retval;
} /* free_names() */

/* end of utils for make tree */

/* static utils for saved_tree */

/* search saved tree within queue */
/* return - struct *saved_tree or NULL if not found */
static 
struct saved_tree *
search_saved_tree(struct queue *queue, unsigned char **names, int size,
					unsigned char FS,
					int height, int width, 
					int menu_height) 
{
	struct m_queue *member;
	struct saved_tree *retval;
	
	if (!queue || !names || !FS || 
		!height || !width || !menu_height)
		return NULL;
	
	if (!(member = queue->first))
		return NULL;

	while (member->next) {
		retval = member->pointer;
		if ((names == retval->names) &&
			(size == retval->size) &&
			(FS == retval->FS) &&
			(height == retval->height) &&
			(width == retval->width) &&
			(menu_height == retval->menu_height))
			return retval;
		member = member->next;
	}
	retval = member->pointer;
	if ((names == retval->names) &&
		(size == retval->size) &&
		(FS == retval->FS) &&
		(height == retval->height) &&
		(width == retval->width) &&
		(menu_height == retval->menu_height))
		return retval;
	return NULL;	
}

/* end of static utils for saved_tree */
