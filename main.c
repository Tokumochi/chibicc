#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "%s: invalid number of arguments\n", argv[0]);
        return 1;
    }

    char *p = argv[1];

    printf("define i32 @main() {\n");
    printf("    %%1 = alloca i32, align 4\n");
    printf("    store i32 %ld, i32* %%1, align 4\n", strtol(p, &p, 10));
    printf("    %%2 = load i32, i32* %%1, align 4\n");

    int varCount = 2;

    while(*p) {
        varCount++;
        if(*p == '+') {
            p++;
            printf("    %%%d = add nsw i32 %%%d, %ld\n", varCount, varCount - 1, strtol(p, &p, 10));
            continue;
        }
        if(*p == '-') {
            p++;
            printf("    %%%d = sub nsw i32 %%%d, %ld\n", varCount, varCount - 1, strtol(p, &p, 10));
            continue;
        }
        fprintf(stderr, "unexpected character: '%c'\n", *p);
        return 1;
    }

    printf("    ret i32 %%%d\n", varCount);
    printf("}");
    return 0;
}