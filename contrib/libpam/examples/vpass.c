#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <security/pam_appl.h>

static int test_conv(int num_msg, const struct pam_message **msgm,
		     struct pam_response **response, void *appdata_ptr)
{
    return 0;
}

static struct pam_conv conv = {
    test_conv,
    NULL
};

int main(void)
{
    char *user;
    pam_handle_t *pamh;
    struct passwd *pw;
    uid_t uid;
    int res;

    uid = geteuid();
    pw = getpwuid(uid);
    if (pw) {
	user = pw->pw_name;
    } else {
	fprintf(stderr, "Invalid userid: %d\n", uid);
	exit(1);
    }

    pam_start("vpass", user, &conv, &pamh);
    pam_set_item(pamh, PAM_TTY, "/dev/tty");
    if ((res = pam_authenticate(pamh, 0)) != PAM_SUCCESS) {
	fprintf(stderr, "Oops: %s\n", pam_strerror(pamh, res));
	exit(1);
    }

    pam_end(pamh, res);
    exit(0);
}


