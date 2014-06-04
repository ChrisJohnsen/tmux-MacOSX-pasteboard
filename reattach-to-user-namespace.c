/*
 * Copyright (c) 2011-2014, Chris Johnsen <chris_johnsen@pobox.com>
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

#include <mach/mach.h>
#include <bootstrap.h>

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
        os = 100000;    /* 10.1, 10.0 and prior betas/previews */
        if (major >= 6) /* 10.2 and newer */
            os += (major-4) * 100;
    }
    else
        warn("unparsable major release number: '%s'", u.release);

    free(whole);

    /*
     * change the 'os' variable to represent the "reattach variation"
     * instead of the major OS release
     *
     *  older => 100500 with warning
     *   10.5 => 100500
     *   10.6 => 100600
     *   10.7 => 100600
     *   10.8 => 100600
     *   10.9 => 100600
     *   10.10=> 101000
     *  newer => 100600 with warning
     */
    if (100600 <= os && os <= 100900)
        os = 100600;
    else if (os == 101000) {
        // do nothing
    }
    else if (os < 100500) {
        warn("%s: unsupported old OS, trying as if it were 10.5", argv[0]);
        os = 100500;
    } else if (os > 100600) {
        warn("%s: unsupported new OS, trying as if it were 10.6-10.9", argv[0]);
        os = 100600;
    }

    switch(os) {
        case 100500:
        case 100600:
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
                 * 10.5 has one fewer arg.
                 * Since we are probably using a caller-cleans-up
                 * calling convention, we could probably always just
                 * call it with the extra arg, but we might as well
                 * do things properly.
                 */
                if (os == 100500) {
                    void *(*func)(uid_t, const char *) = f;
                    r = func(getuid(), bg);
                }
                else if (os == 100600) {
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
       case 101000:
            {
#define FIND_SYMBOL(NAME, RET, SIG) \
                static const char fn_ ## NAME [] = # NAME; \
                typedef RET (*ft_ ## NAME) SIG; \
                ft_ ## NAME f_ ## NAME; \
                if (!(f_ ## NAME = (ft_ ## NAME)dlsym(RTLD_NEXT, fn_ ## NAME))) { \
                    warn("unable to find %s: %s", fn_ ## NAME, dlerror()); \
                    goto reattach_failed; \
                }

                FIND_SYMBOL(bootstrap_get_root, kern_return_t, (mach_port_t, mach_port_t *))
                FIND_SYMBOL(bootstrap_look_up_per_user, kern_return_t, (mach_port_t, const char *, uid_t, mach_port_t *))

#undef FIND_SYMBOL

                mach_port_t puc = MACH_PORT_NULL;
                mach_port_t rootbs = MACH_PORT_NULL;
                if (f_bootstrap_get_root(bootstrap_port, &rootbs) != KERN_SUCCESS) {
                    warn("%s failed", fn_bootstrap_get_root);
                    goto reattach_failed;
                }
                if (f_bootstrap_look_up_per_user(rootbs, NULL, getuid(), &puc) != KERN_SUCCESS) {
                    warn("%s failed", fn_bootstrap_look_up_per_user);
                    goto reattach_failed;
                }

                if (task_set_bootstrap_port(mach_task_self(), puc) != KERN_SUCCESS) {
                    warn("task_set_bootstrap_port failed");
                    goto reattach_failed;
                }
                if (mach_port_deallocate(mach_task_self(), bootstrap_port) != KERN_SUCCESS) {
                    warn("mach_port_deallocate failed");
                    goto reattach_failed;
                }
                bootstrap_port = puc;
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
