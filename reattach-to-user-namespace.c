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
#include <unistd.h>    /* execvp   */
#include <sys/utsname.h> /* uname  */

#include "msg.h"
#include "move_to_user_namespace.h"

static const char version[] = "2.8";
static const char supported_oses[] = "OS X 10.5-10.15";

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
            warn("unknown option: %s", argv[1]);
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
     *   10.11=> 101000
     *   10.12=> 101000
     *   10.13=> 101000
     *   10.14=> 101000
     *   10.15=> 101000
     *  newer => 101000 with warning
     */
    if (100600 <= os && os <= 100900)
        os = 100600;
    else if (101000 <= os && os <= 101500)
        os = 101000;
    else if (os < 100500) {
        warn("%s: unsupported old OS, trying as if it were 10.5", argv[0]);
        os = 100500;
    } else if (os > 101500) {
        warn("%s: unsupported new OS, trying as if it were 10.10", argv[0]);
        os = 101000;
    }

    if (move_to_user_namespace(os) != 0) {
reattach_failed:
        warn("%s: unable to reattach", argv[0]);
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
