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

assert 0 'int main() { return ""[0]; }'
assert 1 'int main() { return sizeof(""); }'

assert 97 'int main() { return "abc"[0]; }'
assert 98 'int main() { return "abc"[1]; }'
assert 99 'int main() { return "abc"[2]; }'
assert 0 'int main() { return "abc"[3]; }'
assert 4 'int main() { return sizeof("abc"); }'

assert 7 'int main() { return "\\a"[0]; }'
assert 8 'int main() { return "\\b"[0]; }'
assert 9 'int main() { return "\\t"[0]; }'
assert 10 'int main() { return "\\n"[0]; }'
assert 11 'int main() { return "\\v"[0]; }'
assert 12 'int main() { return "\\f"[0]; }'
assert 13 'int main() { return "\\r"[0]; }'
assert 27 'int main() { return "\\e"[0]; }'

assert 106 'int main() { return "\\j"[0]; }'
assert 107 'int main() { return "\\k"[0]; }'
assert 108 'int main() { return "\\l"[0]; }'

assert 7 'int main() { return "\\ax\\ny"[0]; }'
assert 120 'int main() { return "\\ax\\ny"[1]; }'
assert 10 'int main() { return "\\ax\\ny"[2]; }'
assert 121 'int main() { return "\\ax\\ny"[3]; }'

assert 0 'int main() { return "\\0"[0]; }'
assert 16 'int main() { return "\\20"[0]; }'
assert 65 'int main() { return "\\101"[0]; }'
assert 104 'int main() { return "\\1500"[0]; }'
assert 0 'int main() { return "\\x00"[0]; }'
assert 119 'int main() { return "\\x77"[0]; }'
assert 165 'int main() { return "\\xA5"[0]; }'
assert 255 'int main() { return "\\x00ff"[0]; }'

echo OK