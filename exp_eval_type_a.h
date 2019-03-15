#pragma once




typedef struct EXP_EvalTypeVarBinding
{
    u32 id;
    u32 val;
} EXP_EvalTypeVarBinding;

typedef vec_t(EXP_EvalTypeVarBinding) EXP_EvalTypeVarSpace;




u32* EXP_evalTypeVarValueGet(EXP_EvalTypeVarSpace* varSpace, u32 var);
void EXP_evalTypeVarValueSet(EXP_EvalTypeVarSpace* varSpace, u32 var, u32 value);


u32 EXP_evalTypeNorm(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 x);
u32 EXP_evalTypeToVarS1(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 x);


bool EXP_evalTypeUnify(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 a, u32 b, u32* pU);


bool EXP_evalTypeUnifyVarS1
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 x, EXP_EvalTypeVarSpace* varSpace1, u32 x1
);




























































































