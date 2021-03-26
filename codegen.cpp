#include "chibicc.h"

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static std::unique_ptr<llvm::Module> module;

static void gen_expr(Node *node) {
    switch(node->kind) {
    case ND_NUM:
        node->lv = builder.CreateAlloca(builder.getInt32Ty());
        builder.CreateStore(builder.getInt32(node->val), node->lv);
        node->lv = builder.CreateLoad(node->lv);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        node->lv = builder.CreateNeg(node->lhs->lv);
        return;
    case ND_VAR:
        node->lv = builder.CreateLoad(node->var->lv);
        return;
    case ND_ASSIGN:
        if(node->lhs->kind == ND_VAR) {
            gen_expr(node->rhs);
            builder.CreateStore(node->rhs->lv, node->lhs->var->lv);
            node->lv = node->rhs->lv;
            return;
        }

        error("not an lvalue");
    }

    gen_expr(node->lhs);
    gen_expr(node->rhs);

    switch(node->kind) {
    case ND_ADD:
        node->lv = builder.CreateAdd(node->lhs->lv, node->rhs->lv);
        return;
    case ND_SUB:
        node->lv = builder.CreateSub(node->lhs->lv, node->rhs->lv);
        return;
    case ND_MUL:
        node->lv = builder.CreateMul(node->lhs->lv, node->rhs->lv);
        return;
    case ND_DIV:
        node->lv = builder.CreateSDiv(node->lhs->lv, node->rhs->lv);
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        if(node->kind == ND_EQ)
            node->lv = builder.CreateICmpEQ(node->lhs->lv, node->rhs->lv);
        else if(node->kind == ND_NE)
            node->lv = builder.CreateICmpNE(node->lhs->lv, node->rhs->lv);
        else if(node->kind == ND_LT)
            node->lv = builder.CreateICmpSLT(node->lhs->lv, node->rhs->lv);
        else if(node->kind == ND_LE)
            node->lv = builder.CreateICmpSLE(node->lhs->lv, node->rhs->lv);

        node->lv = builder.CreateZExt(node->lv, builder.getInt32Ty());
        return;
    }

    error("invalid expression");
}

static bool gen_stmt(Node *node) {
    switch(node->kind) {
    case ND_BLOCK:
        for(Node *n = node->body; n; n = n->next)
            if(gen_stmt(n)) return true;
        return false;
    case ND_RETURN:
        gen_expr(node->lhs);
        builder.CreateRet(node->lhs->lv);
        return true;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return false;
    }

    error("invalid statement");
}

void codegen(Function *prog) {
    module = std::make_unique<llvm::Module>("top", context);
    llvm::Function* mainFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false),
        llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "", mainFunc));

    for(Obj *var = prog->locals; var; var = var->next)
        var->lv = builder.CreateAlloca(builder.getInt32Ty());

    if(gen_stmt(prog->body));

    module->print(llvm::outs(), nullptr);
}