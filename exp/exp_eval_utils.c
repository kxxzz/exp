#include "exp_eval_a.h"
#include "exp_eval_type_a.h"









void EXP_evalDataStackFprint(FILE* f, EXP_EvalContext* ctx)
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
                fprintf(f, "%s\n", v.b ? "true" : "false");
                break;
            }
            case EXP_EvalPrimType_FLOAT:
            {
                fprintf(f, "%f\n", v.f);
                break;
            }
            case EXP_EvalPrimType_STRING:
            {
                const char* s = v.s->data;
                fprintf(f, "%s\n", s);
                break;
            }
            default:
            {
                const char* s = EXP_EvalPrimTypeInfoTable()[t].name;
                fprintf(f, "<TYPE: %s>\n", s);
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













void EXP_evalErrorFprint(FILE* f, const EXP_EvalError* err)
{
    switch (err->code)
    {
    case EXP_EvalErrCode_NONE:
    {
        break;
    }
    case EXP_EvalErrCode_SrcFile:
    {
        break;
    }
    case EXP_EvalErrCode_ExpSyntax:
    {
        break;
    }
    case EXP_EvalErrCode_EvalSyntax:
    {
        break;
    }
    case EXP_EvalErrCode_EvalUnable:
    {
        break;
    }
    case EXP_EvalErrCode_EvalUndefined:
    {
        break;
    }
    case EXP_EvalErrCode_EvalArgs:
    {
        break;
    }
    case EXP_EvalErrCode_EvalBranchUneq:
    {
        break;
    }
    case EXP_EvalErrCode_EvalRecurNoBaseCase:
    {
        break;
    }
    case EXP_EvalErrCode_EvalUnification:
    {
        break;
    }
    default:
        assert(false);
        break;
    }
}





































































