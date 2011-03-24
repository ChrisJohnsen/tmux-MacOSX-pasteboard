all: test user-session-wrapper

user-session-wrapper: user-session-wrapper.o msg.o

test: test.o msg.o
