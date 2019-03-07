#pragma once





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



























































































