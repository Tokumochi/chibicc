#include "chibicc.h"

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static std::unique_ptr<llvm::Module> module;

static llvm::Value *varLvs[26];

static void gen_expr(Node *node) {
    switch(node->kind) {
    case ND_NUM:
        node->lv = builder.CreateAlloca(builder.getInt32Ty(), nullptr);
        builder.CreateStore(builder.getInt32(node->val), node->lv);
        node->lv = builder.CreateLoad(node->lv);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        node->lv = builder.CreateNeg(node->lhs->lv);
        return;
    case ND_VAR:
        node->lv = builder.CreateLoad(varLvs[node->name - 'a']);
        return;
    case ND_ASSIGN:
        if(node->lhs->kind == ND_VAR) {
            gen_expr(node->rhs);
            builder.CreateStore(node->rhs->lv, varLvs[node->lhs->name - 'a']);
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

static void gen_stmt(Node *node) {
    if(node->kind == ND_EXPR_STMT) {
        gen_expr(node->lhs);
        return;
    }

    error("invalid statement");
}

void codegen(Node *node) {
    module = std::make_unique<llvm::Module>("top", context);
    llvm::Function* mainFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false),
        llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "", mainFunc));

    for(int i = 0; i < 26; i++)
        varLvs[i] = builder.CreateAlloca(builder.getInt32Ty(), nullptr);

    // Traverse the AST to emit LLVM IR
    for(Node *n = node; n; n = n->next) {
        node = n;
        gen_stmt(n);
    }

    builder.CreateRet(node->lhs->lv);

    module->print(llvm::outs(), nullptr);
}