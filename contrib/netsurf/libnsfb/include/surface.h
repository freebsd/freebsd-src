/* libnsfb framebuffer surface support */

#include "libnsfb.h" 
#include "libnsfb_plot.h"
#include "nsfb.h"

/* surface default options */
typedef int (nsfb_surfacefn_defaults_t)(nsfb_t *nsfb);

/* surface init */
typedef int (nsfb_surfacefn_init_t)(nsfb_t *nsfb);

/* surface finalise */
typedef int (nsfb_surfacefn_fini_t)(nsfb_t *nsfb);

/* surface set geometry */
typedef int (nsfb_surfacefn_geometry_t)(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format);

/* surface set parameters */
typedef int (nsfb_surfacefn_parameters_t)(nsfb_t *nsfb, const char *parameters);

/* surface input */
typedef bool (nsfb_surfacefn_input_t)(nsfb_t *nsfb, nsfb_event_t *event, int timeout);

/* surface area claim */
typedef int (nsfb_surfacefn_claim_t)(nsfb_t *nsfb, nsfb_bbox_t *box);

/* surface area update */
typedef int (nsfb_surfacefn_update_t)(nsfb_t *nsfb, nsfb_bbox_t *box);

/* surface cursor display */
typedef int (nsfb_surfacefn_cursor_t)(nsfb_t *nsfb, struct nsfb_cursor_s *cursor);

typedef struct nsfb_surface_rtns_s {
    nsfb_surfacefn_defaults_t *defaults;
    nsfb_surfacefn_init_t *initialise;
    nsfb_surfacefn_fini_t *finalise;
    nsfb_surfacefn_geometry_t *geometry;
    nsfb_surfacefn_parameters_t *parameters;
    nsfb_surfacefn_input_t *input;
    nsfb_surfacefn_claim_t *claim;
    nsfb_surfacefn_update_t *update;
    nsfb_surfacefn_cursor_t *cursor;
} nsfb_surface_rtns_t;

void _nsfb_register_surface(const enum nsfb_type_e type, const nsfb_surface_rtns_t *rtns, const char *name);


/* macro which adds a builtin command with no argument limits */
#define NSFB_SURFACE_DEF(__name, __type, __rtns)                       \
    static void __name##_register_surface(void) __attribute__((constructor)); \
    void __name##_register_surface(void) {                              \
        _nsfb_register_surface(__type, __rtns, #__name);               \
    }                                                                   

/** Obtain routines for a surface 
 *
 * Obtain a vlist of methods for a surface type.
 *
 * @param type The surface type.
 * @return A vtable of routines which teh caller must deallocate or
 *         NULL on error
 */
nsfb_surface_rtns_t *nsfb_surface_get_rtns(enum nsfb_type_e type);

