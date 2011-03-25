#include <string.h>    /* strlen, strcpy, strerror */
#include <stdarg.h>    /* va_...   */
#include <stdio.h>     /* vfprintf */
#include <stdlib.h>    /* malloc, exit, free */
#include <sys/errno.h> /* errno    */

#include "msg.h"

void vfmsg(FILE *f,
        const char *pre, const char *fmt, const char *suf,
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

void vmsg(const char *pre, const char *fmt, const char *suf, va_list ap) {
    vfmsg(msgout ? msgout : stderr, pre, fmt, suf, ap);
}

void msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vmsg(NULL, fmt, NULL, ap);
    va_end(ap);
}

void warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vmsg("warning: ", fmt, NULL, ap);
    va_end(ap);
}

void die(int ev, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vmsg("fatal: ", fmt, NULL, ap);
    va_end(ap);
    exit(ev);
}

void die_errno(int ev, const char *fmt, ...) {
    int err = errno; /* just in case it gets clobbered */
    va_list ap;
    va_start(ap, fmt);
    vmsg("fatal: ", fmt, strerror(err), ap);
    va_end(ap);
    exit(ev);
}

