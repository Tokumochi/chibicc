#include <iostream>
#include <stdlib.h>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static std::unique_ptr<llvm::Module> module;

int main(int argc, char **argv) {
    if(argc != 2) {
        std::cerr << argv[0] << ": invalid number of arguments\n";
        return 1;
    }

    char *p = argv[1];

    module = std::make_unique<llvm::Module>("top", context);
    llvm::Function* mainFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false),
        llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "", mainFunc));

    llvm::Value* value = builder.CreateAlloca(builder.getInt32Ty(), nullptr);
    builder.CreateStore(builder.getInt32(strtol(p, &p, 10)), value);
    value = builder.CreateLoad(value);

    while(*p) {
        if(*p == '+') {
            p++;
            value = builder.CreateAdd(value, builder.getInt32(strtol(p, &p, 10)));
            continue;
        }
        if(*p == '-') {
            p++;
            value = builder.CreateSub(value, builder.getInt32(strtol(p, &p, 10)));
            continue;
        }
        std::cerr << "unexpected character: '" << *p << "'\n";
        return 1;
    }

    builder.CreateRet(value);

    module->print(llvm::outs(), nullptr);
}