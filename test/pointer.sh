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

assert 3 'int main() { int x=3; return *&x; }'
assert 3 'int main() { int x=3; int *y=&x; int **z=&y; return **z; }'
assert 5 'int main() { int x=3; int *y=&x; *y=5; return x; }'
assert 8 'int main() { int x, y; x=3; y=5; return x+y; }'
assert 8 'int main() { int x=3, y=5; return x+y; }'

assert 3 'int main() { int x[2]; int*y=x; *y=3; return *x; }'

assert 3 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return x[0]; }'
assert 4 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return x[1]; }'
assert 5 'int main() { int x[3]; *x=3; x[1]=4; x[2]=5; return x[2]; }'

assert 4 'int main() { int x[2][3]; x[0][0]=4; int y=x[0][0]; return y; }'
assert 5 'int main() { int x[2][3]; x[0][1]=5; int y=x[0][1]; return y; }'
assert 6 'int main() { int x[2][3]; x[0][2]=6; int y=x[0][2]; return y; }'
assert 7 'int main() { int x[2][3]; x[1][0]=7; int y=x[1][0]; return y; }'
assert 8 'int main() { int x[2][3]; x[1][1]=8; int y=x[1][1]; return y; }'
assert 9 'int main() { int x[2][3]; x[1][2]=9; int y=x[1][2]; return y; }'

echo OK