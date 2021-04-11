#!/bin/sh
assert() {
    expected="$1"
    input="$2"

    echo "$input" | ./chibicc -o tmp.ll - || exit
    lli tmp.ll
    actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "$input => $actual"
    else
        echo "$input => $expected expected, but got $actual"
        exit 1
    fi
}

assert 1 'int main() { typedef int t; t x=1; return x; }'
assert 1 'int main() { typedef struct {int a;} t; t x; x.a=1; return x.a; }'
assert 1 'int main() { typedef int t; t t=1; return t; }'
assert 2 'int main() { typedef struct {int a;} t; { typedef int t; } t x; x.a=2; return x.a; }'
assert 4 'int main() { typedef t; t x; return sizeof(x); }'

echo OK