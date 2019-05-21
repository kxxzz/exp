#include "exp_eval_a.h"
#include "exp_eval_type_a.h"


#include "exp_eval_utils.h"
#include <time.h>





void EXP_evalValueFprint(FILE* f, EXP_EvalContext* ctx, EXP_EvalValue v, u32 t, u32 indent)
{
    EXP_EvalTypeContext* typeContext = EXP_evalDataTypeContext(ctx);
    const EXP_EvalTypeDesc* desc = EXP_evalTypeDescById(typeContext, t);
    for (u32 i = 0; i < indent; ++i)
    {
        fprintf(f, "    ");
    }
    switch (desc->type)
    {
    case EXP_EvalTypeType_Atom:
    {
        switch (desc->atype)
        {
        case EXP_EvalPrimType_BOOL:
        {
            fprintf(f, "%s", v.b ? "true" : "false");
            break;
        }
        case EXP_EvalPrimType_NUM:
        {
            fprintf(f, "%f", v.f);
            break;
        }
        case EXP_EvalPrimType_STRING:
        {
            const char* s = ((vec_char*)(v.a))->data;
            fprintf(f, "\"%s\"", s);
            break;
        }
        default:
        {
            const char* s = EXP_EvalPrimTypeInfoTable()[t].name;
            fprintf(f, "<TYPE: %s>", s);
            break;
        }
        }
        break;
    }
    case EXP_EvalTypeType_Array:
    {
        assert(EXP_EvalValueType_Object == v.type);
        const EXP_EvalTypeDesc* elmDesc = EXP_evalTypeDescById(typeContext, desc->ary.elm);
        assert(EXP_EvalTypeType_List == elmDesc->type);
        //fprintf(f, "<ARRAY>\n");
        bool hasAryMemb = false;
        for (u32 i = 0; i < elmDesc->list.count; ++i)
        {
            u32 t = elmDesc->list.elms[i];
            const EXP_EvalTypeDesc* membDesc = EXP_evalTypeDescById(typeContext, t);
            if (EXP_EvalTypeType_Array == membDesc->type)
            {
                hasAryMemb = true;
            }
        }
        if (hasAryMemb)
        {
            fprintf(f, "[\n");
        }
        else
        {
            fprintf(f, "[ ");
        }
        EXP_EvalArray* ary = v.ary;
        u32 size = EXP_evalArraySize(ary);
        u32 elmSize = EXP_evalArrayElmSize(ary);
        assert(elmDesc->list.count == elmSize);
        for (u32 i = 0; i < size; ++i)
        {
            EXP_EvalValue elmBuf[64];
            bool r = EXP_evalArrayGetElm(ary, i, elmBuf);
            assert(r);
            for (u32 i = 0; i < elmSize; ++i)
            {
                EXP_EvalValue v = elmBuf[i];
                u32 t = elmDesc->list.elms[i];
                EXP_evalValueFprint(f, ctx, v, t, hasAryMemb ? indent + 1 : 0);
                if (hasAryMemb)
                {
                    fprintf(f, "\n");
                }
                else
                {
                    fprintf(f, " ");
                }
            }
        }
        fprintf(f, "]");
        break;
    }
    default:
        assert(false);
        break;
    }
}





void EXP_evalDataStackFprint(FILE* f, EXP_EvalContext* ctx)
{
    vec_u32* typeStack = EXP_evalDataTypeStack(ctx);
    EXP_EvalValueVec* dataStack = EXP_evalDataStack(ctx);
    for (u32 i = 0; i < dataStack->length; ++i)
    {
        EXP_EvalValue v = dataStack->data[i];
        u32 t = typeStack->data[i];
        EXP_evalValueFprint(f, ctx, v, t, 0);
        fprintf(f, "\n");
    }
}








char* EXP_evalGetNowStr(char* timeBuf)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    timeBuf[strftime(timeBuf, EXP_EvalTimeStrBuf_MAX, "%H:%M:%S", lt)] = '\0';
    return timeBuf;
}







void EXP_evalErrorFprint(FILE* f, const EXP_EvalFileInfoTable* fiTable, const EXP_EvalError* err)
{
    const char* name = EXP_EvalErrCodeNameTable()[err->code];
    const char* filename = fiTable->data[err->file].name;
    char timeBuf[EXP_EvalTimeStrBuf_MAX];
    fprintf(f, "[ERROR] %s: \"%s\": %u:%u [%s]\n", name, filename, err->line, err->column, EXP_evalGetNowStr(timeBuf));
}





































































