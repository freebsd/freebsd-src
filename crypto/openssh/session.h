#ifndef SESSION_H
#define SESSION_H

/* SSH1 */
void	do_authenticated(struct passwd * pw);

/* SSH2 */
void	do_authenticated2(void);
int	session_open(int id);
void	session_input_channel_req(int id, void *arg);
void	session_close_by_pid(pid_t pid, int status);
void	session_close_by_channel(int id, void *arg);

#endif
