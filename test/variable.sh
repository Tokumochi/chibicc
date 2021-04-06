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

assert 3 'int main() { int a; a=3; return a; }'
assert 3 'int main() { int a=3; return a; }'
assert 8 'int main() { int a=3; int z=5; return a+z; }'
assert 6 'int main() { int a; int b; a=b=3; return a+b; }'
assert 3 'int main() { int foo=3; return foo; }'
assert 8 'int main() { int foo123=3; int bar=5; return foo123+bar; }'

assert 4 'int main() { int x; return sizeof(x); }'
assert 4 'int main() { int x; return sizeof x; }'
assert 8 'int main() { int *x; return sizeof(x); }'
assert 16 'int main() { int x[4]; return sizeof(x); }'
assert 48 'int main() { int x[3][4]; return sizeof(x); }'
assert 16 'int main() { int x[3][4]; return sizeof(*x); }'
assert 4 'int main() { int x[3][4]; return sizeof(**x); }'
assert 5 'int main() { int x[3][4]; return sizeof(**x) + 1; }'
assert 5 'int main() { int x[3][4]; return sizeof **x + 1; }'
assert 4 'int main() { int x[3][4]; return sizeof(**x + 1); }'
assert 4 'int main() { int x=1; return sizeof(x=2); }'
assert 1 'int main() { int x=1; sizeof(x=2); return x; }'

assert 0 'int x; int main() { return x; }'
assert 3 'int x; int main() { x=3; return x; }'
assert 7 'int x; int y; int main() { x=3; y=4; return x+y; }'
assert 7 'int x, y; int main() { x=3; y=4; return x+y; }'
assert 0 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[0]; }'
assert 1 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[1]; }'
assert 2 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[2]; }'
assert 3 'int x[4]; int main() { x[0]=0; x[1]=1; x[2]=2; x[3]=3; return x[3]; }'

assert 4 'int x; int main() { return sizeof(x); }'
assert 16 'int x[4]; int main() { return sizeof(x); }'

assert 1 'int main() { char x=1; return x; }'
assert 1 'int main() { char x=1; char y=2; return x; }'
assert 2 'int main() { char x=1; char y=2; return y; }'

assert 1 'int main() { char x; return sizeof(x); }'
assert 10 'int main() { char x[10]; return sizeof(x); }'

assert 2 'int main() { int x=2; { int x=3; } return x; }'
assert 2 'int main() { int x=2; { int x=3; } { int y=4; return x; }}'
assert 3 'int main() { int x=2; { x=3; } return x; }'

assert 8 'int main() { long x; return sizeof(x); }'
assert 2 'int main() { short x; return sizeof(x); }'

assert 24 'int main() { char *x[3]; return sizeof(x); }'
assert 8 'int main() { char (*x)[3]; return sizeof(x); }'
assert 1 'int main() { char (x); return sizeof(x); }'
assert 3 'int main() { char (x)[3]; return sizeof(x); }'
assert 12 'int main() { char (x[3])[4]; return sizeof(x); }'
assert 4 'int main() { char (x[3])[4]; return sizeof(x[0]); }'
assert 3 'int main() { char *x[3]; char y; x[0]=&y; y=3; return *x[0]; }'

echo OK