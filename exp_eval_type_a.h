#pragma once




typedef struct EXP_EvalTypeVarBinding
{
    u32 id;
    u32 val;
} EXP_EvalTypeVarBinding;

typedef vec_t(EXP_EvalTypeVarBinding) EXP_EvalTypeVarSpace;




u32* EXP_evalTypeVarValue(EXP_EvalTypeVarSpace* space, u32 base, u32 var);
void EXP_evalTypeVarAdd(EXP_EvalTypeVarSpace* space, u32 var, u32 value);


u32 EXP_evalTypeNormForm(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* space, u32 base, u32 x);




bool EXP_evalTypeUnify
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* space, u32 base,
    u32 a, u32 b, u32* u
);


bool EXP_evalTypeUnifyVar1
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* space0, u32 x0,
    EXP_EvalTypeVarSpace* space, u32 base, u32 x
);




























































































