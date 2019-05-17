#include "exp_eval_a.h"






u32 EXP_evalArraySize(EXP_EvalArray* a)
{
    return a->size;
}


void EXP_evalArrayResize(EXP_EvalArray* a, u32 size)
{
    if (a->data.length > 0)
    {
        u32 elmSize = a->data.length / a->size;
        u32 dataLen = elmSize * size;
        vec_resize(&a->data, dataLen);
    }
    a->size = size;
}




bool EXP_evalArraySetElm(EXP_EvalArray* a, u32 p, const EXP_EvalValue* inBuf)
{
    if (!a->data.length)
    {
        vec_resize(&a->data, a->size);
    }
    if (p < a->size)
    {
        u32 elmSize = a->data.length / a->size;
        memcpy(a->data.data + elmSize * p, inBuf, sizeof(EXP_EvalValue)*elmSize);
        return true;
    }
    else
    {
        return false;
    }
}


bool EXP_evalArrayGetElm(EXP_EvalArray* a, u32 p, EXP_EvalValue* outBuf)
{
    if (a->data.length)
    {
        if (p < a->size)
        {
            u32 elmSize = a->data.length / a->size;
            memcpy(outBuf, a->data.data + elmSize * p, sizeof(EXP_EvalValue)*elmSize);
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        EXP_EvalValue v = { .f = p, EXP_EvalValueType_AtomVal };
        memcpy(outBuf, &v, sizeof(EXP_EvalValue));
        return true;
    }
}

























































































