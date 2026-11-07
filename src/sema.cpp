// src/sema.cpp - semantic analysis: symbol table, both passes, every compile-time check
// OWNER: Aleena | PHASE: 1 | STATUS: DONE
//
// What this file does to the AST (the contract with the interpreter and codegen):
//   * Expr::type        - the type of every expression
//   * Expr::sym         - VAR / INDEX -> the variable, CALL -> the recipe
//   * Expr::isCount     - the CALL is the built-in count()
//   * Stmt::sym         - the variable declared by HOLD / FOR, or used by ASSIGN / INDEX_ASSIGN
//   * Param::sym, RecipeDef::sym, RecipeDef::frameSize, Program::mainFrameSize
//   * Symbol::slot      - index of a variable in its frame (params + locals of one recipe, or main)
//
// Error recovery: nothing here stops at the first error. A bad expression gets type ERROR and
// checks that involve an ERROR operand stay silent, so one mistake does not cause a cascade.
#include "sema.h"
#include <algorithm>
#include <cstdio>
#include "rt.h"      // rt_selector_valid

// ============================================================================ SymbolTable
void SymbolTable::pushScope() {
    Scope s;
    if (!scopes_.empty()) s.pageOpened = scopes_.back().pageOpened;   // inner blocks inherit
    scopes_.push_back(std::move(s));
}

void SymbolTable::popScope() { scopes_.pop_back(); }

Symbol* SymbolTable::declareVar(const std::string& name, Type t, int line, bool readonly) {
    if (scopes_.empty()) pushScope();
    Scope& cur = scopes_.back();
    if (cur.vars.count(name)) return nullptr;                         // same scope: duplicate
    arena_.emplace_back(new Symbol);
    Symbol* s = arena_.back().get();
    s->kind = Symbol::VARIABLE;
    s->name = name;
    s->type = t;
    s->line = line;
    s->readonly = readonly;
    cur.vars[name] = s;
    return s;
}

Symbol* SymbolTable::lookupVar(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto f = it->vars.find(name);
        if (f != it->vars.end()) return f->second;
    }
    return nullptr;
}

Symbol* SymbolTable::declareRecipe(const std::string& name, Type ret,
                                   std::vector<Type> params, int line) {
    if (recipes_.count(name)) return nullptr;
    arena_.emplace_back(new Symbol);
    Symbol* s = arena_.back().get();
    s->kind = Symbol::RECIPE;
    s->name = name;
    s->type = ret;
    s->line = line;
    s->paramTypes = std::move(params);
    recipes_[name] = s;
    return s;
}

Symbol* SymbolTable::lookupRecipe(const std::string& name) const {
    auto f = recipes_.find(name);
    return f == recipes_.end() ? nullptr : f->second;
}

void SymbolTable::enterRecipe() {
    saved_.push_back(std::move(scopes_));
    scopes_.clear();                                                  // main variables are invisible
}

void SymbolTable::leaveRecipe() {
    scopes_ = std::move(saved_.back());
    saved_.pop_back();
}

void SymbolTable::markPageOpened() {
    if (!scopes_.empty()) scopes_.back().pageOpened = true;           // this scope only
}

bool SymbolTable::pageOpened() const {
    return !scopes_.empty() && scopes_.back().pageOpened;
}

// ============================================================================ helpers
static const char* opName(Op op) {
    switch (op) {
        case Op::ADD: return "+";   case Op::SUB: return "-";   case Op::MUL: return "*";
        case Op::DIV: return "/";   case Op::MOD: return "%";   case Op::EQ:  return "==";
        case Op::NE:  return "!=";  case Op::LT:  return "<";   case Op::LE:  return "<=";
        case Op::GT:  return ">";   case Op::GE:  return ">=";  case Op::AND: return "and";
        case Op::OR:  return "or";  case Op::NOT: return "not"; case Op::NEG: return "-";
    }
    return "?";
}

static std::string q(const std::string& name) { return "'" + name + "'"; }

void Sema::report(int line, const std::string& msg) { errors_.push_back({line, msg}); }

void Sema::printErrors(const std::string& filename) const {
    for (const SemaError& e : errors_)
        std::fprintf(stderr, "%s:%d: error: %s\n", filename.c_str(), e.line, e.message.c_str());
}

// Declares a variable in the current scope. On a duplicate the error is reported and the
// existing symbol is returned, so the rest of the program is still checked sensibly.
Symbol* Sema::declareName(const std::string& name, Type t, int line, bool readonly) {
    if (name == "count") report(line, "'count' is a reserved name");
    Symbol* s = syms_.declareVar(name, t, line, readonly);
    if (!s) {
        Symbol* old = syms_.lookupVar(name);
        report(line, q(name) + " is already declared in this scope (line " +
                         std::to_string(old->line) + ")");
        return old;
    }
    s->slot = nextSlot_++;
    return s;
}

// ============================================================================ driver
bool Sema::analyze(Program& prog) {
    errors_.clear();
    collectRecipes(prog);                                   // pass 1
    for (RecipeDef* r : prog.recipes) checkRecipe(*r);      // pass 2: recipe bodies
    currentRecipe_ = nullptr;                               //         the main program
    nextSlot_ = 0;
    if (prog.mainBody) checkBlock(*prog.mainBody);
    prog.mainFrameSize = nextSlot_;
    std::stable_sort(errors_.begin(), errors_.end(),
                     [](const SemaError& a, const SemaError& b) { return a.line < b.line; });
    return errors_.empty();
}

// ---------------------------------------------------------------- pass 1
void Sema::collectRecipes(Program& prog) {
    for (RecipeDef* r : prog.recipes) {
        if (r->name == "count") report(r->line, "'count' is a reserved name");
        std::vector<Type> ptypes;
        for (const Param& p : r->params) ptypes.push_back(p.type);
        Symbol* s = syms_.declareRecipe(r->name, r->retType, ptypes, r->line);
        if (!s) {
            Symbol* first = syms_.lookupRecipe(r->name);
            report(r->line, "duplicate recipe " + q(r->name) + " (first defined at line " +
                                std::to_string(first->line) + ")");
            r->sym = first;
        } else {
            s->def = r;
            r->sym = s;
        }
    }
}

// ---------------------------------------------------------------- pass 2: recipes and statements
void Sema::checkRecipe(RecipeDef& r) {
    currentRecipe_ = &r;
    nextSlot_ = 0;
    syms_.enterRecipe();
    syms_.pushScope();                    // the body scope: parameters and top-level locals
    for (Param& p : r.params) p.sym = declareName(p.name, p.type, p.line);
    if (r.body) checkStmts(*r.body);
    syms_.popScope();
    syms_.leaveRecipe();
    r.frameSize = nextSlot_;
    currentRecipe_ = nullptr;
}

void Sema::checkBlock(Block& b) {
    syms_.pushScope();
    checkStmts(b);
    syms_.popScope();
}

void Sema::checkStmts(Block& b) {
    for (Stmt* s : b.stmts) checkStmt(*s);
}

void Sema::checkStmt(Stmt& s) {
    switch (s.kind) {
    case StmtKind::HOLD: {
        // The initializer is checked BEFORE the name is declared, so "hold x <- x" does not see itself.
        Type t = valueExpr(*s.expr);
        s.sym = declareName(s.name, t, s.line);
        break;
    }
    case StmtKind::ASSIGN: {
        Type t = valueExpr(*s.expr);
        Symbol* v = syms_.lookupVar(s.name);
        if (!v) { report(s.line, "undeclared variable " + q(s.name)); break; }
        s.sym = v;
        if (v->readonly)
            report(s.line, "cannot assign to the for loop variable " + q(s.name));
        else if (t != Type::ERROR && v->type != Type::ERROR && t != v->type)
            report(s.line, std::string("cannot assign ") + typeName(t) + " to " + q(s.name) +
                               " of type " + typeName(v->type));
        break;
    }
    case StmtKind::INDEX_ASSIGN: {
        expectNum(*s.index, "index");
        Type t = valueExpr(*s.expr);
        Symbol* v = syms_.lookupVar(s.name);
        if (!v) { report(s.line, "undeclared variable " + q(s.name)); break; }
        s.sym = v;
        if (!isRow(v->type)) {
            if (v->type != Type::ERROR)
                report(s.line, q(s.name) + " is not a row (it is " + typeName(v->type) + ")");
        } else if (t != Type::ERROR && t != elementType(v->type)) {
            report(s.line, std::string("cannot assign ") + typeName(t) + " to an element of " +
                               q(s.name) + " (" + typeName(v->type) + ")");
        }
        break;
    }
    case StmtKind::IF:
        expectNum(*s.expr, "condition");
        checkBlock(*s.body);
        if (s.elseBody) checkBlock(*s.elseBody);
        break;
    case StmtKind::WHILE:
        expectNum(*s.expr, "condition");
        checkBlock(*s.body);
        break;
    case StmtKind::FOR:
        expectNum(*s.expr, "for bound");           // bounds belong to the OUTER scope
        expectNum(*s.expr2, "for bound");
        syms_.pushScope();                         // the loop variable lives in the body's scope
        s.sym = declareName(s.name, Type::NUM, s.line, /*readonly=*/true);
        checkStmts(*s.body);
        syms_.popScope();
        break;
    case StmtKind::GIVE: {
        Type t = s.expr ? valueExpr(*s.expr) : Type::VOID;
        if (!currentRecipe_) { report(s.line, "give outside a recipe"); break; }
        Type ret = currentRecipe_->retType;
        const std::string& rn = currentRecipe_->name;
        if (s.expr) {
            if (ret == Type::VOID)
                report(s.line, "give with a value in recipe " + q(rn) + ", which has no return type");
            else if (t != Type::ERROR && t != ret)
                report(s.line, std::string("give returns ") + typeName(t) + " but recipe " + q(rn) +
                                   " returns " + typeName(ret));
        } else if (ret != Type::VOID) {
            report(s.line, "bare give in recipe " + q(rn) + ", which returns " + typeName(ret));
        }
        break;
    }
    case StmtKind::OPEN: {
        Type t = valueExpr(*s.expr);
        if (t != Type::TEXT && t != Type::ERROR)
            report(s.line, std::string("open needs a text path, found ") + typeName(t));
        if (currentRecipe_) report(s.line, "open is not allowed inside a recipe");
        else syms_.markPageOpened();               // marks the CURRENT scope only
        break;
    }
    case StmtKind::SAVE: {
        valueExpr(*s.expr);
        Type p = valueExpr(*s.expr2);
        if (p != Type::TEXT && p != Type::ERROR)
            report(s.line, std::string("save needs a text path, found ") + typeName(p));
        break;
    }
    case StmtKind::SHOW:
        valueExpr(*s.expr);
        break;
    case StmtKind::CALL:
        checkExpr(*s.expr);                        // a call statement may discard a value
        break;
    }
}

// ---------------------------------------------------------------- pass 2: expressions
Type Sema::valueExpr(Expr& e) {
    Type t = checkExpr(e);
    if (t == Type::VOID) {                         // only a call to a no-return recipe is VOID
        report(e.line, "recipe " + q(e.name) + " returns nothing, so it cannot be used as a value");
        e.type = Type::ERROR;
        return Type::ERROR;
    }
    return t;
}

Type Sema::expectNum(Expr& e, const char* what) {
    Type t = valueExpr(e);
    if (t != Type::NUM && t != Type::ERROR)
        report(e.line, std::string(what) + " must be num, found " + typeName(t));
    return t;
}

Type Sema::checkExpr(Expr& e) {
    switch (e.kind) {
    case ExprKind::NUMBER: e.type = Type::NUM;  break;
    case ExprKind::STRING: e.type = Type::TEXT; break;

    case ExprKind::VAR: {
        Symbol* s = syms_.lookupVar(e.name);
        if (!s) {
            report(e.line, "undeclared variable " + q(e.name));
            e.type = Type::ERROR;
        } else {
            e.sym = s;
            e.type = s->type;
        }
        break;
    }

    case ExprKind::INDEX: {
        expectNum(*e.lhs, "index");
        Symbol* s = syms_.lookupVar(e.name);
        if (!s) {
            report(e.line, "undeclared variable " + q(e.name));
            e.type = Type::ERROR;
        } else {
            e.sym = s;
            if (isRow(s->type)) {
                e.type = elementType(s->type);
            } else {
                if (s->type != Type::ERROR)
                    report(e.line, q(e.name) + " is not a row (it is " + typeName(s->type) + ")");
                e.type = Type::ERROR;
            }
        }
        break;
    }

    case ExprKind::UNARY: {
        Type t = valueExpr(*e.lhs);
        if (t != Type::NUM && t != Type::ERROR)
            report(e.line, std::string("operator '") + opName(e.op) + "' needs num operand, found " + typeName(t));
        e.type = Type::NUM;                        // fixed result type: no cascading errors
        break;
    }

    case ExprKind::BINARY: {
        Type a = valueExpr(*e.lhs);
        Type b = valueExpr(*e.rhs);
        bool known = (a != Type::ERROR && b != Type::ERROR);
        if (e.op == Op::EQ || e.op == Op::NE) {
            bool ok = (a == Type::NUM && b == Type::NUM) || (a == Type::TEXT && b == Type::TEXT);
            if (known && !ok)
                report(e.line, std::string("operator '") + opName(e.op) + "' needs two nums or two texts, found " +
                                   typeName(a) + " and " + typeName(b));
        } else {
            if (known && !(a == Type::NUM && b == Type::NUM))
                report(e.line, std::string("operator '") + opName(e.op) + "' needs num operands, found " +
                                   typeName(a) + " and " + typeName(b));
        }
        e.type = Type::NUM;                        // every binary operator gives a num
        break;
    }

    case ExprKind::CALL: {
        std::vector<Type> at;
        for (Expr* a : e.args) at.push_back(valueExpr(*a));

        if (e.name == "count") {                   // the built-in
            e.isCount = true;
            e.type = Type::NUM;
            if (e.args.size() != 1)
                report(e.line, "count() takes exactly 1 argument but " + std::to_string(e.args.size()) + " were given");
            else if (at[0] != Type::ERROR && !isRow(at[0]))
                report(e.line, std::string("count() needs a row, found ") + typeName(at[0]));
            break;
        }

        Symbol* s = syms_.lookupRecipe(e.name);
        if (!s) {
            report(e.line, "undefined recipe " + q(e.name));
            e.type = Type::ERROR;
            break;
        }
        e.sym = s;
        e.type = s->type;                          // VOID when the recipe has no return type
        if (at.size() != s->paramTypes.size()) {
            report(e.line, "recipe " + q(e.name) + " takes " + std::to_string(s->paramTypes.size()) +
                               " arguments but " + std::to_string(at.size()) + " were given");
        } else {
            for (size_t i = 0; i < at.size(); ++i)
                if (at[i] != Type::ERROR && at[i] != s->paramTypes[i])
                    report(e.line, "argument " + std::to_string(i + 1) + " of " + q(e.name) + " has type " +
                                       typeName(at[i]) + ", expected " + typeName(s->paramTypes[i]));
        }
        break;
    }

    case ExprKind::GRAB:
        e.type = e.asNum ? Type::ROW_NUM : Type::ROW_TEXT;
        if (!rt_selector_valid(e.text.c_str()))
            report(e.line, "selector " + q(e.text) + " is not in the supported subset");
        if (currentRecipe_)
            report(e.line, "grab is not allowed inside a recipe");
        else if (!syms_.pageOpened())
            report(e.line, "grab is not preceded by an open in the same or an enclosing block");
        break;

    case ExprKind::ROW_NEW:
        expectNum(*e.lhs, "row size");
        e.type = e.rowOfText ? Type::ROW_TEXT : Type::ROW_NUM;
        break;
    }
    return e.type;
}
