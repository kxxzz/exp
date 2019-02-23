#include "exp_eval_a.h"




typedef struct EXP_EvalTypeContext
{
    EXP_Space* space;
} EXP_EvalTypeContext;





bool EXP_evalTypeUnifyX(EXP_EvalTypeContext* ctx, EXP_Node a, EXP_Node b)
{
    EXP_Space* space = ctx->space;
    if (EXP_isTok(space, a))
    {
        if (EXP_isTok(space, b))
        {
            const char* sa = EXP_tokCstr(space, a);
            const char* sb = EXP_tokCstr(space, b);
            if (0 == strcmp(sa, sb))
            {
                return true;
            }
        }
        else
        {
            return EXP_evalTypeUnifyX(ctx, b, a);
        }
    }
    else if (EXP_isSeq(space, b))
    {
        assert(EXP_isSeq(space, a));


    }
    else
    {
        assert(EXP_isTok(space, b));
    }
}






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





















































































































