#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <bsd/string.h>
#include <stdarg.h>

static char *IdentifierStr = NULL;
static int NumVal;
static int LastChar = ' ';
static FILE *fptr = NULL;
static char *AnonExpr = "__anon_expr";
static int Depth = 0;
static char **stringLiterals = {};
static int curLit = 0;

static char var = 'v';

const char *padding[3] = { "    ", "        " };

enum Token {
    tok_eof = -1,
    tok_fn = -2,
    tok_identifier = -3,
    tok_expose = -4,
    tok_reserve = -5,
    tok_hide = -6,
    tok_int = -7,
    tok_equals = -8,
    tok_declaration = -9,
    tok_assignment = -10,
    tok_binop = -11,
    tok_quo = -12
};

typedef struct VarRefKeyValue
{
    char *Key;
    char *Val;
} VarRefKeyValue;

typedef struct VarRefMap
{
    struct VarRefKeyValue *map;
    size_t size;
    size_t allocated;
} VarRefMap;

static struct VarRefMap varMap = { .map = NULL, .size = 0, .allocated = 0 };

void VarRefMap_add(struct VarRefMap *map, struct VarRefKeyValue keyVal)
{
    if (map->allocated == 0)
    {
        struct VarRefKeyValue *temp = malloc(sizeof(struct VarRefKeyValue) * 8);
        map->allocated = 8;
        map->map = temp;
    }
    struct VarRefKeyValue *mod = map->map + map->size;
    *mod = keyVal;
    map->size++;
}

static char *VarRefMap_getValue(struct VarRefMap *map, char *key)
{
    struct VarRefKeyValue *iter = map->map;
    for (int i = 0; i < map->size; i++)
    {
        struct VarRefKeyValue chk = *iter++;
        if (strcmp(chk.Key, key) == 0)
        {
            return chk.Val;
        }
    }

    return NULL;
}

void printPad(char *format, ...)
{
    va_list args;
    if (Depth > 0)
    {
        printf("%s", padding[Depth-1]);
    }
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

static char* appendStringAlloc(char *str, char character) {
    char *newString, *ptr;

    size_t length;

    if (str == NULL)
    {
        length = 2;
    } else
    {
        length = strlen(str) + 2;
    }

    if ((newString = calloc(length, sizeof(char))) == NULL)
    {
        printf("Error allocating memory.\n");
        exit(-1);
    }

    ptr = newString;

    if (str != NULL)
    {
        for (char c = *str; c != '\0'; c = *++str)
        {
            *ptr = c;
            ptr++;
        }
    }

    *ptr = character;
    ptr++;
    *ptr = '\0';

    return newString;
}

static char nextChar()
{
    char c = getc(fptr);
    LastChar = c;
    return c;
}

static int gettok(FILE *fptr) {

    if (fptr == NULL)
    {
        printf("null file pointer");
        exit(-1);
    }

    while (isspace(LastChar)) {
        LastChar = getc(fptr);
    }

    if (LastChar == '"')
    {
        return tok_quo;
    }

    if (LastChar == '=')
    {
        LastChar = getc(fptr);
        if (LastChar == '=')
        {
            LastChar = getc(fptr);
            return tok_equals;
        }
        return tok_assignment;
    }

    if (isalpha(LastChar)) {
        char *newString = calloc(2, sizeof(char));
        if (newString == NULL) {
            exit(-1);
        }
        newString[0] = LastChar;

        LastChar = getc(fptr);
        while (isalnum(LastChar)) {
            char *appendedStr = appendStringAlloc(newString, LastChar);
            free(newString);
            newString = appendedStr;
            LastChar = getc(fptr);
        }

        IdentifierStr = newString;

        if (LastChar == ':') {
            LastChar = getc(fptr); // eat :
            return tok_declaration;
        }

        if (strcmp(IdentifierStr, "fn") == 0) {
            free(IdentifierStr);
            return tok_fn;
        }

        if (strcmp(IdentifierStr, "expose") == 0) {
            free(IdentifierStr);
            return tok_expose;
        }

        return tok_identifier;
    }

    if (isdigit(LastChar)) {
        char *numString = calloc(2, sizeof(char));

        do {
            char *newString = appendStringAlloc(numString, LastChar);
            free(numString);
            numString = newString;
            LastChar = getc(fptr);
        } while (isdigit(LastChar) );

        NumVal = atoi(numString);
        free(numString);
        return tok_int;
    }

    if (LastChar == EOF) {
        return tok_eof;
    }

    int thisChar = LastChar;
    LastChar = getc(fptr);

    return thisChar;
}


typedef struct exp_body { struct Exp **exprs; int numExprs; } exp_body;
enum context 
{
    none,
    reference,
    assignment
};

enum context Context = none;

typedef struct Exp Exp;
struct Exp
{
    enum EXP {
        exp_int,
        exp_var,
        exp_add,
        exp_call,
        exp_prototype,
        exp_function,
        exp_assignment,
        exp_declaration,
        exp_stringlit
    } tag;
    union {
        struct exp_int { int val; } exp_int;
        struct exp_var { char *name; } exp_var;
        struct exp_add { struct Exp *left; struct Exp *right; } exp_add;
        struct exp_call { char *callee; Exp** args; int numArgs; } exp_call;
        struct exp_prototype { char *name; char **args; int numArgs; } exp_prototype;
        struct exp_function { struct exp_prototype *proto; struct exp_body *body; } exp_function;
        struct exp_assignment { struct Exp *target; struct Exp *right; } exp_assignment;
        struct exp_declaration { char *type; char *name; } exp_declaration;
        struct exp_stringlit { int literalId; } exp_stringlit;
    };
};

Exp *Exp_new(Exp exp)
{
    Exp *ptr = malloc(sizeof(Exp));
    if (ptr) *ptr = exp;
    return ptr;
}

void Exp_printPrototype(struct exp_prototype *proto)
{
    if (proto == NULL)
        return;

    struct exp_prototype protostruct = *proto;
    char *protoName = protostruct.name;
    printf("%s(", protoName);
    int numArgs = proto->numArgs;
    for (int i = 0; i < numArgs; i++)
    {
        printf("%s", proto->args[i]);
    }
    printf(")");
}

void FreeProto(struct exp_prototype *proto)
{
    if (proto == NULL)
        return;

    if (proto->args != NULL)
    {
        int numArgs = proto->numArgs;
        for (int i = 0; i < numArgs; i++)
        {
            free(proto->args[i]);
        }
        free(proto->args);
    }
    free(proto->name);
    free(proto);
    return;
}

void Exp_Free(struct Exp *exp)
{
    if (!exp)
    {
        return;
    }

    if(exp == NULL)
    {
        return;
    }

    if (exp->tag == exp_function)
    {
        struct exp_prototype *proto = exp->exp_function.proto;
        FreeProto(proto);
        exp_body *body = exp->exp_function.body;
        for (int i = 0; i < body->numExprs; i++)
        {
            Exp_Free(body->exprs[i]);
        }
        free(body->exprs);
        free(body);
        free(exp);
        return;
    }
    
    if (exp->tag == exp_prototype)
    {
        struct exp_prototype proto = exp->exp_prototype;
        free(proto.args);
        free(proto.name);
        free(exp);
        return;
    }

    if (exp->tag == exp_declaration)
    {
        free(exp->exp_declaration.name);
        free(exp->exp_declaration.type);
        free(exp);
        return;
    }

    if (exp->tag == exp_assignment)
    {
        Exp_Free(exp->exp_assignment.target);
        Exp_Free(exp->exp_assignment.right);
        free(exp);
        return;
    }

    if (exp->tag == exp_int)
    {
        free(exp);
        return;
    }

    if (exp->tag == exp_var)
    {
        free(exp->exp_var.name);
        free(exp);
        return;
    }

    if (exp->tag == exp_add)
    {
        Exp_Free(exp->exp_add.left);
        Exp_Free(exp->exp_add.right);
        free(exp);
        return;
    }

    if (exp->tag == exp_call)
    {
        free(exp->exp_call.callee);
        int numArgs = exp->exp_call.numArgs;
        for (int i = 0; i < numArgs; i++)
        {
            Exp_Free(exp->exp_call.args[i]);
        }
        free(exp->exp_call.args);
        free(exp);
        return;
    }
}

char *concat(char *s1, char *s2)
{
    char *res = malloc(strlen(s1) + strlen(s2) + 1);
    strlcpy(res, s1, strlen(res) + 1);
    strlcat(res, s2, strlen(res) + strlen(s2) + 1);
    return res;
}

char * Exp_prepare(Exp *exp)
{
    if (exp->tag == exp_call)
    {
        struct exp_call call =  exp->exp_call;
        if (strcmp(call.callee, "toString") == 0)
        {
            char *varName = call.args[0]->exp_var.name;
            char *finalVar = malloc(sizeof(char) * 3);
            char *mod = finalVar;
            *mod = var;
            mod++;
            *mod = '1';
            mod++;
            *mod = '\0';

            printPad("%%%s =l call $itos(w %%%s)\n", finalVar, varName);

            return finalVar;
        }
    } else if(exp->tag == exp_var)
    {
        char *varName = exp->exp_var.name;
        int len = strlen(varName) + 1;
        char *finalVar = malloc(sizeof(char) * len);
        strlcpy(finalVar, varName, len);
        return finalVar;
    }
    return NULL;
}

char *Exp_getType(Exp *exp)
{
    if (exp->tag == exp_declaration)
    {
        return exp->exp_declaration.type;
    }
    if (exp->tag == exp_var)
    {
        char *type = VarRefMap_getValue(&varMap, exp->exp_var.name);
        if (type == NULL)
        {
            printf("type not found");
            exit(-1);
        }
        return type;
    }
    printf("type not found");
    exit(-1);
}

char *getQbeType(char *type)
{
    if (strcmp(type, "int") == 0)
    {
        return "w";
    } else if (strcmp(type, "string") == 0)
    {
        return "l";
    } else
    {
        printf("type not implemented");
        exit(-1);
    }
}

void Exp_getLeftAssignment(Exp *exp)
{
    if (exp->tag == exp_declaration)
    {
        struct exp_declaration decl = exp->exp_declaration;
        printPad("%%%s", decl.name);
        return;
    }
    if (exp->tag == exp_var)
    {
        printPad("%%%s", exp->exp_var.name);
        return;
    }

    printf("cannot assign");
    exit(-1);
}


void Exp_toIL(Exp *exp)
{
    if (exp->tag == exp_function)
    {
        char *funcName = exp->exp_function.proto->name;

        if (strcmp(funcName, "entry") == 0)
        {
            printf("export function w $main() {");
            printf("\n");
        } else {
            printf("function $%s() {", funcName);
            printf("\n");
        }

        printPad("@start\n");

        Depth++;

        exp_body *body = exp->exp_function.body;

        int numExprs = body->numExprs;

        for (int i = 0; i < numExprs; i++)
        {
            Exp_toIL(body->exprs[i]);
        }
        printPad("ret 0\n");
        Depth--;
        printPad("}\n");
    }

    if (exp->tag == exp_assignment)
    {
        Context = assignment;
        struct exp_assignment asign = exp->exp_assignment;

        Exp_getLeftAssignment(asign.target);

        char *type = Exp_getType(asign.target);
        char *qbeType = getQbeType(type);
        printf(" =%s ", qbeType);

        Exp_toIL(asign.right);
        Context = none;
    }

    if (exp->tag == exp_call)
    {
        Context = reference;
        struct exp_call call = exp->exp_call;
        if (strcmp(call.callee, "print") == 0)
        {
            char **vars = malloc(sizeof(char *) * 1);
            size_t allocated = 1;
            size_t size = 0;
            for (int i = 0; i < call.numArgs; i++)
            {
                char * finalVar = Exp_prepare(call.args[i]);
                if (allocated < size + 1)
                {
                    char **temp = realloc(vars, allocated + 1);
                    if (temp == NULL)
                    {
                        printf("error allocating memory");
                        exit(-1);
                    }
                    allocated++;
                    vars = temp;
                }
                vars[size++] = finalVar;
            }

            bool first = true;
            for (int i = 0; i < size; i++)
            {
                printPad("call $dputs(l %%%s, w 1)\n", vars[i]);
                free(vars[i]);
            }
            free(vars);
        }
        Context = none;
    }

    if (exp->tag == exp_var)
    {
        char *vartype = VarRefMap_getValue(&varMap, exp->exp_var.name);
        char *qbetype = getQbeType(vartype);

        printf("%s %%%s", qbetype, exp->exp_var.name);
    }

    if (exp->tag == exp_add)
    {
        // assume add expression is int expression;
        int val1 = exp->exp_add.left->exp_int.val;
        int val2 = exp->exp_add.right->exp_int.val;
        printf("add %d, %d\n", val1, val2);
    }

    if (exp->tag == exp_stringlit)
    {
        if (Context == assignment)
        {
            printf("copy ");
        }
        struct exp_stringlit expLit = exp->exp_stringlit;
        printf("$sl%d\n", expLit.literalId);
    }
}

void Exp_print(Exp *exp)
{
    if (!exp)
    {
        return;
    }

    if(exp == NULL)
    {
        return;
    }

    if (exp->tag == exp_function)
    {
        struct exp_prototype proto = *exp->exp_function.proto;
        Exp_printPrototype(&proto);
        printf(" {\n");
        exp_body *body = exp->exp_function.body;
        for (int i = 0; i < body->numExprs; i++)
        {
            printf("    ");
            Exp_print(body->exprs[i]);
            printf(";\n");
        }
        printf("}\n");
        return;
    }
    
    if (exp->tag == exp_prototype)
    {
        struct exp_prototype proto = exp->exp_prototype;
        Exp_printPrototype(&proto);
        return;
    }

    if (exp->tag == exp_declaration)
    {
        printf("%s: %s", exp->exp_declaration.name, exp->exp_declaration.type);
        return;
    }

    if (exp->tag == exp_assignment)
    {
        Exp_print(exp->exp_assignment.target);
        printf("=");
        Exp_print(exp->exp_assignment.right);
        return;
    }

    if (exp->tag == exp_int)
    {
        printf("%d", exp->exp_int.val);
        return;
    }

    if (exp->tag == exp_var)
    {
        printf("%s", exp->exp_var.name);
        return;
    }

    if (exp->tag == exp_add)
    {
        Exp_print(exp->exp_add.left);
        printf("+");
        Exp_print(exp->exp_add.right);
        return;
    }

    if (exp->tag == exp_call)
    {
        printf("%s(", exp->exp_call.callee);
        int numArgs = exp->exp_call.numArgs;
        Exp **args = exp->exp_call.args;
        bool first = true;
        for (int i = 0; i < numArgs; i++)
        {
            if (first)
            {
                first = false;
                Exp_print(args[i]);
                continue;
            }
            printf(",");
            Exp_print(args[i]);
        }
        printf(")");
        return;
    }
}

static int CurTok;

static int getNextToken()
{
    return CurTok = gettok(fptr);
}

#define EXP_NEW(tag, ...) \
    Exp_new((Exp){tag, {.tag=(struct tag){__VA_ARGS__}}})

typedef struct TokPrecedenceArray
{
    struct TokPrecedenceMap *map;
    size_t size;
} TokPrecedenceArray;

typedef struct TokPrecedenceMap
{
    char Key;
    int Val;
} TokPrecedenceMap;

static struct TokPrecedenceArray BinopPrecedenceArr =
{
    .size = 4,
    .map = (struct TokPrecedenceMap[])
    {
        [0] = { .Key = '<', .Val = 10 },
        [1] = { .Key = '+', .Val = 20 },
        [2] = { .Key = '-', .Val = 20 },
        [3] = { .Key = '*', .Val = 40 }
    }
};

static int getValue(struct TokPrecedenceArray arr, char key)
{
    for (int i = 0; i < arr.size; i++)
    {
        struct TokPrecedenceMap chk = arr.map[i];
        if (chk.Key == key)
        {
            return chk.Val;
        }
    }

    return -1;
}

static int GetTokPrecedence()
{
    if (CurTok > 127) // is not ascii
    {
        return -1;
    }

    int TokPrec = getValue(BinopPrecedenceArr, CurTok);
    if (TokPrec <= 0) return -1;
    return TokPrec;
}

static Exp *ParsePrimary();

static Exp *ParseStringLiteral()
{
    char next = nextChar(); // eat "

    char *str = malloc(sizeof(char) * 1);
    *str = '\0';

    while (true)
    {
        char *temp = appendStringAlloc(str, next);
        if (temp == NULL)
        {
            printf("error allocating memory");
            exit(-1);
        }
        free(str);
        str = temp;
        next = nextChar();
        if (next == '"')
        {
            break;
        }
        if (next == '\n')
        {
            break;
        }
    }
    if (next == '\n')
    {
        printf("expected """);
        exit(-1);
    }
    nextChar(); // eat "

    if (curLit == 0)
    {
        char **tempAr = malloc(sizeof(char *) * 8);
        if (tempAr == NULL)
        {
            printf("error allocating memory");
            exit(-1);
        }
        stringLiterals = tempAr;
    }

    char **set = stringLiterals + curLit;
    *set = str;
    getNextToken(); // hopefully parse symbol ;

    Exp *exp = EXP_NEW(exp_stringlit, curLit);
    curLit++;
    return exp;
}

static Exp *ParseBinOpRHS(int exprPrec, Exp *lhs)
{
    while (1)
    {
        int TokPrec = GetTokPrecedence();
        if (TokPrec < exprPrec)
        {
            return lhs;
        }

        int BinOp = CurTok;
        if (BinOp != '+')
        {
            printf("not implemented");
            free(lhs);
            exit(-1);
        }

        getNextToken(); // eat binop

        Exp *rhs = ParsePrimary();

        if (rhs == NULL)
        {
            free(lhs);
            return NULL;
        }

        int nextPrec = GetTokPrecedence();
        if (TokPrec < nextPrec)
        {
            Exp *temp = ParseBinOpRHS(TokPrec + 1, rhs);
            if (temp == NULL)
            {
                return NULL;
            }
            rhs = temp;
        }

        Exp *addExpr = EXP_NEW(exp_add, lhs, rhs);

        return addExpr;
    }
}

static Exp *ParseIntExpr()
{
    Exp *exp = EXP_NEW(exp_int, NumVal);
    getNextToken();
    return exp;
}


static Exp *ParseExpression()
{
    Exp *lhs = ParsePrimary();
    if (!lhs)
    {
        return NULL;
    }

    Exp* exp = ParseBinOpRHS(0, lhs);
    return exp;
}

static Exp *ParseDeclaration()
{
    char *varName = IdentifierStr;

    getNextToken(); // eat type

    char *type = IdentifierStr;

    struct VarRefKeyValue *temp = calloc(1, sizeof(struct VarRefKeyValue));
    if (temp == NULL)
    {
        printf("error allocating memory");
        exit(-1);
    }

    struct VarRefKeyValue keyval = (struct VarRefKeyValue) { .Key = varName, .Val = type };
    VarRefMap_add(&varMap, keyval);

    getNextToken(); // eat '='
    Exp *lhs = EXP_NEW(exp_declaration, type, varName);

    if (CurTok == ';')
    {
        return lhs;
    }

    getNextToken(); // advance to expression
    Exp *body = ParseExpression();

    if (body == NULL)
    {
        free(lhs);
        exit(-1);
    }

    Exp *expr = EXP_NEW(exp_assignment, lhs, body);

    if (CurTok != ';')
    {
        printf("expected ; after variable declaration");
        exit(-1);
    }

    return expr;
}

static Exp *ParseParenExpr()
{
    getNextToken(); // eat (
    Exp *exp = ParseExpression();
    if (!exp)
        return NULL;

    if (CurTok != ')')
    {
        printf("expected ')'");
        exit(-1);
    }
    getNextToken();
    return exp;
}

static struct exp_prototype *ParsePrototype()
{
    if (CurTok != tok_identifier)
    {
        printf("Expected function name");
        exit(-1);
    }

    char *fnName = IdentifierStr;
    getNextToken();

    char **argNames = calloc(1, sizeof(char *));
    int allocated = 1;
    int idx = 0;
    while((getNextToken() == tok_identifier))
    {
        if (!(idx < allocated))
        {
            char **temp = realloc(argNames, allocated + 1);
            if (temp == NULL)
            {
                printf("error allocating memory.");
                exit(-1);
            }
            allocated++;
            argNames = temp;
        }

        argNames[idx] = IdentifierStr;
        idx++;
    }

    if (CurTok != ')')
    {
        printf("expected ')' at end of function call");
        exit(-1);
    }

    getNextToken(); // eat ')'
    getNextToken(); // eat {

    struct exp_prototype *expr = malloc(sizeof(struct exp_prototype));
    if (expr == NULL)
    {
        free(argNames);
        return NULL;
    }
    expr->name = fnName;
    expr->args = argNames;
    expr->numArgs = idx;
    
    return expr;
}


void expect(char expect)
{
    getNextToken();
    if (CurTok != expect)
    {
        printf("expected: %c got: %c", expect, CurTok);
        exit(-1);
    }
}

static Exp *ParseDefinition()
{
    getNextToken(); // eat fn.
    struct exp_prototype *proto = ParsePrototype();

    if (proto == NULL)
    {
        return NULL;
    }

    Exp **exprs = malloc(sizeof(struct Exp *));
    size_t allocated = 1;
    size_t size = 0;
    while (CurTok != '}')
    {
        Exp *e = ParseExpression();
        if (e == NULL)
            continue;

        if (size + 1 > allocated) {
            Exp **temp = realloc(exprs, (size + 1) * sizeof(struct Exp *));
            if (temp == NULL)
            {
                printf("error allocating memory.");
                free(exprs);
                exit(-1);
            }
            exprs = temp;
            allocated++;
        }
        exprs[size++] = e;
    }

    exp_body *body = malloc(sizeof(struct exp_body));
    if (body == NULL)
    {
        printf("error allocating memory");
        for (int i = 0; i < size; i++)
        {
            Exp_Free(exprs[i]);
        }
        free(exprs);
        free(proto);
        exit(-1);
    }
    body->exprs = exprs;
    body->numExprs = size;
    struct Exp *expr = EXP_NEW(exp_function, proto, body);
    return expr;

    return NULL;
}

static Exp *ParseIdentifierExpr()
{
    char *IdName = IdentifierStr;
    getNextToken();

    if (CurTok != '(' && CurTok != tok_assignment) // simple variable ref
    {
        return EXP_NEW(exp_var, IdName);
    }

    if (CurTok == tok_assignment)
    {
        Exp *var_exp = EXP_NEW(exp_var, IdName);
        getNextToken(); // eat =

        Exp *right = ParseExpression();
        if (right == NULL)
        {
            printf("expected expression");
            exit(-1);
        }
        return EXP_NEW(exp_assignment, var_exp, right);
    }

    getNextToken(); // eat (

    Exp **args = malloc(sizeof(Exp *));
    size_t length = 0;
    size_t allocated = 1;
    if (CurTok != ')')
    {
        while (1)
        {
            Exp *arg = ParseExpression();
            if (arg != NULL)
            {
                if (allocated < length + 1)
                {
                    Exp **temp = realloc(args, (length + 1) * sizeof(Exp *));
                    if (temp == NULL)
                    {
                        printf("error allocating memory.");
                        free(args);
                        exit(-1);
                    }
                    args = temp;
                }
                args[length++] = arg;
                allocated++;
            }
            else
            {
                free(args);
                return NULL;
            }

            if (CurTok == ')')
            {
                break;
            }

            if (CurTok != ',')
            {
                printf("Expected ')' or ',' in argument list");
                exit(-1);
            }

            getNextToken();
        }
    }
	getNextToken(); // eat ')'

	Exp *exp = EXP_NEW(exp_call, IdName, args, length);

    return exp;
}

static Exp *ParsePrimary()
{
    while (true)
    {
        switch (CurTok)
        {
            case '(':
                return ParseParenExpr();

            case ';':
                getNextToken();
                return NULL;

            case '\n':
                getNextToken();

            case '\r':
                getNextToken();

            case tok_quo:
                return ParseStringLiteral();

            case tok_int:
                return ParseIntExpr();

            case tok_identifier:
                return ParseIdentifierExpr();

            case tok_declaration:
                return ParseDeclaration();

            default:
                printf("%s\n", IdentifierStr);
                printf("%d\n", CurTok);
                printf("%c\n", CurTok);
                printf("unknown token\n");
                exit(-1);
        }
    }

    exit(-1);
}

static Exp **Expressions = {};
static int ExpCount = 0;

static void ExpListAppend(Exp **list, Exp *exp)
{
    if (ExpCount == 0)
    {
        Exp **temp = malloc(sizeof(Exp *) * 8);
        if (temp == NULL)
        {
            printf("error allocating memory");
            exit(-1);
        }
        Expressions = temp;
    }

    Exp **set = Expressions + ExpCount;
    ExpCount++;
    *set = exp;
}

static Exp *ParseTopLevelExpr()
{
    Exp *e = ParseExpression();
    if (e != NULL)
    {
        struct exp_prototype *proto = malloc(sizeof(struct exp_prototype));
        char *protoName = calloc(strlen(AnonExpr), sizeof(char));
        strlcpy(protoName, AnonExpr, strlen(AnonExpr) * sizeof(char));
        proto->name = protoName;
        proto->numArgs = 0;
        proto->args = NULL;

        exp_body *body = malloc(sizeof(struct exp_body));
        if (body == NULL)
        {
            printf("error allocating memory");
            exit(-1);
        }

        Exp **exprs = calloc(1, sizeof(struct Exp *));
        if (exprs == NULL)
        {
            printf("error allocating memory");
            exit(-1);
        }
        exprs[0] = e;
        body->exprs = exprs;
        body->numExprs = 1;

        Exp *function = EXP_NEW(exp_function, proto, body);
        return function;
    }

    printf("ParseExpression returned null");

    return NULL;
}

static void HandleTopLevelExpression()
{
    Exp *topLevel = ParseTopLevelExpr();
    if (topLevel != NULL) 
    {
        ExpListAppend(Expressions, topLevel);
    } else
    {
        getNextToken();
    }
}

static void HandleDefinition()
{
    struct Exp *exp = ParseDefinition();
    if (exp != NULL)
    {
        ExpListAppend(Expressions, exp);
        return;
    }
    getNextToken();
}

static void MainLoop()
{
    while (1)
    {
        switch(CurTok)
        {
            case tok_eof:
                if (Depth > 0)
                {
                    printf("Expected }");
                    exit(-1);
                }
                return;

            case '{':
                getNextToken();
                break;

            case '}':
                getNextToken();
                break;

            case ';':
                getNextToken();
                break;

            case tok_fn:
                HandleDefinition();
                break;

            case tok_expose:
                break;

            default:
                HandleTopLevelExpression();
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    fptr = fopen(argv[1], "r");

    if (fptr == NULL)
    {
        printf("file not found.");
        exit(-1);
    }

    getNextToken();

    MainLoop();

    for (int i = 0; i < curLit; i++)
    {
        char *lit = *stringLiterals++;
        printf("data $sl%d = { b \"%s\\0\" }\n", i, lit);
    }

    for (Exp *exp = *Expressions++; ExpCount > 0; ExpCount--)
    {
        Exp_toIL(exp);
        Exp_Free(exp);
    }

    if (fptr != NULL)
    {
        fclose(fptr);
    }

    return 0;
}

