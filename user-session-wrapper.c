#include <string.h>    /* strlen, strcpy, strerror */
#include <stdarg.h>    /* va_...   */
#include <stdio.h>     /* fprintf, vfprintf  */
#include <stdlib.h>    /* exit     */
#include <dlfcn.h>     /* dlsym    */
#include <stdint.h>    /* uint64_t */
#include <unistd.h>    /* execvp   */
#include <sys/errno.h> /* errno    */

#if 0
void * _vprocmgr_move_subset_to_user(uid_t target_user, const char *session_type, uint64_t flags); /* 10.6 */
void * _vprocmgr_move_subset_to_user(uid_t target_user, const char *session_type); /* 10.5 */
#endif

void die(int ev, const char *fmt, ...) {
    va_list ap;
    int len = strlen(fmt);
    char *new_fmt = malloc(len+2);

    strcpy(new_fmt,fmt);
    strcpy(new_fmt+len,"\n");

    va_start(ap,fmt);
    vfprintf(stderr, new_fmt, ap);
    va_end(ap);
    exit(ev);
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        die(1, "usage: %s <program> [args...]", argv[0]);

    /* TODO: detect OS version */
    unsigned int os = 1060;

    if (os < 1050) {
        fprintf(stderr, "warning: unsupported old OS, trying as if it were 10.5\n");
        os = 1050;
    } else if (os > 1060) {
        fprintf(stderr, "warning: unsupported new OS, trying as if it were 10.6\n");
        os = 1060;
    }

    switch(os) {
        case 1050:
        case 1060:
            {
                const char fn[] = "_vprocmgr_move_subset_to_user";
                void *f;
                if (!(f = dlsym(RTLD_SELF, fn)))
                    die(2, "unable to find %s: %s", fn, dlerror());

                void *r;
                const char bg[] = "Background";
                /*
                 * 10.5 has one fewer args.
                 * Since we are probably using a caller-cleans-up
                 * calling convention, we could probably always just
                 * call it with the extra arg, but we might as well
                 * do things properly.
                 */
                if (os == 1050) {
                    void *(*func)(uid_t, const char *) = f;
                    r = func(getuid(), bg);
                }
                else if (os == 1060) {
                    void *(*func)(uid_t, const char *, uint64_t) = f;
                    r = func(getuid(), bg, 0);
                } else
                    die(2, "unexpected OS, giving up");

                if (r)
                    die(2, "%s failed", fn);
            }
            break;
       default:
            die(2, "unsupported OS, giving up");
            break;
    }

    if (execvp(argv[1], argv+1) < 0)
        die(3, "execv failed: %s", strerror(errno));

    return 0;
}
