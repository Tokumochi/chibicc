#include "chibicc.h"

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static std::unique_ptr<llvm::Module> module;

static llvm::Function *curFunc;
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
    case ND_DEREF:
        gen_expr(node->lhs);
        node->lv = builder.CreateLoad(node->lhs->lv);
        return;
    case ND_ADDR:
        if(node->lhs->kind != ND_VAR)
            error_tok(node->tok, "not an lvalue");
        node->lv = node->lhs->var->lv;
        return;
    case ND_ASSIGN:
        switch(node->lhs->kind) {
        case ND_VAR:
            node->lhs->lv = node->lhs->var->lv;
            break;
        case ND_DEREF:
            gen_expr(node->lhs->lhs);
            node->lhs->lv = node->lhs->lhs->lv;
            break;     
        default:
            error_tok(node->lhs->tok, "not an lvalue");
        }
        gen_expr(node->rhs);
        builder.CreateStore(node->rhs->lv, node->lhs->lv);
        node->lv = node->rhs->lv;
        return;
    case ND_FUNCALL: {
        std::vector<llvm::Value*> args;
        for(Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            args.push_back(arg->lv);
        }

        node->lv = builder.CreateCall(node->func->lf, args);
        return;    
    }      
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
        thenBlock = llvm::BasicBlock::Create(context, "", curFunc);
        endBlock = llvm::BasicBlock::Create(context, "", curFunc);
        gen_expr(node->cond);
        node->lv = builder.CreateICmpEQ(builder.getInt32(0), node->cond->lv);
        if(node->els) {
            elseBlock = llvm::BasicBlock::Create(context, "", curFunc);
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
        llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(context, "", curFunc);
        llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(context, "", curFunc);
        if(node->init)
            gen_stmt(node->init);
        if(node->cond) {
            beginBlock = llvm::BasicBlock::Create(context, "", curFunc);
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

static llvm::Type *gen_type(Obj *var) {
    llvm::Type *type = builder.getInt32Ty();
    for(Type *t = var->ty; t->kind == TY_PTR; t = t->base)
        type = llvm::PointerType::getUnqual(type);
    return type;
}

void codegen(Function *prog) {
    module = std::make_unique<llvm::Module>("top", context);

    for(Function *fn = prog; fn; fn = fn->next) {
        std::vector<llvm::Type*> args;
        for(Obj *var = fn->params; var; var = var->next)
            args.push_back(gen_type(var));

        curFunc = fn->lf = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(context), args, false),
            llvm::Function::ExternalLinkage, fn->name, module.get());
        builder.SetInsertPoint(llvm::BasicBlock::Create(context, "", curFunc));

        retBlock = llvm::BasicBlock::Create(context, "", curFunc);
        retValue = builder.CreateAlloca(builder.getInt32Ty());

        for(Obj *var = fn->locals; var; var = var->next)
            var->lv = builder.CreateAlloca(gen_type(var));

        int i = 0;
        for(Obj *var = fn->params; var; var = var->next)
            builder.CreateStore(curFunc->getArg(i++), var->lv);

        if(!gen_stmt(fn->body))
            builder.CreateBr(retBlock);
    
        builder.SetInsertPoint(retBlock);
        retValue = builder.CreateLoad(retValue);
        builder.CreateRet(retValue);
    }

    module->print(llvm::outs(), nullptr);
}