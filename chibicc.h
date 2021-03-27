#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

typedef struct Node Node;

//
// tokenize.c
//

// Token
typedef enum {
    TK_IDENT,   // Identifiers
    TK_PUNCT,   // Punctuators
    TK_KEYWORD, // Keywords
    TK_NUM,     // Numeric literals
    TK_EOF,     // End-of-file markers
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

// Local variable
typedef struct Obj Obj;
struct Obj {
    Obj *next;
    char *name;      // Variable name
    llvm::Value *lv; // LLVM value of variable
};

// Function
typedef struct Function Function;
struct Function {
    Node *body;
    Obj *locals;
};

// AST node
typedef enum {
    ND_ADD,       // +
    ND_SUB,       // -
    ND_MUL,       // *
    ND_DIV,       // /
    ND_NEG,       // unary -
    ND_EQ,        // ==
    ND_NE,        // !=
    ND_LT,        // <
    ND_LE,        // <=
    ND_ASSIGN,    // =
    ND_BLOCK,     // { ... }
    ND_RETURN,    // "return"
    ND_IF,        // "if"
    ND_FOR,       // "for" or "while"
    ND_EXPR_STMT, // Expression statement
    ND_VAR,       // Variable
    ND_NUM,       // Integer
} NodeKind;

// AST node type
struct Node {
    NodeKind kind;   // Node kind
    Node *next;      // Next node
    Token *tok;      // Representative token

    Node *lhs;       // Left-hand side
    Node *rhs;       // Right-hand side

    // "if" or "for" statement
    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;

    // Block
    Node *body;

    llvm::Value *lv; // LLVM value
    Obj *var;        // Used if kind == ND_VAR
    int val;         // Used if kind == ND_NUM
};

Function *parse(Token *tok);

//
// codegen.c
//

void codegen(Function *prog);