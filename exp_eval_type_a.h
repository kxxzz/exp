#pragma once




typedef struct EXP_EvalTypeVarBinding
{
    u32 id;
    u32 val;
} EXP_EvalTypeVarBinding;

typedef vec_t(EXP_EvalTypeVarBinding) EXP_EvalTypeVarTable;




u32* EXP_evalTypeVarTableGet(EXP_EvalTypeVarTable* vtable, u32 vtableBase, u32 var);
void EXP_evalTypeVarTableAdd(EXP_EvalTypeVarTable* vtable, u32 var, u32 value);


u32 EXP_evalTypeNormForm(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarTable* vtable, u32 vtableBase, u32 x);




bool EXP_evalTypeUnify
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarTable* vtable, u32 vtableBase,
    u32 a, u32 b, u32* u
);


bool EXP_evalTypeMatch(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarTable* patVtable, u32 pat, u32 nf);




























































































