/* KEYWORD.H: Definitions for markup declaration keyword processing.
*/
/* Default value types for attribute definition list declaration.
*/
#define DNULL    1            /* Default value: implied attribute. */
#define DREQ     2            /* Default value: required attribute. */
#define DCURR    3            /* Default value: current attribute. */
#define DCONR    4            /* Default value: content reference attribute. */
#define DFIXED   5            /* Default value: fixed attribute. */

/* External identifier types for entity and notation declarations.
*/
#define EDSYSTEM  1           /* SYSTEM (but not PUBLIC) identifier specified.*/
#define EDPUBLIC  2           /* PUBLIC (but not SYSTEM) identifier specified.*/
#define EDBOTH    3           /* PUBLIC and also SYSTEM identifiers specified.*/

/* Marked section keywords.
*/
#define MSTEMP   1
#define MSRCDATA 2
#define MSCDATA  3
#define MSIGNORE 4
