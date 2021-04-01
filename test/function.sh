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

assert 3 'int ret3() { return 3; } int main() { return ret3(); }'
assert 5 'int ret5() { return 5; } int main() { return ret5(); }'
assert 32 'int ret32() { return 32; } int main() { return ret32(); }'
assert 21 'int add6(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; } int main() { return add6(1,2,3,4,5,6); }'
assert 66 'int add6(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; } int main() { return add6(1,2,add6(3,4,5,6,7,8),9,10,11); }'
assert 136 'int add6(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; } int main() { return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }'
assert 55 'int fib(int x) { if (x<=1) return 1; return fib(x-1) + fib(x-2); } int main() { return fib(9); }'

assert 1 'int sub_char(char a, char b, char c) { return a-b-c; } int main() { return sub_char(7, 3, 3); }'

echo OK