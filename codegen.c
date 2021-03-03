#include "chibicc.h"

static int var_number = 26;

static void gen_expr(Node *node);

static void give_var_number(Node *node) {
    node->number = ++var_number;
}

static int count(void) {
    static int i = 1;
    return i++;
}

static int pointer_count(Type *ty) {
    if(ty->kind == TY_INT) return 0;
    return pointer_count(ty->base) + 1;
}

static char* gen_pointer(int times) {
    char *str = calloc(times, sizeof(char));
    for(int i = 0; i < times; i++)
        str[i] = '*';
    return str;
}

static void gen_addr(Node *node) {
    int c = pointer_count(node->ty);

    switch(node->kind) {
    case ND_VAR:
        node->number = node->var->offset;
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        node->number = node->lhs->number;
        return;
    }

    error_tok(node->tok, "not an lvalue");
}

// Generate code for a given node
static void gen_expr(Node *node) {

    int c = pointer_count(node->ty);

    switch(node->kind) {
    case ND_NUM:
        give_var_number(node);
        printf("    %%s_%d = alloca i32, align 4\n", node->number);
        printf("    store i32 %d, i32* %%s_%d, align 4\n", node->val, node->number);
        printf("    %%%d = load i32, i32* %%s_%d, align 4\n", node->number, node->number);
        return;
    case ND_VAR:
        give_var_number(node);
        printf("    %%%d = load i32%s, i32%s %%%d, align 4\n", node->number, gen_pointer(c), gen_pointer(c + 1), node->var->offset);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        give_var_number(node);
        printf("    %%%d = load i32%s, i32%s %%%d, align 4\n", node->number, gen_pointer(c), gen_pointer(c + 1), node->lhs->number);
        return;
    case ND_ADDR:
        if(node->lhs->kind != ND_VAR)
            error_tok(node->tok, "not an lvalue");
        node->number = node->lhs->var->offset;
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        gen_expr(node->rhs);
        node->number = node->rhs->number;
        printf("    store i32%s %%%d, i32%s %%%d, align 4\n", gen_pointer(c), node->rhs->number, gen_pointer(c + 1), node->lhs->number);
        return;
    case ND_FUNCALL:
        give_var_number(node);
        printf("    %%%d = call i32 @%s()\n", node->number, node->funcname);
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

    error_tok(node->tok, "invalid expression");
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
        printf("    store i32 %%%d, i32* %%1, align 4\n", node->lhs->number);
        printf("    br label %%.L.return\n");
        return true;
    case ND_EXPR_STMT: 
        gen_expr(node->lhs);
        return false;
    }

    error_tok(node->tok, "invalid statement");
}

// Assign offsets to local variables.
static void assign_lvar_offsets(Function *prog) {
    for(Function *fn = prog; fn; fn = fn->next) {
        int offset = 1;
        for(Obj *var = fn->locals; var; var = var->next) {
            offset++;
            var->offset = offset;
        }
        fn->offset = offset;
    }
}

void codegen(Function *prog) {
    assign_lvar_offsets(prog);

    for(Function *fn = prog; fn; fn = fn->next) {
        printf("define i32 @%s() {\n", fn->name);

        var_number = fn->offset;

        printf("    %%1 = alloca i32, align 4\n");
        printf("    store i32 0, i32* %%1, align 4\n");

        for(Obj *var = fn->locals; var; var = var->next) {
            int c = pointer_count(var->ty);
            printf("    %%%d = alloca i32%s, align 4\n", var->offset, gen_pointer(c));
        }

        if(!gen_stmt(fn->body))
            printf("    br label %%.L.return\n");

        printf(".L.return:\n");
        printf("    %%%d = load i32, i32* %%1, align 4\n", ++var_number);
        printf("    ret i32 %%%d\n", var_number);
        printf("}\n");
    }
}