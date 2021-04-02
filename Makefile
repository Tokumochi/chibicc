CC=g++
CFLAGS=-std=c++11 -g -fno-common

SRCS=$(wildcard *.cpp)

chibicc: $(SRCS)
	$(CC) $(CFLAGS) `llvm-config --cxxflags --ldflags --libs --system-libs` -o $@ $^ $(LDFLAGS)

test: chibicc
	sh test/arith.sh
	sh test/variable.sh
	sh test/control.sh
	sh test/pointer.sh
	sh test/function.sh
	sh test/string.sh
	sh test/struct.sh
	sh test/driver.sh

clean:
	rm -rf chibicc tmp* $(TESTS) test/*.ll test/*.exe
	rm -r *.dSYM
	find * -type f '(' -name '*~' ')' -exec rm {} ';'

.PHONY: test clean
