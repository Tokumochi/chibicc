#include "chibicc.h"

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static std::unique_ptr<llvm::Module> module;

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

void codegen(Node *node) {
    module = std::make_unique<llvm::Module>("top", context);
    llvm::Function* mainFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false),
        llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "", mainFunc));

    // Traverse the AST to emit LLVM IR
    gen_expr(node);

    builder.CreateRet(node->lv);

    module->print(llvm::outs(), nullptr);
}