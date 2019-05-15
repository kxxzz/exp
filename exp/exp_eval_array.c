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


u32 EXP_evalArraySize(EXP_EvalArray* a)
{
    return a->size;
}


bool EXP_evalArrayGetElmAt(EXP_EvalArray* a, u32 p, EXP_EvalValueVec* outBuf)
{
    if (a->data.length)
    {
        if (p < a->size)
        {
            u32 elmSize = a->data.length / a->size;
            vec_pusharr(outBuf, a->data.data + elmSize * p, elmSize);
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        EXP_EvalValue v = { .u = p, EXP_EvalValueType_AtomVal };
        vec_push(outBuf, v);
        return true;
    }
}


void EXP_evalArrayResize(EXP_EvalArray* a, u32 size)
{
    u32 elmSize = a->data.length / a->size;
    u32 dataLen = elmSize * size;
    vec_resize(&a->data, dataLen);
}






















































































