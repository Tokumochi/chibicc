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

assert 1 'int main() { struct {int a; int b;} x; x.a=1; x.b=2; return x.a; }'
assert 2 'int main() { struct {int a; int b;} x; x.a=1; x.b=2; return x.b; }'
assert 1 'int main() { struct {char a; int b; char c;} x; x.a=1; x.b=2; x.c=3; return x.a; }'
assert 2 'int main() { struct {char a; int b; char c;} x; x.a=1; x.b=2; x.c=3; return x.b; }'
assert 3 'int main() { struct {char a; int b; char c;} x; x.a=1; x.b=2; x.c=3; return x.c; }'

assert 0 'int main() { struct {char a; char b;} x[3]; x[0].a=0; return x[0].a; }'
assert 1 'int main() { struct {char a; char b;} x[3]; x[0].b=1; return x[0].b; }'
assert 2 'int main() { struct {char a; char b;} x[3]; x[1].a=2; return x[1].a; }'
assert 3 'int main() { struct {char a; char b;} x[3]; x[1].b=3; return x[1].b; }'

assert 6 'int main() { struct {char a[3]; char b[5];} x; char *p=x.a; *p=6; return x.a[0]; }'
assert 7 'int main() { struct {char a[3]; char b[5];} x; x.a[1] = 7; return x.a[1]; }'
assert 8 'int main() { struct {char a[3]; char b[5];} x; char *p=x.b; *p=8; return x.b[0]; }'
assert 9 'int main() { struct {char a[3]; char b[5];} x; x.b[1] = 9; return x.b[1]; }'

assert 6 'int main() { struct { struct { char b; } a; } x; x.a.b=6; return x.a.b; }'

assert 4 'int main() { struct {int a;} x; return sizeof(x); }'
assert 8 'int main() { struct {int a; int b;} x; return sizeof(x); }'
assert 8 'int main() { struct {int a, b;} x; return sizeof(x); }'
assert 12 'int main() { struct {int a[3];} x; return sizeof(x); }'
assert 16 'int main() { struct {int a;} x[4]; return sizeof(x); }'
assert 24 'int main() { struct {int a[3];} x[2]; return sizeof(x); }'
assert 2 'int main() { struct {char a; char b;} x; return sizeof(x); }'
assert 5 'int main() { struct {char a; int b;} x; return sizeof(x); }'
assert 0 'int main() { struct {} x; return sizeof(x); }'

assert 8 'int main() { struct t {int a; int b;} x; struct t y; return sizeof(y); }'
assert 8 'int main() { struct t {int a; int b;}; struct t y; return sizeof(y); }'
assert 2 'int main() { struct t {char a[2];}; { struct t {char a[4];}; } struct t y; return sizeof(y); }'
assert 3 'int main() { struct t {int x;}; int t=1; struct t y; y.x=2; return t+y.x; }'

assert 3 'int main() { struct t {char a;} x; struct t *y = &x; x.a=3; return y->a; }'
assert 3 'int main() { struct t {char a;} x; struct t *y = &x; y->a=3; return x.a; }'

assert 3 'int main() { struct {int a,b;} x,y; x.a=3; y=x; return y.a; }'
assert 7 'int main() { struct t {int a,b;}; struct t x; x.a=7; struct t y; struct t *z=&y; *z=x; return y.a; }'
assert 7 'int main() { struct t {int a,b;}; struct t x; x.a=7; struct t y, *p=&x, *q=&y; *q=*p; return y.a; }'
assert 5 'int main() { struct t {char a,b;} x, y; x.a=5; y=x; return y.a; }'

echo OK