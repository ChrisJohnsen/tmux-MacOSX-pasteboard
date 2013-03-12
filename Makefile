# use for ppc support:
# ARCHES = i386 ppc x86_64
ARCHES = i386 x86_64
ARCH_FLAGS = $(ARCHES:%=-arch %)

CFLAGS ?= -Wall -Wextra -ansi -pedantic -std=c99
CFLAGS += $(ARCH_FLAGS) -mmacosx-version-min=10.5
LDFLAGS += $(ARCH_FLAGS)

MSG_BINARIES = test reattach-to-user-namespace
MSG_OBJECTS = $(MSG_BINARIES:%=%.o) msg.o

OBJECTS = $(MSG_OBJECTS)
BINARIES = $(MSG_BINARIES)

all: $(BINARIES)

$(MSG_BINARIES): msg.o
$(MSG_OBJECTS): msg.h

clean:
	rm -f $(BINARIES) $(OBJECTS)
