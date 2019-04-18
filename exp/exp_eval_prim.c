#include "exp_eval_a.h"




const char** EXP_EvalKeyNameTable(void)
{
    static const char* a[EXP_NumEvalKeys] =
    {
        "def",
        "->",
        ":",
        "if",
        "drop",
        "gc",
    };
    return a;
}






static bool EXP_evalBoolByStr(u32 len, const char* str, EXP_EvalValue* pVal)
{
    if (0 == strncmp(str, "true", len))
    {
        if (pVal) pVal->b = true;
        return true;
    }
    if (0 == strncmp(str, "false", len))
    {
        if (pVal) pVal->b = true;
        return true;
    }
    return false;
}

static bool EXP_evalFloatByStr(u32 len, const char* str, EXP_EvalValue* pVal)
{
    f64 f;
    u32 r = NSTR_str2num(&f, str, len, NULL);
    if (f < 0)
    {
        return false;
    }
    if (len == r)
    {
        if (pVal) pVal->f = f;
    }
    return len == r;
}




static bool EXP_evalStringByStr(u32 len, const char* str, EXP_EvalValue* pVal)
{
    if (pVal)
    {
        vec_char* s = pVal->a;
        vec_pusharr(s, str, len);
        vec_push(s, 0);
    }
    return true;
}

static void EXP_evalStringDtor(void* ptr)
{
    vec_char* s = ptr;
    vec_free(s);
}






const EXP_EvalAtypeInfo* EXP_EvalPrimTypeInfoTable(void)
{
    static const EXP_EvalAtypeInfo a[EXP_NumEvalPrimTypes] =
    {
        { "bool", EXP_evalBoolByStr, true },
        { "float", EXP_evalFloatByStr, true },
        { "string", EXP_evalStringByStr, false, sizeof(vec_char), EXP_evalStringDtor },
    };
    return a;
}













static void EXP_evalAfunCall_Not(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    bool a = ins[0].b;
    outs[0].b = !a;
}



static void EXP_evalAfunCall_FloatAdd(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a + b;
}

static void EXP_evalAfunCall_FloatSub(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a - b;
}

static void EXP_evalAfunCall_FloatMul(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a * b;
}

static void EXP_evalAfunCall_FloatDiv(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a / b;
}



static void EXP_evalAfunCall_FloatNeg(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    outs[0].f = -a;
}




static void EXP_evalAfunCall_FloatEQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a == b;
}

static void EXP_evalAfunCall_FloatINEQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a != b;
}

static void EXP_evalAfunCall_FloatGT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a > b;
}

static void EXP_evalAfunCall_FloatLT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a < b;
}

static void EXP_evalAfunCall_FloatGE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a >= b;
}

static void EXP_evalAfunCall_FloatLE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a <= b;
}





const EXP_EvalAfunInfo* EXP_EvalPrimFunInfoTable(void)
{
    static const EXP_EvalAfunInfo a[EXP_NumEvalPrimFuns] =
    {
        {
            "!",
            EXP_evalAfunCall_Not,
            1, { EXP_EvalPrimType_BOOL },
            1, { EXP_EvalPrimType_BOOL },
        },

        {
            "+",
            EXP_evalAfunCall_FloatAdd,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_FLOAT },
        },
        {
            "-",
            EXP_evalAfunCall_FloatSub,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_FLOAT },
        },
        {
            "*",
            EXP_evalAfunCall_FloatMul,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_FLOAT },
        },
        {
            "/",
            EXP_evalAfunCall_FloatDiv,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_FLOAT },
        },

        {
            "neg",
            EXP_evalAfunCall_FloatNeg,
            1, { EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_FLOAT },
        },

        {
            "=",
            EXP_evalAfunCall_FloatEQ,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_BOOL },
        },
        {
            "!=",
            EXP_evalAfunCall_FloatINEQ,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_BOOL },
        },

        {
            ">",
            EXP_evalAfunCall_FloatGT,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_BOOL },
        },
        {
            "<",
            EXP_evalAfunCall_FloatLT,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_BOOL },
        },
        {
            ">=",
            EXP_evalAfunCall_FloatGE,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_BOOL },
        },
        {
            "<=",
            EXP_evalAfunCall_FloatLE,
            2, { EXP_EvalPrimType_FLOAT, EXP_EvalPrimType_FLOAT },
            1, { EXP_EvalPrimType_BOOL },
        },
    };
    return a;
}



















































































































