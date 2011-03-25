#include <stdarg.h>
#include <stdio.h>

void vfmsg(FILE *f,
        const char *pre, const char *suf, const char *fmt,
        va_list ap);
extern FILE *msgout;
void vmsg(const char *pre, const char *suf, const char *fmt, va_list ap);
void msg(const char *fmt, ...);
void warn(const char *fmt, ...);
void die(int ev, const char *fmt, ...);
void die_errno(int ev, const char *fmt, ...);
