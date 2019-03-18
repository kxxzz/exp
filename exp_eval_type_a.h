#pragma once




typedef struct EXP_EvalTypeBvar
{
    u32 id;
    u32 val;
} EXP_EvalTypeBvar;

typedef vec_t(EXP_EvalTypeBvar) EXP_EvalTypeBvarVec;

typedef struct EXP_EvalTypeVarSpace
{
    EXP_EvalTypeBvarVec bvars;
    u32 varCount;
} EXP_EvalTypeVarSpace;


void EXP_evalTypeVarSpaceFree(EXP_EvalTypeVarSpace* varSpace);
void EXP_evalTypeVarSpaceReset(EXP_EvalTypeVarSpace* varSpace);


u32 EXP_evalTypeNewVar(EXP_EvalTypeVarSpace* varSpace);

bool EXP_evalTypeVarIsExist(EXP_EvalTypeVarSpace* varSpace, u32 var);
bool EXP_evalTypeVarIsFree(EXP_EvalTypeVarSpace* varSpace, u32 var);
bool EXP_evalTypeVarIsBound(EXP_EvalTypeVarSpace* varSpace, u32 var);


u32* EXP_evalTypeVarValue(EXP_EvalTypeVarSpace* varSpace, u32 var);
void EXP_evalTypeVarBind(EXP_EvalTypeVarSpace* varSpace, u32 var, u32 value);



u32 EXP_evalTypeNorm(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 x);


bool EXP_evalTypeUnify(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 a, u32 b, u32* pU);


u32 EXP_evalTypeVarRenameNorm
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, EXP_EvalTypeVarSpace* varRenMap, u32 x
);

bool EXP_evalTypeUnifyPat
(
    EXP_EvalTypeContext* ctx,
    EXP_EvalTypeVarSpace* varSpace, u32 x,
    EXP_EvalTypeVarSpace* varRenMap, u32 pat,
    u32* pU
);




























































































