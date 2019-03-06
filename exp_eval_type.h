#pragma once





typedef struct EXP_EvalTypeContext EXP_EvalTypeContext;

EXP_EvalTypeContext* EXP_newEvalTypeContext(void);
void EXP_evalTypeContextFree(EXP_EvalTypeContext* ctx);



u32 EXP_evalTypeNval(EXP_EvalTypeContext* ctx, u32 nativeType);
u32 EXP_evalTypeVar(EXP_EvalTypeContext* ctx, u32 varId);
u32 EXP_evalTypeFun(EXP_EvalTypeContext* ctx, u32 numIns, const u32* ins, u32 numOuts, const u32* outs);
u32 EXP_evalTypeTuple(EXP_EvalTypeContext* ctx, u32 count, const u32* elms);
u32 EXP_evalTypeArray(EXP_EvalTypeContext* ctx, u32 elm);






typedef struct EXP_EvalTypeVarTable EXP_EvalTypeVarTable;

EXP_EvalTypeVarTable* EXP_newEvalTypeVarTable(void);
void EXP_evalTypeVarTableFree(EXP_EvalTypeVarTable* table);



u32* EXP_evalTypeGetVarValue(EXP_EvalTypeVarTable* varTable, u32 varId);
void EXP_evalTypeAddVar(EXP_EvalTypeVarTable* varTable, u32 varId, u32 value);






bool EXP_evalTypeUnifyX(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarTable* varTable, u32 a, u32 b, u32* t);



// todo replace
enum
{
    EXP_EvalValueType_Any = -1,
};

bool EXP_evalTypeUnify(u32 a, u32 b, u32* out);



























































































