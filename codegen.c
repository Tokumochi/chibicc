#include "chibicc.h"

static int var_number = 26;

static void give_var_number(Node *node) {
    node->number = ++var_number;
}

static int count(void) {
    static int i = 1;
    return i++;
}

// Generate code for a given node
static void gen_expr(Node *node) {
    switch(node->kind) {
    case ND_NUM:
        give_var_number(node);
        printf("    %%s_%d = alloca i32, align 4\n", node->number);
        printf("    store i32 %d, i32* %%s_%d, align 4\n", node->val, node->number);
        printf("    %%%d = load i32, i32* %%s_%d, align 4\n", node->number, node->number);
        return;
    case ND_VAR:
        give_var_number(node);
        printf("    %%%d = load i32, i32* %%%d, align 4\n", node->number, node->var->offset);
        return;
    case ND_ASSIGN:
        if(node->lhs->kind == ND_VAR) {
            gen_expr(node->rhs);
            node->number = node->rhs->number;
            printf("    store i32 %%%d, i32* %%%d, align 4\n", node->rhs->number,  node->lhs->var->offset);
            return;
        }

        error("not an lvalue");
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

static bool gen_stmt(Node *node) {
    switch(node->kind) {
    case ND_IF: {
        int c = count();
        gen_expr(node->cond);
        give_var_number(node);
        printf("    %%%d = icmp eq i32 0, %%%d\n", node->number, node->cond->number);
        if(node->els)
            printf("    br i1 %%%d, label %%.L.else.%d, label %%.L.then.%d\n", node->number, c, c);
        else
            printf("    br i1 %%%d, label %%.L.end.%d, label %%.L.then.%d\n", node->number, c, c);
        printf(".L.then.%d:\n", c);
        if(!gen_stmt(node->then))
            printf("    br label %%.L.end.%d\n", c);
        if(node->els) {
            printf(".L.else.%d:\n", c);
            if(!gen_stmt(node->els))
                printf("    br label %%.L.end.%d\n", c);
        }
        printf(".L.end.%d:\n", c);
        return false;
    }
    case ND_FOR: {
        int c = count();
        if(node->init)
            gen_stmt(node->init);
        if(node->cond) {
            printf("    br label %%.L.begin.%d\n", c);
            printf(".L.begin.%d:\n", c);
            gen_expr(node->cond);
            give_var_number(node);
            printf("    %%%d = icmp eq i32 0, %%%d\n", node->number, node->cond->number);
            printf("    br i1 %%%d, label %%.L.end.%d, label %%.L.then.%d\n", node->number, c, c);
        } else
            printf("    br label %%.L.then.%d\n", c);
        printf(".L.then.%d:\n", c);
        if(!gen_stmt(node->then)) {
            if(node->inc)
                gen_expr(node->inc);
            if(node->cond)
                printf("    br label %%.L.begin.%d\n", c);
            else
                printf("    br label %%.L.then.%d\n", c);
        }
        printf(".L.end.%d:\n", c);
        return false;
    }
    case ND_BLOCK:
        for(Node *n = node->body; n; n = n->next)
            if(gen_stmt(n)) return true;
        return false;
    case ND_RETURN:
        gen_expr(node->lhs);
        printf("    store i32 %%%d, i32* %%s_ret, align 4\n", node->lhs->number);
        printf("    br label %%.L.return\n");
        return true;
    case ND_EXPR_STMT: 
        gen_expr(node->lhs);
        return false;
    }

    error("invalid statement");
}

// Assign offsets to local variables.
static void assign_lvar_offsets(Function *prog) {
    int total = 0;
    for(Obj *var = prog->locals; var; var = var->next) {
        total++;
        var->offset = total;
    }
    prog->total = total;
}

void codegen(Function *prog) {
    assign_lvar_offsets(prog);

    printf("define i32 @main() {\n");

    var_number = prog->total;

    printf("    %%s_ret = alloca i32, align 4\n");
    printf("    store i32 0, i32* %%s_ret, align 4\n");

    for(int n = 1; n <= prog->total; n++) {
        printf("    %%%d = alloca i32, align 4\n", n);
        printf("    store i32 0, i32* %%%d, align 4\n", n);
    }

    if(!gen_stmt(prog->body))
        printf("    br label %%.L.return\n");

    printf(".L.return:\n");
    printf("    %%ret = load i32, i32* %%s_ret, align 4\n");
    printf("    ret i32 %%ret\n");
    printf("}\n");
}