/*-
 * $Id: $
 */

#ifndef _DEV_FB_SPLASHREG_H_
#define _DEV_FB_SPLASHREG_H_

#define SPLASH_IMAGE	"splash_image_data"

struct video_adapter;

typedef struct splash_decoder {
	char		*name;
	int		(*init)(struct video_adapter *adp, void *data,
				size_t size);
	int		(*term)(struct video_adapter *adp);
	int		(*splash)(struct video_adapter *adp, int on);
} splash_decoder_t;

#define SPLASH_DECODER(name, sw)				\
	static int name##_modevent(module_t mod, int type, void *data) \
	{							\
		switch ((modeventtype_t)type) {			\
		case MOD_LOAD:					\
			return splash_register(&sw);		\
		case MOD_UNLOAD:				\
			return splash_unregister(&sw);		\
		}						\
		return 0;					\
	}							\
	static moduledata_t name##_mod = {			\
		#name, 						\
		name##_modevent,				\
		NULL						\
	};							\
	DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_ANY)

/* entry point for the splash image decoder */
int	splash_register(splash_decoder_t *decoder);
int	splash_unregister(splash_decoder_t *decoder);

/* entry points for the console driver */
int	splash_init(video_adapter_t *adp, int (*callback)(int));
int	splash_term(video_adapter_t *adp);
int	splash(video_adapter_t *adp, int on);

/* event types for the callback function */
#define SPLASH_INIT	0
#define SPLASH_TERM	1

#endif /* _DEV_FB_SPLASHREG_H_ */
