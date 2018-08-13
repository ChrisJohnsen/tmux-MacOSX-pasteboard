#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <Security/AuthSession.h>

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

#define VPROC_GSK_MGR_NAME 6

static int move_to_user_namespace__101000(void)
{
    char *mgr_name = NULL;

    FIND_SYMBOL(vproc_swap_string, void *, (void *, int64_t, const char *, char **))

    if (f_vproc_swap_string(NULL, VPROC_GSK_MGR_NAME, NULL, &mgr_name)) {
        warn("%s failed", fn_vproc_swap_string);
        return -1;
    }

    if (strcmp("System", mgr_name) == 0) {
        kern_return_t kr;
        mach_port_t puc = MACH_PORT_NULL;
        mach_port_t rootbs = MACH_PORT_NULL;

        FIND_SYMBOL(bootstrap_get_root, kern_return_t, (mach_port_t, mach_port_t *))
        FIND_SYMBOL(bootstrap_strerror, const char *, (kern_return_t))
        FIND_SYMBOL(bootstrap_look_up_per_user, kern_return_t, (mach_port_t, const char *, uid_t, mach_port_t *))

        if ((kr = f_bootstrap_get_root(bootstrap_port, &rootbs)) != KERN_SUCCESS) {
            warn("%s failed: %d %s", fn_bootstrap_get_root, kr, f_bootstrap_strerror(kr));
            return -1;
        }

        if ((kr = f_bootstrap_look_up_per_user(rootbs, NULL, getuid(), &puc)) != KERN_SUCCESS) {
            warn("%s failed: %d %s", fn_bootstrap_look_up_per_user, kr, f_bootstrap_strerror(kr));
            return -1;
        }


        if ((kr = task_set_bootstrap_port(mach_task_self(), puc)) != KERN_SUCCESS) {
            warn("task_set_bootstrap_port failed: %d %s", kr, mach_error_string(kr));
            return -1;
        }

       if ((kr = mach_port_deallocate(mach_task_self(), bootstrap_port)) != KERN_SUCCESS) {
            warn("mach_port_deallocate failed: %d %s", kr, mach_error_string(kr));
            return -1;
        }

        bootstrap_port = puc;
    } else {
        FIND_SYMBOL(_vprocmgr_detach_from_console, void *, (uint64_t))

        if (f__vprocmgr_detach_from_console(0) != NULL) {
            warn("%s failed", fn__vprocmgr_detach_from_console);
            return -1;
        }
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

