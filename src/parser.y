/* parser.y — Bison grammar for the web-scraping DSL (owner: Hareem)
 *
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
static std::vector<Recipe *> g_recipes;
%}

%locations
%define parse.error verbose

%union {
    int64_t                 num;
    std::string            *str;
    Expr                   *expr;
    Stmt                   *stmt;
    Block                  *block;
    Recipe                 *recipe;
    Param                  *param;
    Type                    type;
    std::vector<Expr *>    *exprs;
    std::vector<Param *>   *params;
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
    : %empty            { g_program = new Program; g_program->main = new Block(1); }
    | program recipe NL { g_recipes.push_back($2); }
    | program stmt NL   { g_program->main->stmts.push_back($2); }
    ;

recipe
    : RECIPE ID '(' params ')' rettype block
        { $$ = new Recipe(*$2, *$4, $6, $7, @1.first_line); delete $2; delete $4; }
    ;

params
    : %empty            { $$ = new std::vector<Param *>; }
    | paramlist         { $$ = $1; }
    ;

paramlist
    : param                 { $$ = new std::vector<Param *>{$1}; }
    | paramlist ',' param   { $$ = $1; $$->push_back($3); }
    ;

param
    : type ID           { $$ = new Param($1, *$2, @2.first_line); delete $2; }
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
    : %empty            { $$ = new Block(0); }
    | stmts stmt NL     { $$ = $1; $$->stmts.push_back($2); }
    ;

/* ------------------------------------------------------------ statements */

stmt
    : HOLD ID ASSIGN expr               { $$ = new HoldStmt(*$2, $4, @1.first_line); delete $2; }
    | ID ASSIGN expr                    { $$ = new AssignStmt(*$1, $3, @1.first_line); delete $1; }
    | ID '[' expr ']' ASSIGN expr       { $$ = new IndexAssignStmt(*$1, $3, $6, @1.first_line); delete $1; }
    | ifstmt                            { $$ = $1; }
    | WHILE expr block                  { $$ = new WhileStmt($2, $3, @1.first_line); }
    | FOR ID FROM expr UNTIL expr block { $$ = new ForStmt(*$2, $4, $6, $7, @1.first_line); delete $2; }
    | GIVE                              { $$ = new GiveStmt(nullptr, @1.first_line); }
    | GIVE expr                         { $$ = new GiveStmt($2, @1.first_line); }
    | OPEN expr                         { $$ = new OpenStmt($2, @1.first_line); }
    | SAVE expr TO expr                 { $$ = new SaveStmt($2, $4, @1.first_line); }
    | SHOW expr                         { $$ = new ShowStmt($2, @1.first_line); }
    | ID '(' args ')'
        {
            auto *c = new CallExpr(*$1, *$3, @1.first_line);
            $$ = new CallStmt(c, @1.first_line);
            delete $1; delete $3;
        }
    ;

ifstmt
    : IF expr block
        { $$ = new IfStmt($2, $3, nullptr, @1.first_line); }
    | IF expr block ELSE block
        { $$ = new IfStmt($2, $3, $5, @1.first_line); }
    | IF expr block ELSE ifstmt
        {
            auto *wrap = new Block(@5.first_line);
            wrap->stmts.push_back($5);
            auto *s = new IfStmt($2, $3, wrap, @1.first_line);
            s->elseIsIf = true;
            $$ = s;
        }
    ;

/* ------------------------------------------------------------ expressions */

expr
    : expr OR expr              { $$ = new BinaryExpr(BinOp::Or,  $1, $3, @2.first_line); }
    | expr AND expr             { $$ = new BinaryExpr(BinOp::And, $1, $3, @2.first_line); }
    | NOT expr                  { $$ = new UnaryExpr(UnOp::Not, $2, @1.first_line); }
    | expr EQ expr              { $$ = new BinaryExpr(BinOp::Eq, $1, $3, @2.first_line); }
    | expr NE expr              { $$ = new BinaryExpr(BinOp::Ne, $1, $3, @2.first_line); }
    | expr LT expr              { $$ = new BinaryExpr(BinOp::Lt, $1, $3, @2.first_line); }
    | expr LE expr              { $$ = new BinaryExpr(BinOp::Le, $1, $3, @2.first_line); }
    | expr GT expr              { $$ = new BinaryExpr(BinOp::Gt, $1, $3, @2.first_line); }
    | expr GE expr              { $$ = new BinaryExpr(BinOp::Ge, $1, $3, @2.first_line); }
    | expr '+' expr             { $$ = new BinaryExpr(BinOp::Add, $1, $3, @2.first_line); }
    | expr '-' expr             { $$ = new BinaryExpr(BinOp::Sub, $1, $3, @2.first_line); }
    | expr '*' expr             { $$ = new BinaryExpr(BinOp::Mul, $1, $3, @2.first_line); }
    | expr '/' expr             { $$ = new BinaryExpr(BinOp::Div, $1, $3, @2.first_line); }
    | expr '%' expr             { $$ = new BinaryExpr(BinOp::Mod, $1, $3, @2.first_line); }
    | '-' expr %prec UMINUS     { $$ = new UnaryExpr(UnOp::Neg, $2, @1.first_line); }
    | primary                   { $$ = $1; }
    ;

primary
    : NUMBER                    { $$ = new NumExpr($1, @1.first_line); }
    | STRING                    { $$ = new TextExpr(*$1, @1.first_line); delete $1; }
    | ID                        { $$ = new VarExpr(*$1, @1.first_line); delete $1; }
    | ID '[' expr ']'           { $$ = new IndexExpr(*$1, $3, @1.first_line); delete $1; }
    | ID '(' args ')'           { $$ = new CallExpr(*$1, *$3, @1.first_line); delete $1; delete $3; }
    | '(' expr ')'              { $$ = $2; }
    | GRAB STRING               { $$ = new GrabExpr(*$2, false, @1.first_line); delete $2; }
    | GRAB STRING AS KW_TEXT    { $$ = new GrabExpr(*$2, false, @1.first_line); delete $2; }
    | GRAB STRING AS KW_NUM     { $$ = new GrabExpr(*$2, true,  @1.first_line); delete $2; }
    | ROW KW_NUM OF expr  %prec ROWOF { $$ = new RowNewExpr(false, $4, @1.first_line); }
    | ROW KW_TEXT OF expr %prec ROWOF { $$ = new RowNewExpr(true,  $4, @1.first_line); }
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
    g_program->recipes = g_recipes;
    return g_program;
}
