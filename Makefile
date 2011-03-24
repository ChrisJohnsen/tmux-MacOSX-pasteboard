all: test user-session-wrapper

test: test.o
	cc -framework Security -o test test.o

user-session-wrapper: user-session-wrapper.o msg.o
