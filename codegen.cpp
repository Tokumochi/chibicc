#include "chibicc.h"

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static std::unique_ptr<llvm::Module> module;

static llvm::Function *mainFunc;
static llvm::BasicBlock *retBlock;
static llvm::Value *retValue;

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

        error_tok(node->tok, "not an lvalue");
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

    error_tok(node->tok, "invalid expression");
}

static bool gen_stmt(Node *node) {
    switch(node->kind) {
    case ND_IF: {
        llvm::BasicBlock *thenBlock;
        llvm::BasicBlock *elseBlock;
        llvm::BasicBlock *endBlock;
        thenBlock = llvm::BasicBlock::Create(context, "", mainFunc);
        endBlock = llvm::BasicBlock::Create(context, "", mainFunc);
        gen_expr(node->cond);
        node->lv = builder.CreateICmpEQ(builder.getInt32(0), node->cond->lv);
        if(node->els) {
            elseBlock = llvm::BasicBlock::Create(context, "", mainFunc);
            builder.CreateCondBr(node->lv, elseBlock, thenBlock);
         } else
            builder.CreateCondBr(node->lv, endBlock, thenBlock);
        builder.SetInsertPoint(thenBlock);
        if(!gen_stmt(node->then))
            builder.CreateBr(endBlock);
        if(node->els) {
            builder.SetInsertPoint(elseBlock);
            if(!gen_stmt(node->els))
                builder.CreateBr(endBlock);
        }
        builder.SetInsertPoint(endBlock);
        return false;
    }
    case ND_FOR: {
        llvm::BasicBlock *beginBlock;
        llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(context, "", mainFunc);
        llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(context, "", mainFunc);
        if(node->init)
            gen_stmt(node->init);
        if(node->cond) {
            beginBlock = llvm::BasicBlock::Create(context, "", mainFunc);
            builder.CreateBr(beginBlock);
            builder.SetInsertPoint(beginBlock);
            gen_expr(node->cond);
            node->lv = builder.CreateICmpEQ(builder.getInt32(0), node->cond->lv);
            builder.CreateCondBr(node->lv, endBlock, thenBlock);
        } else
            builder.CreateBr(thenBlock);
        builder.SetInsertPoint(thenBlock);
        if(!gen_stmt(node->then)) {
            if(node->inc)
                gen_expr(node->inc);
            if(node->cond)
                builder.CreateBr(beginBlock);
            else
                builder.CreateBr(thenBlock);
        }
        builder.SetInsertPoint(endBlock);
        return false;
    }
    case ND_BLOCK:
        for(Node *n = node->body; n; n = n->next)
            if(gen_stmt(n)) return true;
        return false;
    case ND_RETURN:
        gen_expr(node->lhs);
        builder.CreateStore(node->lhs->lv, retValue);
        builder.CreateBr(retBlock);
        return true;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return false;
    }

    error_tok(node->tok, "invalid statement");
}

void codegen(Function *prog) {
    module = std::make_unique<llvm::Module>("top", context);
    mainFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false),
        llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "", mainFunc));

    retBlock = llvm::BasicBlock::Create(context, "", mainFunc);
    retValue = builder.CreateAlloca(builder.getInt32Ty());

    for(Obj *var = prog->locals; var; var = var->next)
        var->lv = builder.CreateAlloca(builder.getInt32Ty());

    if(!gen_stmt(prog->body))
        builder.CreateBr(retBlock);

    builder.SetInsertPoint(retBlock);
    retValue = builder.CreateLoad(retValue);
    builder.CreateRet(retValue);

    module->print(llvm::outs(), nullptr);
}