/* parser.y — Bison grammar for the web-scraping DSL (owner: Hareem)
 *
 * OWNER: Hareem | PHASE: 1 | STATUS: DONE
 * Builds the AST in src/ast.h, following docs/HANDOFF_NOTES.md.
 * Follows the EBNF in the revised proposal ("Grammar"). Must build with zero
 * conflicts:   bison -Wall -Wcounterexamples -d src/parser.y
 *
 * There is no dangling-else problem: the then-branch is always a >> .. <<
 * block, so an `else` can only belong to the innermost open `if`.
 * One ambiguity is resolved with precedence:
 *   `row num of n + 1` -> row num of (n + 1)   (the size takes the whole
 *   expression to its right, like a prefix operator).
 */

%code requires {
#include <cstdint>
#include <string>
#include <vector>
#include "ast.h"
}

%{
#include <cstdio>
#include "frontend.h"

int yylex();
void yyerror(const char *msg);

static Program *g_program = nullptr;

// Small builders for the "fat node" AST in ast.h (see the field comments there).
static Expr *mkExpr(ExprKind k, int line) { return new Expr(k, line); }
static Expr *mkBin(Op op, Expr *a, Expr *b, int line) {
    Expr *e = new Expr(ExprKind::BINARY, line);
    e->op = op; e->lhs = a; e->rhs = b;
    return e;
}
static Expr *mkUn(Op op, Expr *a, int line) {
    Expr *e = new Expr(ExprKind::UNARY, line);
    e->op = op; e->lhs = a;
    return e;
}
static Stmt *mkStmt(StmtKind k, int line, Expr *expr = nullptr, Expr *expr2 = nullptr) {
    Stmt *s = new Stmt(k, line);
    s->expr = expr; s->expr2 = expr2;
    return s;
}
%}

%locations
%define parse.error verbose

%union {
    int64_t                 num;
    std::string            *str;
    Expr                   *expr;
    Stmt                   *stmt;
    Block                  *block;
    RecipeDef              *recipe;
    Param                  *param;
    Type                    type;
    std::vector<Expr *>    *exprs;
    std::vector<Param>     *params;
}


/* keywords */
%token OPEN "open" GRAB "grab" AS "as" HOLD "hold" RECIPE "recipe" GIVE "give"
%token SAVE "save" TO "to" SHOW "show" ROW "row" OF "of" KW_NUM "num" KW_TEXT "text"
%token FOR "for" FROM "from" UNTIL "until" WHILE "while" IF "if" ELSE "else"
%token AND "and" OR "or" NOT "not"
/* operators */
%token ASSIGN "<-" ARROW "->" BEGIN_BLOCK ">>" END_BLOCK "<<"
%token EQ "==" NE "!=" LT "<" LE "<=" GT ">" GE ">="
%token NL "end of line"
/* literals */
%token <num> NUMBER "number"
%token <str> STRING "text literal" ID "identifier"

%type <expr>   expr primary
%type <exprs>  args arglist
%type <stmt>   stmt ifstmt
%type <block>  block stmts
%type <recipe> recipe
%type <param>  param
%type <params> params paramlist
%type <type>   type rettype

/* ---- precedence, lowest first ---- */
%precedence ROWOF            /* `row num of expr` takes everything to its right */
%left       OR
%left       AND
%precedence NOT
%nonassoc   EQ NE LT LE GT GE    /* a < b < c is a syntax error */
%left       '+' '-'
%left       '*' '/' '%'
%precedence UMINUS

%start program

%%

/* ------------------------------------------------------------ program */

program
    : %empty            { g_program = new Program; g_program->mainBody = new Block; g_program->mainBody->line = 1; }
    | program recipe NL { g_program->recipes.push_back($2); }
    | program stmt NL   { g_program->mainBody->stmts.push_back($2); }
    ;

recipe
    : RECIPE ID '(' params ')' rettype block
        {
            $$ = new RecipeDef;
            $$->name = *$2; $$->params = *$4; $$->retType = $6; $$->body = $7;
            $$->line = @1.first_line;
            delete $2; delete $4;
        }
    ;

params
    : %empty            { $$ = new std::vector<Param>; }
    | paramlist         { $$ = $1; }
    ;

paramlist
    : param                 { $$ = new std::vector<Param>{*$1}; delete $1; }
    | paramlist ',' param   { $$ = $1; $$->push_back(*$3); delete $3; }
    ;

param
    : type ID           { $$ = new Param{$1, *$2, @2.first_line}; delete $2; }
    ;

type
    : KW_NUM            { $$ = Type::NUM; }
    | KW_TEXT           { $$ = Type::TEXT; }
    | ROW KW_NUM        { $$ = Type::ROW_NUM; }
    | ROW KW_TEXT       { $$ = Type::ROW_TEXT; }
    ;

rettype
    : %empty            { $$ = Type::VOID; }
    | ARROW type        { $$ = $2; }
    ;

/* ------------------------------------------------------------ blocks */

block
    : BEGIN_BLOCK NL stmts END_BLOCK    { $$ = $3; $$->line = @1.first_line; }
    ;

stmts
    : %empty            { $$ = new Block; }
    | stmts stmt NL     { $$ = $1; $$->stmts.push_back($2); }
    ;

/* ------------------------------------------------------------ statements */

stmt
    : HOLD ID ASSIGN expr
        { $$ = mkStmt(StmtKind::HOLD, @1.first_line, $4); $$->name = *$2; delete $2; }
    | ID ASSIGN expr
        { $$ = mkStmt(StmtKind::ASSIGN, @1.first_line, $3); $$->name = *$1; delete $1; }
    | ID '[' expr ']' ASSIGN expr
        { $$ = mkStmt(StmtKind::INDEX_ASSIGN, @1.first_line, $6); $$->name = *$1; $$->index = $3; delete $1; }
    | ifstmt
        { $$ = $1; }
    | WHILE expr block
        { $$ = mkStmt(StmtKind::WHILE, @1.first_line, $2); $$->body = $3; }
    | FOR ID FROM expr UNTIL expr block
        { $$ = mkStmt(StmtKind::FOR, @1.first_line, $4, $6); $$->name = *$2; $$->body = $7; delete $2; }
    | GIVE
        { $$ = mkStmt(StmtKind::GIVE, @1.first_line); }
    | GIVE expr
        { $$ = mkStmt(StmtKind::GIVE, @1.first_line, $2); }
    | OPEN expr
        { $$ = mkStmt(StmtKind::OPEN, @1.first_line, $2); }
    | SAVE expr TO expr
        { $$ = mkStmt(StmtKind::SAVE, @1.first_line, $2, $4); }
    | SHOW expr
        { $$ = mkStmt(StmtKind::SHOW, @1.first_line, $2); }
    | ID '(' args ')'
        {
            Expr *c = mkExpr(ExprKind::CALL, @1.first_line);
            c->name = *$1; c->args = *$3;
            $$ = mkStmt(StmtKind::CALL, @1.first_line, c);
            delete $1; delete $3;
        }
    ;

ifstmt
    : IF expr block
        { $$ = mkStmt(StmtKind::IF, @1.first_line, $2); $$->body = $3; }
    | IF expr block ELSE block
        { $$ = mkStmt(StmtKind::IF, @1.first_line, $2); $$->body = $3; $$->elseBody = $5; }
    | IF expr block ELSE ifstmt
        {
            /* else if: the nested IF goes into a Block of its own (HANDOFF_NOTES 4) */
            Block *wrap = new Block;
            wrap->line = @5.first_line;
            wrap->stmts.push_back($5);
            $$ = mkStmt(StmtKind::IF, @1.first_line, $2); $$->body = $3; $$->elseBody = wrap;
        }
    ;

/* ------------------------------------------------------------ expressions */

expr
    : expr OR expr              { $$ = mkBin(Op::OR,  $1, $3, @2.first_line); }
    | expr AND expr             { $$ = mkBin(Op::AND, $1, $3, @2.first_line); }
    | NOT expr                  { $$ = mkUn(Op::NOT, $2, @1.first_line); }
    | expr EQ expr              { $$ = mkBin(Op::EQ, $1, $3, @2.first_line); }
    | expr NE expr              { $$ = mkBin(Op::NE, $1, $3, @2.first_line); }
    | expr LT expr              { $$ = mkBin(Op::LT, $1, $3, @2.first_line); }
    | expr LE expr              { $$ = mkBin(Op::LE, $1, $3, @2.first_line); }
    | expr GT expr              { $$ = mkBin(Op::GT, $1, $3, @2.first_line); }
    | expr GE expr              { $$ = mkBin(Op::GE, $1, $3, @2.first_line); }
    | expr '+' expr             { $$ = mkBin(Op::ADD, $1, $3, @2.first_line); }
    | expr '-' expr             { $$ = mkBin(Op::SUB, $1, $3, @2.first_line); }
    | expr '*' expr             { $$ = mkBin(Op::MUL, $1, $3, @2.first_line); }
    | expr '/' expr             { $$ = mkBin(Op::DIV, $1, $3, @2.first_line); }
    | expr '%' expr             { $$ = mkBin(Op::MOD, $1, $3, @2.first_line); }
    | '-' expr %prec UMINUS     { $$ = mkUn(Op::NEG, $2, @1.first_line); }
    | primary                   { $$ = $1; }
    ;

primary
    : NUMBER                    { $$ = mkExpr(ExprKind::NUMBER, @1.first_line); $$->num = $1; }
    | STRING                    { $$ = mkExpr(ExprKind::STRING, @1.first_line); $$->text = *$1; delete $1; }
    | ID                        { $$ = mkExpr(ExprKind::VAR, @1.first_line); $$->name = *$1; delete $1; }
    | ID '[' expr ']'           { $$ = mkExpr(ExprKind::INDEX, @1.first_line); $$->name = *$1; $$->lhs = $3; delete $1; }
    | ID '(' args ')'           { $$ = mkExpr(ExprKind::CALL, @1.first_line); $$->name = *$1; $$->args = *$3; delete $1; delete $3; }
    | '(' expr ')'              { $$ = $2; }
    | GRAB STRING               { $$ = mkExpr(ExprKind::GRAB, @1.first_line); $$->text = *$2; delete $2; }
    | GRAB STRING AS KW_TEXT    { $$ = mkExpr(ExprKind::GRAB, @1.first_line); $$->text = *$2; delete $2; }
    | GRAB STRING AS KW_NUM     { $$ = mkExpr(ExprKind::GRAB, @1.first_line); $$->text = *$2; $$->asNum = true; delete $2; }
    | ROW KW_NUM OF expr  %prec ROWOF { $$ = mkExpr(ExprKind::ROW_NEW, @1.first_line); $$->lhs = $4; }
    | ROW KW_TEXT OF expr %prec ROWOF { $$ = mkExpr(ExprKind::ROW_NEW, @1.first_line); $$->lhs = $4; $$->rowOfText = true; }
    ;

args
    : %empty                    { $$ = new std::vector<Expr *>; }
    | arglist                   { $$ = $1; }
    ;

arglist
    : expr                      { $$ = new std::vector<Expr *>{$1}; }
    | arglist ',' expr          { $$ = $1; $$->push_back($3); }
    ;

%%

/* ------------------------------------------------------------ driver glue */

extern FILE *yyin;

void yyerror(const char *msg) {
    /* Bison's verbose messages already name the unexpected token, e.g.
       syntax error, unexpected "<<", expecting end of line */
    fatalError(yylloc.first_line, "%s", msg);
}

Program *parseFile(const char *path) {
    yyin = fopen(path, "r");
    if (!yyin) {
        fprintf(stderr, "%s: error: cannot open file\n", path);
        exit(1);
    }
    if (yyparse() != 0) exit(1);
    fclose(yyin);
    return g_program;
}
