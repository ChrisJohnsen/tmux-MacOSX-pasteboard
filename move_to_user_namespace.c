#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <mach/mach.h>

#include "msg.h"

#define FIND_SYMBOL(NAME, RET, SIG) \
    static const char fn_ ## NAME [] = # NAME; \
    typedef RET (*ft_ ## NAME) SIG; \
    ft_ ## NAME f_ ## NAME; \
    if (!(f_ ## NAME = (ft_ ## NAME)dlsym(RTLD_NEXT, fn_ ## NAME))) { \
        warn("unable to find %s: %s", fn_ ## NAME, dlerror()); \
        return -1; \
    }

static int move_to_user_namespace__100500(void)
{
    FIND_SYMBOL(_vprocmgr_move_subset_to_user, void *, (uid_t, const char *))

    if (f__vprocmgr_move_subset_to_user(getuid(), "Background") != NULL) {
        warn("%s failed", fn__vprocmgr_move_subset_to_user);
        return -1;
    }

    return 0;
}

static int move_to_user_namespace__100600(void)
{
    FIND_SYMBOL(_vprocmgr_move_subset_to_user, void *, (uid_t, const char *, uint64_t))

    if (f__vprocmgr_move_subset_to_user(getuid(), "Background", 0) != NULL) {
        warn("%s failed", fn__vprocmgr_move_subset_to_user);
        return -1;
    }

    return 0;
}

static int move_to_user_namespace__101000(void)
{
    FIND_SYMBOL(_vprocmgr_detach_from_console, void *, (uint64_t))

    if (f__vprocmgr_detach_from_console(0) != NULL) {
        warn("%s failed: can't detach from console", fn__vprocmgr_detach_from_console);
        return -1;
    }

    return 0;
}

int move_to_user_namespace(unsigned int os)
{
    switch (os) {
    case 100500:
        return move_to_user_namespace__100500();

    case 100600:
        return move_to_user_namespace__100600();

    case 101000:
        return move_to_user_namespace__101000();

    default:
        warn("move_to_user_namespace: unhandled os value: %u", os);
        return -1;
    }
}
