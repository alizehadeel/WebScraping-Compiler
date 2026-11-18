// src/codegen_recipes.cpp - LLVM IR for recipes, calls, give and rows
// OWNER: Aleena | PHASE: 2 | STATUS: DONE
//
// Helpers come from codegen.h / codegen.cpp: tmp(), label(), emit(), br(), condBr(),
// ret(), startBlock(), terminated(), varAddr(sym), load(), store(), llType(),
// lineArg(line), strConst(), genExpr(), genBlock(), beginFunction(), endFunction().
//
//   [x] genRecipe     one `define` per recipe; params copied into allocas; default return
//   [x] genCall       arguments left to right, then `call`
//   [x] genGive       `ret <type> <value>` or `ret void`
//   [x] genRowNew     rt_row_new(n, is_text, line)
//   [x] genIndexLoad  row, index, rt_check_index, data pointer (field 2), element load
//   [x] genIndexStore row, index, value, rt_check_index, element store (order from interp.cpp)
//   [x] genCount      load field 0 (len)
//
// Evaluation order follows the comment at the top of src/interp.cpp.
#include "codegen.h"

#include "sema.h"

namespace {

// Parameter values arrive as %arg.<name>; the "arg." prefix keeps them apart from
// the %tN temporaries and %vN.<name> allocas that codegen.cpp creates.
std::string paramValue(const Param &p) { return "%arg." + p.name; }

}  // namespace

void CodeGen::genRecipe(RecipeDef &r) {
    currentRecipe = &r;
    beginFunction();

    std::string header = "define " + llType(r.retType) + " " + recipeName(r) + "(";
    for (size_t i = 0; i < r.params.size(); i++) {
        if (i) header += ", ";
        header += llType(r.params[i].type) + " " + paramValue(r.params[i]);
    }
    header += ")";

    // Every parameter gets an alloca like any other variable (mem2reg removes it),
    // so a parameter can be assigned inside the body.
    for (Param &p : r.params) store(p.type, paramValue(p), varAddr(p.sym));

    genBlock(*r.body);

    // Reaching the end without a give returns the default value (interp.cpp: defaultValue).
    if (!terminated()) {
        switch (r.retType) {
            case Type::NUM: ret("i64 0"); break;
            case Type::TEXT: ret("ptr " + strConst("")); break;
            case Type::ROW_NUM:
            case Type::ROW_TEXT: {
                std::string row = tmp();
                emit(row + " = call ptr @rt_row_new(i64 0, i64 " + (r.retType == Type::ROW_TEXT ? "1" : "0") +
                     ", " + lineArg(r.line) + ")");
                ret("ptr " + row);
                break;
            }
            default: ret("void"); break;
        }
    }

    endFunction(header);
    currentRecipe = nullptr;
}

std::string CodeGen::genCall(Expr &call) {
    RecipeDef *d = call.sym->def;
    std::string args;
    for (size_t i = 0; i < call.args.size(); i++) {  // left to right
        std::string v = genExpr(*call.args[i]);
        if (i) args += ", ";
        args += llType(d->params[i].type) + " " + v;
    }
    if (d->retType == Type::VOID) {
        emit("call void " + recipeName(*d) + "(" + args + ")");
        return "";
    }
    std::string r = tmp();
    emit(r + " = call " + llType(d->retType) + " " + recipeName(*d) + "(" + args + ")");
    return r;
}

void CodeGen::genGive(Stmt &s) {
    if (!currentRecipe) {  // sema rejects give outside a recipe; stay well-formed anyway
        ret("i32 0");
        return;
    }
    if (!s.expr) {
        ret("void");
        return;
    }
    std::string v = genExpr(*s.expr);
    ret(llType(currentRecipe->retType) + " " + v);
}

std::string CodeGen::genRowNew(Expr &e) {
    std::string n = genExpr(*e.lhs);
    std::string r = tmp();
    emit(r + " = call ptr @rt_row_new(i64 " + n + ", i64 " + (e.rowOfText ? "1" : "0") + ", " + lineArg(e.line) +
         ")");
    return r;
}

namespace {

// Address of element i of row `row` (already bounds-checked): data = field 2, then gep.
std::string elementAddr(CodeGen &cg, const std::string &row, const std::string &i, Type elem) {
    std::string dataField = cg.tmp(), data = cg.tmp(), addr = cg.tmp();
    cg.emit(dataField + " = getelementptr %Row, ptr " + row + ", i32 0, i32 2");
    cg.emit(data + " = load ptr, ptr " + dataField);
    cg.emit(addr + " = getelementptr " + CodeGen::llType(elem) + ", ptr " + data + ", i64 " + i);
    return addr;
}

}  // namespace

std::string CodeGen::genIndexLoad(Expr &e) {
    std::string row = load(e.sym->type, varAddr(e.sym));
    std::string i = genExpr(*e.lhs);
    emit("call void @rt_check_index(ptr " + row + ", i64 " + i + ", " + lineArg(e.line) + ")");
    Type elem = elementType(e.sym->type);
    return load(elem, elementAddr(*this, row, i, elem));
}

void CodeGen::genIndexStore(Stmt &s) {
    std::string row = load(s.sym->type, varAddr(s.sym));
    std::string i = genExpr(*s.index);  // index, then value, then the check
    std::string v = genExpr(*s.expr);
    emit("call void @rt_check_index(ptr " + row + ", i64 " + i + ", " + lineArg(s.line) + ")");
    Type elem = elementType(s.sym->type);
    store(elem, v, elementAddr(*this, row, i, elem));
}

std::string CodeGen::genCount(Expr &e) {
    std::string row = genExpr(*e.args[0]);
    std::string lenField = tmp();
    emit(lenField + " = getelementptr %Row, ptr " + row + ", i32 0, i32 0");
    return load(Type::NUM, lenField);
}
