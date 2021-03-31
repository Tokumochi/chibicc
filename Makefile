CC=g++
CFLAGS=-std=c++11 -g -fno-common
SRCS=$(wildcard *.cpp)

chibicc: $(SRCS)
	$(CC) $(CFLAGS) `llvm-config --cxxflags --ldflags --libs --system-libs` -o $@ $^ $(LDFLAGS)

test: chibicc
	sh ./test.sh
	sh ./test-driver.sh

clean:
	rm -f chibicc *.o *~ tmp*
	rm -r *.dSYM

.PHONY: test clean
