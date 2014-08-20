#ifndef DESKMENU_H_INCLUDED
#define DESKMENU_H_INCLUDED

void deskmenu_init(void);
void deskmenu_destroy(void);
int deskmenu_dispatch_item(short title, short item);
int deskmenu_dispatch_keypress(unsigned short kcode, unsigned short kstate,
							unsigned short nkc);
OBJECT * deskmenu_get_obj_tree(void);
void deskmenu_update( void );

#endif // DESKMENU_H_INCLUDED
