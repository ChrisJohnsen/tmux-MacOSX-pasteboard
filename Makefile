all: test

test: test.o
	cc -framework Security -o test test.o
