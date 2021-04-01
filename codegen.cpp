#include "chibicc.h"

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static std::unique_ptr<llvm::Module> module;

static llvm::Function *curFunc;
static llvm::BasicBlock *retBlock;

static Node *ret;

static bool gen_expr(Node *node);
static bool gen_stmt(Node *node);

static void type_conversion(Node *node, Type *to) {
    if(node->ty->kind == TY_CHAR && to->kind == TY_INT)
        node->lv = builder.CreateSExt(node->lv, builder.getInt32Ty());
    else if(node->ty->kind == TY_INT && to->kind == TY_CHAR)
        node->lv = builder.CreateTrunc(node->lv, builder.getInt8Ty());
}

static void load(Node *node, llvm::Value *lv) {
    if(node->ty->kind == TY_ARRAY)
        node->lv = builder.CreateInBoundsGEP(lv, {builder.getInt64(0), builder.getInt64(0)});
    else
        node->lv = builder.CreateLoad(lv);
}

static void store(Node *lhs, Node *rhs) {
    type_conversion(rhs, lhs->ty);
    builder.CreateStore(rhs->lv, lhs->lv);
}

static void gen_addr(Node *node) {
    switch(node->kind) {
    case ND_VAR:
        node->lv = node->var->lv;
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        node->lv = node->lhs->lv;
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_addr(node->rhs);
        node->lv = node->rhs->lv;
        return;
    default:
        error_tok(node->lhs->tok, "not an lvalue");
    }
}

static bool gen_expr(Node *node) {
    switch(node->kind) {
    case ND_NUM:
        node->lv = builder.CreateAlloca(builder.getInt32Ty());
        builder.CreateStore(builder.getInt32(node->val), node->lv);
        node->lv = builder.CreateLoad(node->lv);
        return false;
    case ND_NEG:
        gen_expr(node->lhs);
        node->lv = builder.CreateNeg(node->lhs->lv);
        return false;
    case ND_VAR:
        load(node, node->var->lv);
        return false;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node, node->lhs->lv);
        return false;
    case ND_ADDR:
        if(node->lhs->kind != ND_VAR)
            error_tok(node->tok, "not an lvalue");
        node->lv = node->lhs->var->lv;
        return false;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        gen_expr(node->rhs);
        store(node->lhs, node->rhs);
        node->lv = node->rhs->lv;
        return false;
    case ND_STMT_EXPR:
        for(Node *n = node->body; n; n = n->next) {
            if(gen_stmt(n)) return true;
            node->lv = n->lv;
        }
        return false;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_expr(node->rhs);
        node->lv = node->rhs->lv;
        return false;
    case ND_FUNCALL: {
        std::vector<llvm::Value*> args;
        Obj *var = node->func->params;
        for(Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            type_conversion(arg, var->ty);
            var = var->next;
            args.push_back(arg->lv);
        }

        node->lv = builder.CreateCall(node->func->lf, args);
        return false;    
    }      
    }

    if(gen_expr(node->lhs)) return true;
    if(gen_expr(node->rhs)) return true;

    switch(node->kind) {
    case ND_ADD:
        node->lv = builder.CreateAdd(node->lhs->lv, node->rhs->lv);
        return false;
    case ND_SUB:
        node->lv = builder.CreateSub(node->lhs->lv, node->rhs->lv);
        return false;
    case ND_MUL:
        node->lv = builder.CreateMul(node->lhs->lv, node->rhs->lv);
        return false;
    case ND_DIV:
        node->lv = builder.CreateSDiv(node->lhs->lv, node->rhs->lv);
        return false;
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
        return false;
    case ND_GETP:
        node->lv = builder.CreateSExt(node->rhs->lv, builder.getInt64Ty());
        node->lv = builder.CreateInBoundsGEP(node->lhs->lv, {builder.getInt64(0), node->lv});
        return false;
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
        for(Node *n = node->body; n; n = n->next) {
            if(gen_stmt(n)) return true;
            node->lv = n->lv;
        }
        return false;
    case ND_RETURN:
        if(gen_expr(node->lhs)) return true;
        store(ret, node->lhs);
        builder.CreateBr(retBlock);
        return true;
    case ND_EXPR_STMT:
        if(gen_expr(node->lhs)) return true;
        node->lv = node->lhs->lv;
        return false;
    }

    error_tok(node->tok, "invalid statement");
}

static llvm::Type *gen_type(Obj *var) {
    llvm::Type *i8_ty = builder.getInt8Ty();
    llvm::Type *i32_ty = builder.getInt32Ty();
    Type *t;
    for(t = var->ty; t->base; t = t->base) {
        if(t->kind == TY_PTR) {
            i8_ty = llvm::PointerType::getUnqual(i8_ty);
            i32_ty = llvm::PointerType::getUnqual(i32_ty);
        } else {
            i8_ty = llvm::ArrayType::get(i8_ty, t->array_len);
            i32_ty = llvm::ArrayType::get(i32_ty, t->array_len);
        }
    }

    if(t->kind == TY_CHAR)
        return i8_ty;
    else
        return i32_ty;
}

static void emit_data(Obj *prog) {
    for(Obj *var = prog; var; var = var->next) {
        if(var->is_function)
            continue;

        llvm::Type *type = gen_type(var);
        llvm::GlobalVariable *global;
        if(var->init_data)
            global = new llvm::GlobalVariable(
                *module, type, true, llvm::GlobalValue::PrivateLinkage,
                llvm::ConstantDataArray::getString(context, var->init_data, true), var->name);
        else 
            global = new llvm::GlobalVariable(
                *module, type, false, llvm::GlobalValue::ExternalLinkage,
                llvm::ConstantAggregateZero::get(type), var->name);

        var->lv = global;
    }
}

static void emit_text(Obj *prog) {
    for(Obj *fn = prog; fn; fn = fn->next) {
        if(!fn->is_function)
            continue;

        std::vector<llvm::Type*> args;
        for(Obj *var = fn->params; var; var = var->next)
            args.push_back(gen_type(var));

        fn->lf = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getInt32Ty(context), args, false),
            llvm::Function::ExternalLinkage, fn->name, module.get());
    }

    for(Obj *fn = prog; fn; fn = fn->next) {
        if(!fn->is_function)
            continue;

        curFunc = fn->lf;
        builder.SetInsertPoint(llvm::BasicBlock::Create(context, "", curFunc));

        retBlock = llvm::BasicBlock::Create(context, "", curFunc);

        ret = (Node*) calloc(1, sizeof(Node));
        ret->ty = ty_int;
        ret->lv = builder.CreateAlloca(builder.getInt32Ty());

        for(Obj *var = fn->locals; var; var = var->next)
            var->lv = builder.CreateAlloca(gen_type(var));

        int i = 0;
        for(Obj *var = fn->params; var; var = var->next)
            builder.CreateStore(curFunc->getArg(i++), var->lv);

        if(!gen_stmt(fn->body))
            builder.CreateBr(retBlock);
    
        builder.SetInsertPoint(retBlock);
        ret->lv = builder.CreateLoad(ret->lv);
        builder.CreateRet(ret->lv);
    }
}

void codegen(Obj *prog, char *path) {
    module = std::make_unique<llvm::Module>("top", context);

    emit_data(prog);
    emit_text(prog);

    if(!path || strcmp(path, "-") == 0)
        module->print(llvm::outs(), nullptr);
    else {
        std::error_code EC;
        llvm::raw_fd_ostream os(path, EC, llvm::sys::fs::OpenFlags::F_None);
        module->print(os, nullptr);
    }
}