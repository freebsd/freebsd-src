#ifndef _WRAPPER_H_
#define _WRAPPER_H_

#define mem_map_reserve(p)	set_bit(PG_reserved, &((p)->flags))
#define mem_map_unreserve(p)	clear_bit(PG_reserved, &((p)->flags))

#endif /* _WRAPPER_H_ */
