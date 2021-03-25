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

    builder.CreateRet(builder.getInt32(strtol(p, &p, 10)));

    module->print(llvm::outs(), nullptr);
}