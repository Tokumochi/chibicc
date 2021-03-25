CC = g++
CFLAGS=-std=c11 -g -fno-common

chibicc : main.cpp
	$(CC) `llvm-config --cxxflags --ldflags --libs --system-libs` -o chibicc main.cpp $(LDFLAGS)

test: chibicc
	sh ./test.sh

clean:
	rm -f chibicc *.o *~ tmp*

.PHONY: test clean
