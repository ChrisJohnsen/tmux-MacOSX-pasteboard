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

#include <string.h>    /* strlen, strcpy, strerror */
#include <stdarg.h>    /* va_...   */
#include <stdio.h>     /* vfprintf */
#include <stdlib.h>    /* malloc, exit, free */
#include <sys/errno.h> /* errno    */

#include "msg.h"

void vfmsg(FILE *f,
        const char *pre, const char *suf, const char *fmt,
        va_list ap) {
    int prelen = 0, fmtlen = 0, suflen = 0;
    if (pre) prelen = strlen(pre);
    if (fmt) fmtlen = strlen(fmt);
    if (suf) suflen = strlen(suf);
    char *newfmt = malloc(
            prelen*2 + /* %-doubled pre */
            fmtlen +
            2 + /* ':' and SP */
            suflen*2 + /* %-doubled suf */
            2 /* NL and NUL */
            );
    if (!newfmt)
        goto finish;

    char *newfmt_end = newfmt;
    if (prelen)
        while(*pre)
            if ((*newfmt_end++ = *pre++) == '%')
                *newfmt_end++ = '%';
    if (fmtlen) {
        strcpy(newfmt_end, fmt);
        newfmt_end += fmtlen;
    }
    if (suflen) {
        *newfmt_end++ = ':';
        *newfmt_end++ = ' ';
        while (*suf)
            if ((*newfmt_end++ = *suf++) == '%')
                *newfmt_end++ = '%';
    }
    *newfmt_end++ = '\n';
    *newfmt_end++ = '\0';
    fmt = newfmt;

finish:
    vfprintf(f, fmt, ap);
    fflush(f);

    if (newfmt)
        free(newfmt);
}

FILE *msgout = NULL;

void vmsg(const char *pre, const char *suf, const char *fmt, va_list ap) {
    vfmsg(msgout ? msgout : stderr, pre, suf, fmt, ap);
}

void msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vmsg(NULL, NULL, fmt, ap);
    va_end(ap);
}

void warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vmsg("warning: ", NULL, fmt, ap);
    va_end(ap);
}

void warn_errno(const char *fmt, ...) {
    int err = errno; /* just in case it gets clobbered */
    va_list ap;
    va_start(ap, fmt);
    vmsg("warning: ", strerror(err), fmt, ap);
    va_end(ap);
    errno = err;
}

void die(int ev, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vmsg("fatal: ", NULL, fmt, ap);
    va_end(ap);
    exit(ev);
}

void die_errno(int ev, const char *fmt, ...) {
    int err = errno; /* just in case it gets clobbered */
    va_list ap;
    va_start(ap, fmt);
    vmsg("fatal: ", strerror(err), fmt, ap);
    va_end(ap);
    exit(ev);
}
