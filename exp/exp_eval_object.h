#pragma once




typedef struct EXP_EvalObject
{
    bool gcFlag;
    char a[1];
} EXP_EvalObject;




typedef vec_t(EXP_EvalObject*) EXP_EvalObjectPtrVec;
typedef vec_t(EXP_EvalObjectPtrVec) EXP_EvalObjectTable;





EXP_EvalObjectTable EXP_newEvalObjectTable(EXP_EvalAtypeInfoVec* atypeTable);
void EXP_evalObjectTableFree(EXP_EvalObjectTable* objectTable, EXP_EvalAtypeInfoVec* atypeTable);





void EXP_evalSetGcFlag(EXP_EvalContext* ctx, EXP_EvalValue v);
void EXP_evalGC(EXP_EvalContext* ctx);
















































































