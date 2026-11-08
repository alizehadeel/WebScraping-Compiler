/* src/lexer.l - Flex lexer
 * OWNER: Hareem | PHASE: 1 | STATUS: DONE
 *
 * NL rule (proposal, "Grammar"): one NL token at the end of every line that
 * contains a token, plus one at end of file if the last line has a token but
 * no newline. Blank lines and comment-only lines produce nothing.
 *
 * Longest match is automatic in Flex, so  <-  <<  <=  <   and  ->  -  and
 * >>  >=  >  are all handled just by listing them. A lone  =  is an error.
 */
%option noyywrap nounput noinput never-interactive

%{
#include <cstdint>
#include <cstring>
#include <string>
#include "frontend.h"
#include "parser.tab.hpp"

static int  cur_line = 1;          /* line the scanner is on              */
static bool line_has_token = false; /* has this line produced a token yet? */

/* Every real token goes through TOK so NL bookkeeping and locations stay
   in one place. */
#define TOK(t) do { line_has_token = true; yylloc.first_line = cur_line; return (t); } while (0)

static std::string decodeText(const char *raw, size_t len) {
    /* raw includes the surrounding quotes */
    std::string out;
    for (size_t i = 1; i + 1 < len; i++) {
        char c = raw[i];
        if (c != '\\') { out += c; continue; }
        char n = raw[++i];
        switch (n) {
            case '"':  out += '"';  break;
            case '\\': out += '\\'; break;
            case 'n':  out += '\n'; break;
            default:
                fatalError(cur_line, "invalid escape '\\%c' in text literal (only \\\", \\\\ and \\n are allowed)", n);
        }
    }
    return out;
}

static int64_t parseNumber(const char *s) {
    const uint64_t limit = INT64_MAX;
    uint64_t v = 0;
    for (const char *p = s; *p; p++) {
        uint64_t digit = (uint64_t)(*p - '0');
        if (v > (limit - digit) / 10)
            fatalError(cur_line, "number literal %s is too large for a 64-bit num", s);
        v = v * 10 + digit;
    }
    return (int64_t)v;
}
%}

DIGIT   [0-9]
ID      [A-Za-z_][A-Za-z0-9_]*

%%

[ \t\r]+                ; /* whitespace */
"#"[^\n]*               ; /* comment to end of line */

\n                      {
                            int l = cur_line++;
                            if (line_has_token) {
                                line_has_token = false;
                                yylloc.first_line = l;
                                return NL;
                            }
                        }

<<EOF>>                 {
                            if (line_has_token) {
                                line_has_token = false;
                                yylloc.first_line = cur_line;
                                return NL;
                            }
                            return 0;
                        }

 /* ---- keywords (must come before ID) ---- */
"open"                  TOK(OPEN);
"grab"                  TOK(GRAB);
"as"                    TOK(AS);
"hold"                  TOK(HOLD);
"recipe"                TOK(RECIPE);
"give"                  TOK(GIVE);
"save"                  TOK(SAVE);
"to"                    TOK(TO);
"show"                  TOK(SHOW);
"row"                   TOK(ROW);
"of"                    TOK(OF);
"num"                   TOK(KW_NUM);
"text"                  TOK(KW_TEXT);
"for"                   TOK(FOR);
"from"                  TOK(FROM);
"until"                 TOK(UNTIL);
"while"                 TOK(WHILE);
"if"                    TOK(IF);
"else"                  TOK(ELSE);
"and"                   TOK(AND);
"or"                    TOK(OR);
"not"                   TOK(NOT);
 /* `count` is reserved but lexed as an ID: sema reports misuse of it. */

 /* ---- operators: Flex picks the longest match ---- */
"<-"                    TOK(ASSIGN);
"->"                    TOK(ARROW);
">>"                    TOK(BEGIN_BLOCK);
"<<"                    TOK(END_BLOCK);
"=="                    TOK(EQ);
"!="                    TOK(NE);
"<="                    TOK(LE);
">="                    TOK(GE);
"<"                     TOK(LT);
">"                     TOK(GT);
"+"                     TOK('+');
"-"                     TOK('-');
"*"                     TOK('*');
"/"                     TOK('/');
"%"                     TOK('%');
"("                     TOK('(');
")"                     TOK(')');
"["                     TOK('[');
"]"                     TOK(']');
","                     TOK(',');

"="                     fatalError(cur_line, "a single '=' is not valid; use '<-' to assign or '==' to compare");

 /* ---- literals and names ---- */
{DIGIT}+                { yylval.num = parseNumber(yytext); TOK(NUMBER); }

\"([^"\\\n]|\\.)*\"     { yylval.str = new std::string(decodeText(yytext, yyleng)); TOK(STRING); }
\"([^"\\\n]|\\.)*\\?    fatalError(cur_line, "unterminated text literal");

{ID}                    { yylval.str = new std::string(yytext); TOK(ID); }

.                       {
                            unsigned char c = (unsigned char)yytext[0];
                            if (c >= 32 && c < 127)
                                fatalError(cur_line, "invalid character '%c'", c);
                            fatalError(cur_line, "invalid character (byte 0x%02x)", c);
                        }

%%
