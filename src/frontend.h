// src/frontend.h - glue shared by lexer.l, parser.y and main.cpp
// OWNER: Hareem | PHASE: 1 | STATUS: DONE
#pragma once

#include "ast.h"

// Name of the file being compiled, used in "file:line: error: msg".
extern const char *g_filename;

// Print "file:line: error: msg" to stderr and exit(1).
// Lexical and syntax errors stop compilation immediately (proposal, "Errors").
[[noreturn]] void fatalError(int line, const char *fmt, ...);

// Parse `path`. Returns the AST, or exits with status 1 on a lexical,
// syntax or I/O error.
Program *parseFile(const char *path);
