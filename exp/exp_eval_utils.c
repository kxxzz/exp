#include "exp_eval_a.h"
#include "exp_eval_type_a.h"


#include "exp_eval_utils.h"
#include <time.h>





typedef struct EXP_EvalValueFprintArrayLevel
{
    EXP_EvalValue v;
    u32 arySize;
    u32 elmSize;
    const u32* elmTypes;
    bool hasAryMemb;
    bool multiLine;
    u32 indent;
    u32 elmBufBase;

    u32 aryIdx;
    u32 elmIdx;
} EXP_EvalValueFprintArrayLevel;

typedef vec_t(EXP_EvalValueFprintArrayLevel) EXP_EvalValueFprintStack;




typedef struct EXP_EvalValueFprintContext
{
    FILE* f;
    EXP_EvalContext* evalContext;
    EXP_EvalValueVec elmBuf;
    EXP_EvalValueFprintStack stack;
} EXP_EvalValueFprintContext;

static EXP_EvalValueFprintContext EXP_newEvalValueFprintContext(FILE* f, EXP_EvalContext* evalContext)
{
    EXP_EvalValueFprintContext a = { f, evalContext };
    return a;
}
static void EXP_evalValueFprintContextFree(EXP_EvalValueFprintContext* ctx)
{
    vec_free(&ctx->stack);
    vec_free(&ctx->elmBuf);
}





static bool EXP_evalValueFprintNonArray(EXP_EvalValueFprintContext* ctx, EXP_EvalValue v, u32 t)
{
    FILE* f = ctx->f;
    EXP_EvalContext* evalContext = ctx->evalContext;
    EXP_EvalTypeContext* typeContext = EXP_evalDataTypeContext(evalContext);
    const EXP_EvalTypeDesc* desc = EXP_evalTypeDescById(typeContext, t);
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
            APNUM_rat* a = v.a;
            char buf[1024];
            u32 n = APNUM_ratToStrWithBaseFmt(evalContext->numPool, a, APNUM_int_StrBaseFmtType_DEC, buf, sizeof(buf));
            buf[n] = 0;
            fprintf(f, "%s", buf);
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
        return false;
    }
    default:
        assert(false);
        break;
    }
    return true;
}






static void EXP_evalValueFprintPushArrayLevel
(
    EXP_EvalValueFprintContext* ctx,
    EXP_EvalValue v, u32 arySize, u32 elmSize, const u32* elmTypes,
    bool hasAryMemb, u32 indent
)
{
    FILE* f = ctx->f;
    EXP_EvalContext* evalContext = ctx->evalContext;
    EXP_EvalTypeContext* typeContext = EXP_evalDataTypeContext(evalContext);
    EXP_EvalValueVec* elmBuf = &ctx->elmBuf;
    EXP_EvalValueFprintStack* stack = &ctx->stack;

    u32 elmBufBase = elmBuf->length;
    vec_resize(elmBuf, elmBufBase + elmSize);

    bool multiLine = hasAryMemb || (elmSize > 1);
    if (multiLine)
    {
        fprintf(f, "[\n");
    }
    else
    {
        fprintf(f, "[ ");
    }
    EXP_EvalValueFprintArrayLevel newLevel =
    {
        v, arySize, elmSize, elmTypes, hasAryMemb, multiLine, indent, elmBufBase
    };
    vec_push(stack, newLevel);
}





static void EXP_evalValueFprintPushArrayLevelByVT(EXP_EvalValueFprintContext* ctx, EXP_EvalValue v, u32 t, u32 indent)
{
    EXP_EvalContext* evalContext = ctx->evalContext;
    EXP_EvalTypeContext* typeContext = EXP_evalDataTypeContext(evalContext);

    const EXP_EvalTypeDesc* desc = EXP_evalTypeDescById(typeContext, t);
    assert(EXP_EvalTypeType_Array == desc->type);

    assert(EXP_EvalValueType_Object == v.type);
    const EXP_EvalTypeDesc* elmDesc = EXP_evalTypeDescById(typeContext, desc->ary.elm);
    assert(EXP_EvalTypeType_List == elmDesc->type);

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

    EXP_EvalArray* ary = v.ary;
    u32 arySize = EXP_evalArraySize(ary);
    u32 elmSize = EXP_evalArrayElmSize(ary);
    assert(elmDesc->list.count == elmSize);
    const u32* elmTypes = elmDesc->list.elms;

    EXP_evalValueFprintPushArrayLevel(ctx, v, arySize, elmSize, elmTypes, hasAryMemb, indent);
}





static void EXP_evalValueFprint(EXP_EvalValueFprintContext* ctx, EXP_EvalValue v, u32 t, u32 indent)
{
    FILE* f = ctx->f;
    EXP_EvalContext* evalContext = ctx->evalContext;
    EXP_EvalTypeContext* typeContext = EXP_evalDataTypeContext(evalContext);
    EXP_EvalValueVec* elmBuf = &ctx->elmBuf;
    EXP_EvalValueFprintStack* stack = &ctx->stack;

    for (u32 i = 0; i < indent; ++i)
    {
        fprintf(f, "  ");
    }
    if (!EXP_evalValueFprintNonArray(ctx, v, t))
    {
        EXP_evalValueFprintPushArrayLevelByVT(ctx, v, t, indent);
    }

    EXP_EvalValueFprintArrayLevel* top = NULL;
    u32 aryIdx = 0;
    u32 elmIdx = 0;

next:
    if (!stack->length)
    {
        return;
    }
    top = &vec_last(stack);
    aryIdx = top->aryIdx;
    elmIdx = top->elmIdx++;

    if (aryIdx == top->arySize)
    {
        vec_resize(elmBuf, top->elmBufBase);
        fprintf(f, "]");
        vec_pop(stack);
        goto next;
    }
    if (0 == elmIdx)
    {
        bool r = EXP_evalArrayGetElm(top->v.ary, aryIdx, elmBuf->data + top->elmBufBase);
        assert(r);
    }
    else
    {
        if (top->multiLine && ((elmIdx == top->elmSize) || top->hasAryMemb))
        {
            fprintf(f, "\n");
        }
        else
        {
            fprintf(f, " ");
        }
        if (elmIdx == top->elmSize)
        {
            if (top->hasAryMemb)
            {
                fprintf(f, "\n");
            }

            ++top->aryIdx;
            top->elmIdx = 0;
            goto next;
        }
    }
    v = elmBuf->data[top->elmBufBase + elmIdx];
    t = top->elmTypes[elmIdx];
    indent = (top->multiLine && ((0 == elmIdx) || top->hasAryMemb)) ? top->indent + 1 : 0;
    for (u32 i = 0; i < indent; ++i)
    {
        fprintf(f, "  ");
    }
    if (!EXP_evalValueFprintNonArray(ctx, v, t))
    {
        EXP_evalValueFprintPushArrayLevelByVT(ctx, v, t, indent);
    }
    goto next;
}







void EXP_evalDataStackFprint(FILE* f, EXP_EvalContext* evalContext)
{
    vec_u32* typeStack = EXP_evalDataTypeStack(evalContext);
    EXP_EvalValueVec* dataStack = EXP_evalDataStack(evalContext);
    EXP_EvalValueFprintContext ctx = EXP_newEvalValueFprintContext(f, evalContext);
    for (u32 i = 0; i < dataStack->length; ++i)
    {
        EXP_EvalValue v = dataStack->data[i];
        u32 t = typeStack->data[i];
        EXP_evalValueFprint(&ctx, v, t, 0);
        fprintf(f, "\n");
    }
    EXP_evalValueFprintContextFree(&ctx);
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





































































