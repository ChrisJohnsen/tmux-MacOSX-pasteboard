MSG_BINARIES = test user-session-wrapper
MSG_OBJECTS = $(MSG_BINARIES:%=%.o) msg.o

OBJECTS = $(MSG_OBJECTS)
BINARIES = $(MSG_BINARIES)

all: $(BINARIES)

$(MSG_BINARIES): msg.o
$(MSG_OBJECTS): msg.h

clean:
	rm -f $(BINARIES) $(OBJECTS)
