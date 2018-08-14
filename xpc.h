
typedef struct _xpc_pipe_s* xpc_pipe_t;

xpc_pipe_t xpc_pipe_create_from_port(mach_port_t port, int flags);

mach_port_t xpc_dictionary_copy_mach_send(xpc_object_t dict, const char *name);

int xpc_pipe_routine(xpc_pipe_t pipe,
                     xpc_object_t request,
                     xpc_object_t *reply);

