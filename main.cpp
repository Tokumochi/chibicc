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
// Tokenizer
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

// Input string
static char *current_input;

// Reports an error and exit.
static void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Reports an error location and exit.
static void verror_at(char *loc, const char *fmt, va_list ap) {
    int pos = loc - current_input;
    fprintf(stderr, "%s\n", current_input);
    fprintf(stderr, "%*s", pos, "");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

static void error_at(char *loc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

static void error_tok(Token *tok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->loc, fmt, ap);
}

// Consumes the current token if it matches 'op'.
static bool equal(Token *tok, const char *op) {
    return memcmp(tok->loc, op, tok->len) == 0 && op[tok->len] == '\0';
}

// Ensure that the current token is 's'.
static Token *skip(Token *tok, const char *s) {
    if(!equal(tok, s))
        error_tok(tok, "expected '%s'", s);
    return tok->next;
}

// Ensure that the current token is TK_NUM.
static int get_number(Token *tok) {
    if(tok->kind != TK_NUM)
        error_tok(tok, "expected a number");
    return tok->val;
}

static Token *new_token(TokenKind kind, char *start, char *end) {
    Token *tok = (Token*) calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->loc = start;
    tok->len = end - start;
    return tok;
}

static bool startswith(const char *p, const char *q) {
    return strncmp(p, q, strlen(q)) == 0;
}

// Read a punctuator token from p and returns its length.
static int read_punct(char *p) {
    if(startswith(p, "==") || startswith(p, "!=") ||
       startswith(p, "<=") || startswith(p, ">="))
        return 2;
    
    return ispunct(*p) ? 1 : 0;
}

// Tokenize 'current_input' and returns new tokens.
static Token *tokenize(void) {
    char *p = current_input;
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

        // Punctuators
        int punct_len = read_punct(p);
        if(punct_len) {
            cur = cur->next = new_token(TK_PUNC, p, p + punct_len);
            p += punct_len;
            continue;
        }

        error_at(p, "invalid token");
    }

    cur = cur->next = new_token(TK_EOF, p, p);
    return head.next;
}

//
// Parser
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

static Node *new_node(NodeKind kind) {
    Node *node = (Node*) calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr) {
    Node *node = new_node(kind);
    node->lhs = expr;
    return node;
}

static Node *new_num(int val) {
    Node *node = new_node(ND_NUM);
    node->val = val;
    return node;
}

static Node *expr(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *toK);
static Node *primary(Token **rest, Token *tok);

// expr = equality
static Node *expr(Token **rest, Token *tok) {
    return equality(rest, tok);
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality(Token **rest, Token *tok) {
    Node *node = relational(&tok, tok);

    for(;;) {
        if(equal(tok, "==")) {
            node = new_binary(ND_EQ, node, relational(&tok, tok->next));
            continue;
        }

        if(equal(tok, "!=")) {
            node = new_binary(ND_NE, node, relational(&tok, tok->next));
            continue;
        }

        *rest = tok;
        return node;
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(Token **rest, Token *tok) {
    Node *node = add(&tok, tok);

    for(;;) {
        if(equal(tok, "<")) {
            node = new_binary(ND_LT, node, add(&tok, tok->next));
            continue;
        }

        if(equal(tok, "<=")) {
            node = new_binary(ND_LE, node, add(&tok, tok->next));
            continue;
        }

        if(equal(tok, ">")) {
            node = new_binary(ND_LT, add(&tok, tok->next), node);
            continue;
        }

        if(equal(tok, ">=")) {
            node = new_binary(ND_LE, add(&tok, tok->next), node);
            continue;
        }

        *rest = tok;
        return node;
    }
}

// add = mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for(;;) {
        if(equal(tok, "+")) {
            node = new_binary(ND_ADD, node, mul(&tok, tok->next));
            continue;
        }

        if(equal(tok, "-")) {
            node = new_binary(ND_SUB, node, mul(&tok, tok->next));
            continue;
        }

        *rest = tok;
        return node;
    }
}

// mul = unary ("*" unary | "/" unary)*
static Node *mul(Token **rest, Token *tok) {
    Node *node = unary(&tok, tok);
    
    for(;;) {
        if(equal(tok, "*")) {
            node = new_binary(ND_MUL, node, unary(&tok, tok->next));
            continue;
        }

        if(equal(tok, "/")) {
            node = new_binary(ND_DIV, node, unary(&tok, tok->next));
            continue;
        }

        *rest = tok;
        return node;
    }
}

// unary = ("+" | "-") unary
//       | primary
static Node *unary(Token **rest, Token *tok) {
    if(equal(tok, "+"))
        return unary(rest, tok->next);
    
    if(equal(tok, "-"))
        return new_unary(ND_NEG, unary(rest, tok->next));
    
    return primary(rest, tok);
}

// primary = "(" expr ")" | num
static Node *primary(Token **rest, Token *tok) {
    if(equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }

    if(tok->kind == TK_NUM) {
        Node *node = new_num(tok->val);
        *rest = tok->next;
        return node;
    }

    error_tok(tok, "expected an expression");
}

//
// Code generator
//

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

int main(int argc, char **argv) {
    if(argc != 2)
        error("%s: invalid number of arguments\n", argv[0]);

    // Tokenize and parse.
    current_input = argv[1];
    Token *tok = tokenize();
    Node *node = expr(&tok, tok);

    if(tok->kind != TK_EOF)
        error_tok(tok, "extra token");

    module = std::make_unique<llvm::Module>("top", context);
    llvm::Function* mainFunc = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false),
        llvm::Function::ExternalLinkage, "main", module.get());
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "", mainFunc));

    // Traverse the AST to emit LLVM IR
    gen_expr(node);

    builder.CreateRet(node->lv);

    module->print(llvm::outs(), nullptr);

    return 0;
}