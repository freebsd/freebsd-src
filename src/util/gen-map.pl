#!perl -w
use ktemplate;
# List of parameters accepted for substitution.
@parms = qw(NAME KEY VALUE COMPARE COPYKEY FREEKEY FREEVALUE);
# Defaults, if any.
$parm{"COPYKEY"} = "0";
$parm{"FREEKEY"} = "0";
$parm{"FREEVALUE"} = "0";
#
&run;
#
__DATA__
/*
 * map, generated from template
 * map name: <NAME>
 * key: <KEY>
 * value: <VALUE>
 * compare: <COMPARE>
 * copy_key: <COPYKEY>
 * free_key: <FREEKEY>
 * free_value: <FREEVALUE>
 */
struct <NAME>__element {
    <KEY> key;
    <VALUE> value;
    struct <NAME>__element *next;
};
struct <NAME>__head {
    struct <NAME>__element *first;
};
typedef struct <NAME>__head <NAME>;
static inline int <NAME>_init (struct <NAME>__head *head)
{
    head->first = NULL;
    return 0;
}
static inline void <NAME>_destroy (struct <NAME>__head *head)
{
    struct <NAME>__element *e, *e_next;
    void (*free_key)(<KEY>) = <FREEKEY>;
    void (*free_value)(<VALUE>) = <FREEVALUE>;
    for (e = head->first; e; e = e_next) {
	e_next = e->next;
	if (free_key)
	    (*free_key)(e->key);
	if (free_value)
	    (*free_value)(e->value);
	free(e);
    }
    head->first = NULL;
}
/* Returns pointer to linked-list entry, or null if key not found.  */
static inline struct <NAME>__element *
<NAME>__find_node (struct <NAME>__head *head, <KEY> key)
{
    struct <NAME>__element *e;
    for (e = head->first; e; e = e->next)
	if (<COMPARE> (key, e->key) == 0)
	    return e;
    return 0;
}
/* Returns pointer to value, or null if key not found.  */
static inline <VALUE> *
<NAME>_find (struct <NAME>__head *head, <KEY> key)
{
    struct <NAME>__element *e = <NAME>__find_node(head, key);
    if (e)
	return &e->value;
    return 0;
}
/* Returns 0 or error code.  */
static inline int
<NAME>__copy_key (<KEY> *out, <KEY> in)
{
    int (*copykey)(<KEY> *, <KEY>) = <COPYKEY>;
    if (copykey == 0) {
	*out = in;
	return 0;
    } else
	return (*copykey)(out, in);
}
/* Returns 0 or error code.  */
static inline int
<NAME>_replace_or_insert (struct <NAME>__head *head,
			  <KEY> key, <VALUE> new_value)
{
    struct <NAME>__element *e = <NAME>__find_node(head, key);
    int ret;

    if (e) {
	/* replace */
	void (*free_value)(<VALUE>) = <FREEVALUE>;
	if (free_value)
	    (*free_value)(e->value);
	e->value = new_value;
    } else {
	/* insert */
	e = malloc(sizeof(*e));
	if (e == NULL)
	    return ENOMEM;
	ret = <NAME>__copy_key (&e->key, key);
	if (ret) {
	    free(e);
	    return ret;
	}
	e->value = new_value;
	e->next = head->first;
	head->first = e;
    }
    return 0;
}
