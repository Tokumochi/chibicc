#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

typedef enum {
    TK_PUNC, // Punctuators
    TK_NUM,  // Numeric literals
    TK_EOF,  // End-of-file markers
} TokenKind;

// Token type
typedef struct Token Token;
struct Token {
    TokenKind kind; // Token kind
    Token *next;    // Next token
    int val;        // If kind is TK_NUM, its value
    char *loc;      // Token location
    int len;        // Token length
};

// Reports an error and exit.
static void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Consumes the current token if it matches 'op'.
static bool equal(Token *tok, const char *op) {
    return memcmp(tok->loc, op, tok->len) == 0 && op[tok->len] == '\0';
}

// Ensure that the current token is 's'.
static Token *skip(Token *tok, const char *s) {
    if(!equal(tok, s))
        error("expected '%s'", s);
    return tok->next;
}

// Ensure that the current token is TK_NUM.
static int get_number(Token *tok) {
    if(tok->kind != TK_NUM)
        error("expected a number");
    return tok->val;
}

static Token *new_token(TokenKind kind, char *start, char *end) {
    Token *tok = (Token*) calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->loc = start;
    tok->len = end - start;
    return tok;
}

// Tokenize 'p' and returns new tokens.
static Token *tokenize(char *p) {
    Token head = {};
    Token *cur = &head;

    while(*p) {
        // Skip whitespace characters.
        if(isspace(*p)) {
            p++;
            continue;
        }

        // Numeric literal
        if(isdigit(*p)) {
            cur = cur->next = new_token(TK_NUM, p, p);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }

        // Punctuator
        if(*p == '+' || *p == '-') {
            cur = cur->next = new_token(TK_PUNC, p, p + 1);
            p++;
            continue;
        }

        error("invalid token");
    }

    cur = cur->next = new_token(TK_EOF, p, p);
    return head.next;
}

static llvm::LLVMContext context;
static llvm::IRBuilder<> builder(context);
static std::unique_ptr<llvm::Module> module;

int main(int argc, char **argv) {
    if(argc != 2)
        error("%s: invalid number of arguments\n", argv[0]);

    Token *tok = tokenize(argv[1]);

    module = std::make_unique<llvm::Module>("top", context);
    llvm::Function* mainFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false),
        llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "", mainFunc));

    llvm::Value* value = builder.CreateAlloca(builder.getInt32Ty(), nullptr);

    // The first token must be a number
    builder.CreateStore(builder.getInt32(get_number(tok)), value);
    value = builder.CreateLoad(value);
    tok = tok->next;

    // ... followed by either '+ <number>' or '- <number>'.
    while(tok->kind != TK_EOF) {
        if(equal(tok, "+")) {
            value = builder.CreateAdd(value, builder.getInt32(get_number(tok->next)));
            tok = tok->next->next;
            continue;
        }

        tok = skip(tok, "-");
        value = builder.CreateSub(value, builder.getInt32(get_number(tok)));
        tok = tok->next;    
    }

    builder.CreateRet(value);

    module->print(llvm::outs(), nullptr);
}