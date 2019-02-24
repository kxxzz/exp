#include "exp_eval_a.h"



typedef enum EXP_EvalTypeType
{
    EXP_EvalTypeType_Nval,
    EXP_EvalTypeType_Fun,
    EXP_EvalTypeType_Tuple,
    EXP_EvalTypeType_Array,
    EXP_EvalTypeType_Var,

    EXP_NumEvalTypeTypes
} EXP_EvalTypeType;

typedef struct EXP_EvalTypeList
{
    u32 count;
    u32 offset;
} EXP_EvalTypeList;

typedef struct EXP_EvalTypeFun
{
    u32 ins;
    u32 outs;
} EXP_EvalTypeFun;

typedef struct EXP_EvalType
{
    bool hasVar;
    EXP_EvalTypeType type;
    union
    {
        EXP_EvalTypeFun fun;
        EXP_EvalTypeList tuple;
        u32 aryElm;
        u32 varId;
    };
} EXP_EvalType;



typedef struct EXP_EvalTypeContext
{
    Dict* listPool;
    Dict* typePool;
} EXP_EvalTypeContext;







bool EXP_evalTypeMatch(u32 pat, u32 x)
{
    if (EXP_EvalValueType_Any == pat)
    {
        return true;
    }
    return pat == x;
}


bool EXP_evalTypeUnify(u32 a, u32 b, u32* out)
{
    if (EXP_EvalValueType_Any == a)
    {
        *out = b;
        return true;
    }
    if (EXP_EvalValueType_Any == b)
    {
        *out = a;
        return true;
    }
    if (a != b)
    {
        return false;
    }
    *out = a;
    return true;
}





















































































































