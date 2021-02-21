#include "chibicc.h"

static int var_number;

static void give_var_number(Node *node) {
    node->number = ++var_number;
}

static void gen_expr(Node *node) {
    if(node->kind == ND_NUM) {
        give_var_number(node);
        printf("    %%s_%d = alloca i32, align 4\n", node->number);
        printf("    store i32 %d, i32* %%s_%d, align 4\n", node->val, node->number);
        printf("    %%%d = load i32, i32* %%s_%d, align 4\n", node->number, node->number);
        return;
    }

    gen_expr(node->lhs);
    gen_expr(node->rhs);

    give_var_number(node);

    switch(node->kind) {
    case ND_ADD:
        printf("    %%%d = add nsw i32 %%%d, %%%d\n", node->number, node->lhs->number, node->rhs->number);
        return;
    case ND_SUB:
        printf("    %%%d = sub nsw i32 %%%d, %%%d\n", node->number, node->lhs->number, node->rhs->number);
        return;
    case ND_MUL:
        printf("    %%%d = mul nsw i32 %%%d, %%%d\n", node->number, node->lhs->number, node->rhs->number);
        return;
    case ND_DIV:
        printf("    %%%d = sdiv i32 %%%d, %%%d\n", node->number, node->lhs->number, node->rhs->number);
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        if(node->kind == ND_EQ)
            printf("    %%%d = icmp eq i32 %%%d, %%%d\n", node->number, node->lhs->number, node->rhs->number);
        else if(node->kind == ND_NE)
            printf("    %%%d = icmp ne i32 %%%d, %%%d\n", node->number, node->lhs->number, node->rhs->number);
        else if(node->kind == ND_LT)
            printf("    %%%d = icmp slt i32 %%%d, %%%d\n", node->number, node->lhs->number, node->rhs->number);
        else if(node->kind == ND_LE)
            printf("    %%%d = icmp sle i32 %%%d, %%%d\n", node->number, node->lhs->number, node->rhs->number);
        
        give_var_number(node);
        printf("    %%%d = zext i1 %%%d to i32\n", node->number, node->number - 1);
        return;
    }

    error("invalid expression");
}

static void gen_stmt(Node *node) {
    if(node->kind == ND_EXPR_STMT) {
        gen_expr(node->lhs);
        return;
    }

    error("invalid statement");
}

void codegen(Node *node) {
    printf("define i32 @main() {\n");

    for(Node *n = node; n; n = n->next) {
        node = n;
        gen_stmt(n);
    }

    printf("    ret i32 %%%d\n", node->lhs->number);
    printf("}\n");
}