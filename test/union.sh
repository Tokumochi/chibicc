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

assert 8 'int main() { union { int a; char b[6]; } x; return sizeof(x); }'
assert 3 'int main() { union { int a; char b[4]; } x; x.a = 515; return x.b[0]; }'
assert 2 'int main() { union { int a; char b[4]; } x; x.a = 515; return x.b[1]; }'
assert 0 'int main() { union { int a; char b[4]; } x; x.a = 515; return x.b[2]; }'
assert 0 'int main() { union { int a; char b[4]; } x; x.a = 515; return x.b[3]; }'

assert 3 'int main() { union {int a,b;} x,y; x.a=3; y.a=5; y=x; return y.a; }'
assert 3 'int main() { union {struct {int a,b;} c;} x,y; x.c.b=3; y.c.b=5; y=x; return y.c.b; }'

echo OK