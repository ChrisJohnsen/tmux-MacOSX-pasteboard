#include <string.h>    /* strlen, strcpy, strerror, strcmp, strrchr */
#include <stdarg.h>    /* va_...   */
#include <stdio.h>     /* fprintf, vfprintf  */
#include <stdlib.h>    /* malloc, exit, free, atoi */
#include <dlfcn.h>     /* dlsym    */
#include <stdint.h>    /* uint64_t */
#include <unistd.h>    /* execvp   */
#include <sys/errno.h> /* errno    */
#include <sys/utsname.h> /* uname  */

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

    free(new_fmt);

    exit(ev);
}

/*
 * ##__VA_ARGS_ is a GNU CPP extension to invoke with zero extra args.
 * Expects f to be a literal string (for compile-time concatenation).
 */
#define die_errno(x,f,...) die((x),(f ": %s"),##__VA_ARGS__,strerror(errno))

int main(int argc, char *argv[]) {
    if (argc < 2)
        die(1, "usage: %s <program> [args...]", argv[0]);

    unsigned int os = 0;

    struct utsname u;
    if (uname(&u))
        die_errno(2, "uname failed");
    if (strcmp(u.sysname, "Darwin"))
        fprintf(stderr, "warning: unsupported OS sysname: %s\n", u.sysname);

    char *rest, *whole = strdup(u.release);
    if (!whole)
        die_errno(2, "strdup failed");
    rest = whole;
    strsep(&rest, ".");
    if (whole && *whole && whole != rest) {
        int major = atoi(whole);
        if (major >= 6) /* 10.2 -- 10.7 */
            os = 1000 + (major-4) * 10;
        else /* 10.1, 10.0 and prior betas/previews */
            os = 1000;
    }
    else
        fprintf(stderr, "warning: unparsable major release number: '%s'\n", u.release);

    free(whole);

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
                if (!(f = dlsym(RTLD_NEXT, fn)))
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

    int arg = 1;
    char **newargs = argv+arg;
    const char *file = argv[arg++];
    if (!strcmp(file, "-l")) {
        file = argv[arg];
        /*
         * For their argv[0], take the bit of file after the
         * last slash (the whole thing if there is no slash
         * or if that bit would be zero length) and prefix
         * it with '-'.
         */
        char *arg0 = malloc(strlen(file) + 2);
        *arg0 = '-';
        char *slash = strrchr(file, '/');
        if (slash && slash[1])
            strcpy(arg0+1, slash+1);
        else
            strcpy(arg0+1, file);

        /* use the rest of the args as they are */
        int ofs = arg;
        newargs = malloc(sizeof(*newargs) * (argc-ofs+1));
        newargs[arg-ofs] = arg0;
        arg++;
        for(; arg < argc; arg++)
            newargs[arg-ofs] = argv[arg];
        newargs[arg-ofs] = NULL;
    }

    if (execvp(file, newargs) < 0)
        die(3, "execv failed: %s", strerror(errno));

    if (newargs) {
        free(newargs[0]);
        free(newargs);
    }

    return 0;
}
