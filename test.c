/*
 * Copyright (c) 2011, Chris Johnsen <chris_johnsen@pobox.com>
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

#include <unistd.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#include <fcntl.h>
#include <dlfcn.h>

#include "msg.h"
#include "move_to_user_namespace.h"

#define UNUSED __attribute__ ((unused))

static int our_daemon(int nochdir, int noclose) {
    /*
     * Implementation based on description in daemon(3).
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

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static int sys_daemon(int nocd, int nocl) { return daemon(nocd, nocl); }
#pragma GCC diagnostic warning "-Wdeprecated-declarations"

static void do_daemon(const char *opt) {
    if (!(opt && *opt))
        die(2, "daemon requires an option (i.e. daemon=sys)");

    int (*daemon_)(int,int);
    if (!strcmp(opt,"sys"))
        daemon_ = sys_daemon;
    else if (!strcmp(opt, "ours"))
        daemon_ = our_daemon;
    else
        die(2, "daemon: unknown option: %s", opt);

    int r = daemon_(1,0);
    if (r) die_errno(2, "%s daemon() failed = %d", opt, r);
}

static void show_pid(const char *opt) {
    msg("pid: %d (%s)", getpid(), opt ? opt : "");
}

static void move_to_user(const char *opt) {
    if (!(opt && *opt))
        die(3, "move-to-user: requires an option (i.e. move-to-user=10.6)");

    int r;
    if (!strcmp(opt, "10.5"))
        r = move_to_user_namespace(100500);
    else if (!strcmp(opt, "10.6"))
        r = move_to_user_namespace(100600);
    else if (!strcmp(opt, "10.10"))
        r = move_to_user_namespace(101000);
    else
        die(3, "move-to-user: unknown option: %s", opt);
    if (r) die(3, "move_to_user_namespace failed");
}

typedef void *(*detach_from_console_f)(unsigned int flags);
static void detach_from_console(const char *opt UNUSED) {
    static const char detach_fn[] = "_vprocmgr_detach_from_console";
    void *f = dlsym(RTLD_NEXT, detach_fn);
    if (!f) die(4, "unable to find %s: %s", detach_fn, dlerror());
    if (((detach_from_console_f)f)(0) != NULL)
        die(4, "%s failed", detach_fn);
}

static int out_fd;
static void do_system(const char *opt) {
    if (dup2(out_fd,1) < 0)
        die_errno(5, "dup2(out_fd,1) failed");
    if (dup2(out_fd,2) < 0)
        die_errno(5, "dup2(out_fd,2) failed");

    int r = system(opt);
    if (r < 0)
        die(5, "system() failed");

    if (WIFEXITED(r))
        msg("system(%s) process exited %d", opt, WEXITSTATUS(r));
    else if (WIFSIGNALED(r))
        msg("system(%s) process terminated by signal %d", opt, WTERMSIG(r));
    else if (WIFSTOPPED(r))
        msg("system(%s) process stopped with signal %d", opt, WSTOPSIG(r));
}

static int parse_int(const char *str, char **rest_,
        char expected_stop) {
    char *rest;
    errno = 0;
    int v = strtoul(str, &rest, 0);
    if (errno)
        die_errno(1, "error parsing \"%s\" as int", str);
    if (rest && *rest != expected_stop)
        die(1, "unexpected char '%c' (%d) while parsing int from \"%s\"",
                *rest, *rest, str);
    if (rest_) *rest_ = rest;
    return v;
}

typedef int (*session_create_f)(int, int);
static void session_create(const char *opt) {
    static const char fw[] =
        "/System/Library/Frameworks/Security.framework/Versions/Current/Security";
    static const char fn[] = "SessionCreate";
    if (!(opt && *opt && strchr(opt, ',')))
        die(6, "session-createn needs two args (e.g. 0,0)");
    char *rest;
    int a = parse_int(opt, &rest, ',');
    int b = parse_int(rest+1, NULL, '\0');
    void *lib = dlopen(fw, RTLD_LAZY|RTLD_LOCAL);
    if (!lib)
        die(6, "unable to load Security framework (%s): %s", fw, dlerror());
    void *f = dlsym(lib, fn);
    msg("calling %s(0x%x,0x%x)", fn, a, b);
    int r = ((session_create_f)f)(a, b);
    if (r) die(6, "%s failed: %d", fn, r);
    if (dlclose(lib))
        die(6, "unable to close Security framework: %s", dlerror());
    /*
    OSStatus result = SessionCreate(0, 0x30);
     * 0x1,0x11,0x21,0x1001,0x1011,0x1021,0x1031 -> -60501
     * (invalid attribute bits)
     */
}

static void do_sleep(const char *opt) {
    int s = parse_int(opt, NULL, '\0');
    sleep(s);
}

static void show_msg(const char *opt) {
    msg("%s", opt);
}

typedef void cmd_func(const char *opt);
struct cmd {
    cmd_func * const func;
    const char * const str;
    const char * const desc;
};

static cmd_func
    show_msg, show_pid, do_sleep, do_daemon, detach_from_console,
    do_system, move_to_user, session_create, help;

static struct cmd all_cmds[] = {
    { show_msg,       "msg",    "=<text>   print text to stderr" },
    { show_pid,       "pid",    "=<text>   print pid and text to stderr" },
    { do_sleep,       "sleep",  "=<secs>   sleep(secs)" },
    { do_daemon,      "daemon", "=sys      system daemon(3)\n"
                                "=ours     non-Apple version" },
    { detach_from_console,
                      "detach", "          _vprocmgr_detach_from_console(0)", },
    { do_system,      "system", "=<cmd>    system(cmd)"},
    { move_to_user,   "move-to-user",
                                "=10.5     _vprocmgr_move_subset_to_user(uid,\"Background\")\n"
                                "=10.6         call with extra arg == 0\n"
                                "=10.10    custom implementation simulating _vprocmgr_move_subset_to_user" },
    { session_create, "session-create",
                                "=<a>,<b>  SessionCreate(a,b) (numeric a and b)" },
    { help,           "help",   "          show this help text" },
    { NULL, "", "" }
};

static void help(const char *opt UNUSED) {
    struct cmd *c = all_cmds;
    int w, cmd_width = 0;
    for (c = all_cmds; c->func; c++) {
        w = strlen(c->str);
        cmd_width = w > cmd_width ? w : cmd_width;
    }
    for (c = all_cmds; c->func; c++) {
        const char *nl, *s = c->desc;
        while ((nl = strchr(s, '\n'))) {
            msg("    %*s%.*s", cmd_width, c->str, nl-s, s);
            s = nl+1;
        }
        msg("    %*s%s", cmd_width, c->str, s);
    }
}

static void run_cmd(const char *cmd) {
    const char *opt = strchr(cmd, '=');
    size_t cmd_len = opt ? (size_t)(opt-cmd) : strlen(cmd);
    if (!cmd_len)
        die(1, "no command in argument: %s", cmd);
    if (opt)
        opt++;
    struct cmd *c;
    for (c = all_cmds; c->func; c++)
        if (!strncmp(cmd, c->str, cmd_len) &&
                c->str[cmd_len] == '\0') {
            c->func(opt);
            return;
        }
    die(1, "unknown command: %s (try help)", cmd);
}

int main(int argc, const char * const argv[]) {
    if ((out_fd = dup(2)) < 0)
        die_errno(1, "dup msgout");
    FILE *out = fdopen(out_fd, "w");
    if (!out) die_errno(1, "fdopen for msgout");
    msgout = out;

    if (argc < 2)
        die(1, "usage: %s <command>...\n\n"
                "    Execute the given commands.\n"
                "    Run \"%s help\" for command list.\n",
                argv[0], argv[0]);

    const char * const * cmds = argv+1;
    while(*cmds)
        run_cmd(*cmds++);

    return 0;
}
