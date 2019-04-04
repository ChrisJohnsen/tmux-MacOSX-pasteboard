# use for ppc or i386 support:
# ARCHES = i386 ppc x86_64
ARCHES = x86_64
ARCH_FLAGS = $(ARCHES:%=-arch %)

CFLAGS ?= -Wall -Wextra -ansi -pedantic -std=c99
CFLAGS += $(ARCH_FLAGS) -mmacosx-version-min=10.5
LDFLAGS += $(ARCH_FLAGS)

MSG_BINARIES = test reattach-to-user-namespace
MSG_OBJECTS = $(MSG_BINARIES:%=%.o) msg.o move_to_user_namespace.o

OBJECTS = $(MSG_OBJECTS)
BINARIES = $(MSG_BINARIES)

all: $(BINARIES)

$(MSG_BINARIES): msg.o move_to_user_namespace.o
$(MSG_OBJECTS): msg.h move_to_user_namespace.h

clean:
	rm -f $(BINARIES) $(OBJECTS)
