#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <xpc/xpc.h>
#include "xpc.h"

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
    xpc_object_t dict = xpc_dictionary_create(NULL, NULL, 0);

    xpc_dictionary_set_uint64(dict, "subsystem", 0x3);
    xpc_dictionary_set_uint64(dict, "routine", 0x343);
    xpc_dictionary_set_uint64(dict, "handle", 0x0);
    xpc_dictionary_set_uint64(dict, "type", 0x1);
    xpc_dictionary_set_uint64(dict, "uid", getuid());

    kern_return_t kr;
    xpc_object_t reply;
    xpc_pipe_t pipe = xpc_pipe_create_from_port(bootstrap_port, 0);

    if (xpc_pipe_routine(pipe, dict, &reply) != 0) {
        kr = xpc_dictionary_get_uint64(reply, "error");
        warn("xpc_pipe_routine failed: %d %s", kr, mach_error_string(kr));
        return -1;
    }

    mach_port_t puc = xpc_dictionary_copy_mach_send(reply, "bootstrap");

    if ((kr = task_set_bootstrap_port(mach_task_self(), puc)) != KERN_SUCCESS) {
        warn("task_set_bootstrap_port failed: %d %s", kr, mach_error_string(kr));
        return -1;
    }

    if ((kr = mach_port_deallocate(mach_task_self(), bootstrap_port)) != KERN_SUCCESS) {
        warn("mach_port_deallocate failed: %d %s", kr, mach_error_string(kr));
        return -1;
    }

    bootstrap_port = puc;

    xpc_release(pipe);
    xpc_release(dict);
    xpc_release(reply);

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

