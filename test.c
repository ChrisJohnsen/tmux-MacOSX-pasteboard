#include <sys/errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <unistd.h>

void *_vprocmgr_detach_from_console(unsigned int flags);

static FILE *myerr;

void die(const char *str) {
    fprintf(myerr?myerr:stderr, "fatal: %s\n", str);
    exit(1);
}
void die_errno(const char *str) {
    fprintf(myerr?myerr:stderr, "fatal: %s: %s\n", str, strerror(errno));
    exit(1);
}

int mydaemon(int nochdir, int noclose) {
    /*
     * Hmm, got pretty close.
     * http://www.opensource.apple.com/source/Libc/Libc-594.9.4/gen/daemon-fbsd.c
     * Theirs touches on signal handling, too. And, it does the
     * unwanted bit about moving to the root bootstrap namespace.
     */
    pid_t p;
    int fd, i;
    if ((p = fork()) < 0) return -1;
    if (p>0) exit(0);
    if ((p = setsid()) < 0) return -1;
    if (!nochdir) chdir("/");
    if (!noclose &&
        !((fd = open("/dev/null", O_RDWR)) < 0)) {
        for (i = 0; i < 3; i++) { dup2(fd,i); }
        if (fd > 2) close(fd);
    }
    return 0;
}

#ifdef MYDAEMON
#define thedaemon(a,b) mydaemon(a,b)
#define thedaemon_str "mydaemon"
#else
#define thedaemon(a,b) daemon(a,b)
#define thedaemon_str "daemon"
#endif

#include <stdint.h>
void *
_vprocmgr_move_subset_to_user(uid_t target_user, const char *session_type, uint64_t flags);

int main(int ac, char *av[]) {
    int r, o, e;

    if ((e = dup(2)) < 0)
        die_errno("dup myerr");
    if (!(myerr = fdopen(e, "w")))
        die_errno("fdopen for myerr");

    fprintf(myerr, "pid==%d\n", getpid());
    /*
    if (_vprocmgr_detach_from_console(0) != NULL)
        die("_vprocmgr_detach_from_console failed");
    fprintf(myerr, "pid==%d (after detach)\n", getpid());
    */
    if ((r = thedaemon(1,0))) {
        char buf[256];
        sprintf(buf, thedaemon_str "() failed = %d", r);
        die_errno(buf);
    }
#define SNOW_LEOPARD
#if defined(LEOPARD)
    if(_vprocmgr_move_subset_to_user(getuid(), "Background") != NULL)
#elif defined(SNOW_LEOPARD)
    if(_vprocmgr_move_subset_to_user(getuid(), "Background", 0) != NULL)
#endif
        die("move_subset_to_user failed");
    fprintf(myerr, "pid==%d (after " thedaemon_str ")\n", getpid());

    if (dup2(e,1) < 0)
        die_errno("dup2(e,1) failed");
    if (dup2(e,2) < 0)
        die_errno("dup2(e,2) failed");

    if ((r = system("pbpaste")) < 0)
        die("system() failed");

    if (WIFEXITED(r))
        fprintf(myerr, "system() exited %d\n", WEXITSTATUS(r));
    else
        fprintf(myerr, "system() == %d", r);

    return 0;
}
