#include "exp_eval_a.h"
#include "exp_eval_type_a.h"









void EXP_evalContextDataStackPrint(EXP_EvalContext* ctx)
{
    vec_u32* typeStack = EXP_evalDataTypeStack(ctx);
    EXP_EvalValueVec* dataStack = EXP_evalDataStack(ctx);
    for (u32 i = 0; i < dataStack->length; ++i)
    {
        EXP_EvalValue v = dataStack->data[i];
        u32 t = typeStack->data[i];
        const EXP_EvalTypeDesc* desc = EXP_evalTypeDescById(EXP_evalDataTypeContext(ctx), t);
        switch (desc->type)
        {
        case EXP_EvalTypeType_Atom:
        {
            switch (desc->atom)
            {
            case EXP_EvalPrimType_BOOL:
            {
                printf("%s\n", v.b ? "true" : "false");
                break;
            }
            case EXP_EvalPrimType_FLOAT:
            {
                printf("%f\n", v.f);
                break;
            }
            case EXP_EvalPrimType_STRING:
            {
                const char* s = v.s->data;
                printf("%s\n", s);
                break;
            }
            default:
            {
                const char* s = EXP_EvalPrimTypeInfoTable()[t].name;
                printf("<TYPE: %s>\n", s);
                break;
            }
            }
            break;
        }
        default:
            assert(false);
            break;
        }
    }
}



















































































