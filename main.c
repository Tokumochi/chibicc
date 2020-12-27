#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "%s: invalid number of arguments\n", argv[0]);
        return 1;
    }

    printf("define i32 @main() {\n");
    printf("    %%1 = alloca i32, align 4\n");
    printf("    store i32 %s, i32* %%1, align 4\n", argv[1]);
    printf("    %%2 = load i32, i32* %%1, align 4\n");
    printf("    ret i32 %%2\n");
    printf("}");
    return 0;
}