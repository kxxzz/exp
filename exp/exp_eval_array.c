#include "exp_eval_a.h"






u32 EXP_evalArraySize(EXP_EvalArray* a)
{
    return a->size;
}

u32 EXP_evalArrayElmSize(EXP_EvalArray* a)
{
    assert(a->data.length / a->size == a->elmSize);
    return a->elmSize;
}





void EXP_evalArraySetElmSize(EXP_EvalArray* a, u32 elmSize)
{
    a->elmSize = elmSize;
    u32 dataLen = a->elmSize * a->size;
    vec_resize(&a->data, dataLen);
}

void EXP_evalArrayResize(EXP_EvalArray* a, u32 size)
{
    a->size = size;
    u32 dataLen = a->elmSize * a->size;
    vec_resize(&a->data, dataLen);
}






bool EXP_evalArraySetElm(EXP_EvalArray* a, u32 p, const EXP_EvalValue* inBuf)
{
    if (p < a->size)
    {
        u32 elmSize = a->elmSize;
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
    if (p < a->size)
    {
        u32 elmSize = a->elmSize;
        memcpy(outBuf, a->data.data + elmSize * p, sizeof(EXP_EvalValue)*elmSize);
        return true;
    }
    else
    {
        return false;
    }
}






void EXP_evalArrayPushElm(EXP_EvalArray* a, const EXP_EvalValue* inBuf)
{
    ++a->size;
    u32 elmSize = a->elmSize;
    u32 dataLen = elmSize * a->size;
    vec_resize(&a->data, dataLen);
    memcpy(a->data.data + elmSize * (a->size - 1), inBuf, sizeof(EXP_EvalValue)*elmSize);
}


















































































