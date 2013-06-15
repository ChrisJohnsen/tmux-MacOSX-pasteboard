/*
 * Copyright (c) 2011-2013, Chris Johnsen <chris_johnsen@pobox.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 * 
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>    /* strlen, strcpy, strcmp, strrchr */
#include <stdarg.h>    /* va_...   */
#include <stdio.h>     /* fprintf, vfprintf  */
#include <stdlib.h>    /* malloc, exit, free, atoi */
#include <dlfcn.h>     /* dlsym    */
#include <stdint.h>    /* uint64_t */
#include <unistd.h>    /* execvp   */
#include <sys/utsname.h> /* uname  */

#include "msg.h"

static const char version[] = "2.2";
static const char supported_oses[] = "OS X 10.5-10.9";

#if 0
void * _vprocmgr_move_subset_to_user(uid_t target_user, const char *session_type, uint64_t flags); /* 10.6 */
void * _vprocmgr_move_subset_to_user(uid_t target_user, const char *session_type); /* 10.5 */
#endif

static const char usage_msg[] = "\n"
    "    Reattach to the per-user bootstrap namespace in its \"Background\"\n"
    "    session then exec the program with args. If \"-l\" is given,\n"
    "    rewrite the program's argv[0] so that it starts with a '-'.\n";

int main(int argc, char *argv[]) {
    unsigned int login = 0, usage = 0;

    if (argc > 1) {
        if (!strcmp(argv[1], "-l")) {
            login = 1;
            argv[1] = argv[0];
            argv++;
            argc--;
        } else if (!strcmp(argv[1], "-v") ||
                !strcmp(argv[1], "--version")) {
            printf("%s version %s\n    Supported OSes: %s\n",
                    argv[0], version, supported_oses);
            exit(0);
        } else if (*argv[1] == '-') {
            warn("unkown option: %s", argv[1]);
            usage = 2;
        }
    }
    if (argc < 2)
        usage = 1;
    if (usage)
        die(usage, "usage: %s [-l] <program> [args...]\n%s", argv[0], usage_msg);

    unsigned int os = 0;

    struct utsname u;
    if (uname(&u)) {
        warn_errno("uname failed");
        goto reattach_failed;
    }
    if (strcmp(u.sysname, "Darwin")) {
        warn("unsupported OS sysname: %s", u.sysname);
        goto reattach_failed;
    }

    char *rest, *whole = strdup(u.release);
    if (!whole) {
        warn_errno("strdup failed");
        goto reattach_failed;
    }
    rest = whole;
    strsep(&rest, ".");
    if (whole && *whole && whole != rest) {
        int major = atoi(whole);
        if (major >= 6) /* 10.2 and newer */
            os = 1000 + (major-4) * 10;
        else /* 10.1, 10.0 and prior betas/previews */
            os = 1000;
    }
    else
        warn("unparsable major release number: '%s'", u.release);

    free(whole);

    /*
     * change the 'os' variable to represent the "reattach variation"
     * instead of the major OS release
     *
     *  older => 1050 with warning
     *   10.5 => 1050
     *   10.6 => 1060
     *   10.7 => 1060
     *   10.8 => 1060
     *   10.9 => 1060
     *  newer => 1060 with warning
     */
    if (1060 <= os && os <= 1090)
        os = 1060;
    else if (os < 1050) {
        warn("%s: unsupported old OS, trying as if it were 10.5", argv[0]);
        os = 1050;
    } else if (os > 1060) {
        warn("%s: unsupported new OS, trying as if it were 10.6-10.9", argv[0]);
        os = 1060;
    }

    switch(os) {
        case 1050:
        case 1060:
            {
                static const char fn[] = "_vprocmgr_move_subset_to_user";
                void *(*f)();
                if (!(f = (void *(*)()) dlsym(RTLD_NEXT, fn))) {
                    warn("unable to find %s: %s", fn, dlerror());
                    goto reattach_failed;
                }

                void *r;
                static const char bg[] = "Background";
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
                } else {
                    warn("BUG: unhandled reattach variation: %u", os);
                    goto reattach_failed;
                }

                if (r) {
                    warn("%s failed", fn);
                    goto reattach_failed;
                }
            }
            break;
       default:
reattach_failed:
            warn("%s: unable to reattach", argv[0]);
            break;
    }

    char **newargs = NULL;
    const char *file = argv[1];
    if (login) {
        /*
         * For their argv[0], take the bit of file after the
         * last slash (the whole thing if there is no slash
         * or if that bit would be zero length) and prefix
         * it with '-'.
         */
        char *arg0 = malloc(strlen(file) + 2);
        if (!arg0)
            goto exec_it;
        *arg0 = '-';
        char *slash = strrchr(file, '/');
        if (slash && slash[1])
            strcpy(arg0+1, slash+1);
        else
            strcpy(arg0+1, file);

        /* use the rest of the args as they are */
        newargs = malloc(sizeof(*newargs) * (argc));
        if (!newargs) {
            free(arg0);
            goto exec_it;
        }
        newargs[0] = arg0;
        int arg = 2;
        for(; arg < argc; arg++)
            newargs[arg-1] = argv[arg];
        newargs[arg-1] = NULL;
    }

exec_it:
    if (execvp(file, newargs ? newargs : argv+1) < 0)
        die_errno(3, "%s: execv failed", argv[0]);

    if (newargs) {
        free(newargs[0]);
        free(newargs);
    }

    return 0;
}
