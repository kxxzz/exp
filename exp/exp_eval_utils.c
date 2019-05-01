#include "exp_eval_a.h"
#include "exp_eval_type_a.h"


#include "exp_eval_utils.h"
#include <time.h>







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
            switch (desc->atype)
            {
            case EXP_EvalPrimType_BOOL:
            {
                fprintf(f, "%s\n", v.b ? "true" : "false");
                break;
            }
            case EXP_EvalPrimType_NUM:
            {
                fprintf(f, "%f\n", v.f);
                break;
            }
            case EXP_EvalPrimType_STRING:
            {
                const char* s = ((vec_char*)(v.a))->data;
                fprintf(f, "\"%s\"\n", s);
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





































































