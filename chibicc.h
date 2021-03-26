#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

//
// tokenize.c
//

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

void error(const char *fmt, ...);
void error_at(char *loc, const char *fmt, ...);
void error_tok(Token *tok, const char *fmt, ...);
bool equal(Token *tok, const char *op);
Token *skip(Token *tok, const char *op);
Token *tokenize(char *input);

//
// parse.c
//

typedef enum {
    ND_ADD, // +
    ND_SUB, // -
    ND_MUL, // *
    ND_DIV, // /
    ND_NEG, // unary -
    ND_EQ,  // ==
    ND_NE,  // !=
    ND_LT,  // <
    ND_LE,  // <=
    ND_NUM, // Integer
} NodeKind;

// AST node type
typedef struct Node Node;
struct Node {
    NodeKind kind;   // Node kind
    Node *lhs;       // Left-hand side
    Node *rhs;       // Right-hand side
    llvm::Value *lv; // LLVM value
    int val;         // Used if kind == ND_NUM
};

Node *parse(Token *tok);

//
// codegen.c
//

void codegen(Node *node);