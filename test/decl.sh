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

assert 1 'int main() { char x; return sizeof(x); }'
assert 2 'int main() { short int x; return sizeof(x); }'
assert 2 'int main() { int short x; return sizeof(x); }'
assert 4 'int main() { int x; return sizeof(x); }'
assert 8 'int main() { long int x; return sizeof(x); }'
assert 8 'int main() { int long x; return sizeof(x); }'

assert 8 'int main() { long long x; return sizeof(x); }'

echo OK