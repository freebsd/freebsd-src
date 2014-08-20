#ifndef AESTABS_H_INCLUDED
#define AESTABS_H_INCLUDED

struct aes_tab_s;
struct aes_tablist_s;
typedef struct aes_tab_s AES_TAB;
typedef struct aes_tablist_s AES_TABLIST;

#define AES_TABLIST_TAB_ACTIVATED 	0x01
#define AES_TABLIST_TAB_DEACTIVATED	0x02

#define AES_TABLIST_OPTION_FORCE_EVENTS 0x01	// do not eat events which do
												// not changed the internal state
												// this is required for tabs which
												// require "activate" events
												// for tabs which are already
												// selected.


struct aes_tablist_user_args_s
{
	short event;
	AES_TAB *tab;
};

typedef struct aes_tablist_user_args_s AES_TABLIST_FUNC_ARGS;

typedef void (*aes_tablist_user_func)(AES_TABLIST * list,
									AES_TABLIST_FUNC_ARGS * args);

struct aes_tab_s {
	short obj_tab;
	short obj_page;
	OBJECT * page_tree;
	AES_TAB * next, *prev;
};

struct aes_tablist_s {
	OBJECT *tree;
	AES_TAB * first;
	aes_tablist_user_func user_func;
};



AES_TABLIST * tablist_declare(OBJECT *tree, aes_tablist_user_func user_func);
void tablist_delete(AES_TABLIST * tablist);
AES_TAB * tablist_add(AES_TABLIST * tablist, short tab, OBJECT *page_tree,
					 short page);
short tablist_activate(AES_TABLIST * tablist, short tab, short option);
struct aes_tab_s *tablist_get_active(AES_TABLIST * tablist);
AES_TAB * tablist_find(AES_TABLIST * tablist, OBJECT *page, short tab);

#define AES_TAB_IS_ACTIVE(l, x) (tablist_get_active(l) == x)

#endif // AESTABS_H_INCLUDED
