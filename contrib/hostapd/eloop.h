#ifndef ELOOP_H
#define ELOOP_H

/* Magic number for eloop_cancel_timeout() */
#define ELOOP_ALL_CTX (void *) -1

/* Initialize global event loop data - must be called before any other eloop_*
 * function. user_data is a pointer to global data structure and will be passed
 * as eloop_ctx to signal handlers. */
void eloop_init(void *user_data);

/* Register handler for read event */
int eloop_register_read_sock(int sock,
			     void (*handler)(int sock, void *eloop_ctx,
					     void *sock_ctx),
			     void *eloop_data, void *user_data);
void eloop_unregister_read_sock(int sock);

/* Register timeout */
int eloop_register_timeout(unsigned int secs, unsigned int usecs,
			   void (*handler)(void *eloop_ctx, void *timeout_ctx),
			   void *eloop_data, void *user_data);

/* Cancel timeouts matching <handler,eloop_data,user_data>.
 * ELOOP_ALL_CTX can be used as a wildcard for cancelling all timeouts
 * regardless of eloop_data/user_data. */
int eloop_cancel_timeout(void (*handler)(void *eloop_ctx, void *sock_ctx),
			 void *eloop_data, void *user_data);

/* Register handler for signal.
 * Note: signals are 'global' events and there is no local eloop_data pointer
 * like with other handlers. The (global) pointer given to eloop_init() will be
 * used as eloop_ctx for signal handlers. */
int eloop_register_signal(int sock,
			  void (*handler)(int sig, void *eloop_ctx,
					  void *signal_ctx),
			  void *user_data);

/* Start event loop and continue running as long as there are any registered
 * event handlers. */
void eloop_run(void);

/* Terminate event loop even if there are registered events. */
void eloop_terminate(void);

/* Free any reserved resources. After calling eloop_destoy(), other eloop_*
 * functions must not be called before re-running eloop_init(). */
void eloop_destroy(void);

/* Check whether event loop has been terminated. */
int eloop_terminated(void);

#endif /* ELOOP_H */
