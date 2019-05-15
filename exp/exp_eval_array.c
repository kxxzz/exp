#include "exp_eval_a.h"






typedef struct EXP_EvalArray
{
    EXP_EvalValueVec data;
    u32 size;
} EXP_EvalArray;



EXP_EvalArray* EXP_newEvalArray(u32 size)
{
    EXP_EvalArray* a = zalloc(sizeof(*a));
    a->size = size;
    return a;
}

void EXP_evalArrayFree(EXP_EvalArray* a)
{
    vec_free(&a->data);
    free(a);
}



EXP_EvalValue EXP_evalArrayAt(EXP_EvalArray* a, u32 p)
{
    if (a->data.length)
    {
        u32 elmSize = a->data.length / a->size;
        return a->data.data[elmSize*p];
    }
    else
    {
        EXP_EvalValue v = { .u = p, EXP_EvalValueType_AtomVal };
        return v;
    }
}

























































































