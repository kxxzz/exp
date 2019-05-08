#include "exp_eval_a.h"




const char** EXP_EvalKeyNameTable(void)
{
    static const char* a[EXP_NumEvalKeys] =
    {
        "#",
        "=>",
        ":",
        "if",
        "gc",
        "!",
        "@",
        "each",
        "foldl",
        "foldr",
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

static bool EXP_evalNumByStr(u32 len, const char* str, EXP_EvalValue* pVal)
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
        { "num", EXP_evalNumByStr, true },
        { "string", EXP_evalStringByStr, false, sizeof(vec_char), EXP_evalStringDtor },
    };
    return a;
}













static void EXP_evalAfunCall_Not(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    bool a = ins[0].b;
    outs[0].b = !a;
}



static void EXP_evalAfunCall_NumAdd(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a + b;
}

static void EXP_evalAfunCall_NumSub(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a - b;
}

static void EXP_evalAfunCall_NumMul(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a * b;
}

static void EXP_evalAfunCall_NumDiv(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a / b;
}



static void EXP_evalAfunCall_NumNeg(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    outs[0].f = -a;
}




static void EXP_evalAfunCall_NumEQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a == b;
}

static void EXP_evalAfunCall_NumINEQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a != b;
}

static void EXP_evalAfunCall_NumGT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a > b;
}

static void EXP_evalAfunCall_NumLT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a < b;
}

static void EXP_evalAfunCall_NumGE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a >= b;
}

static void EXP_evalAfunCall_NumLE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
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
            "not",
            EXP_evalAfunCall_Not,
            1, 1,
            "bool -> bool",
        },

        {
            "+",
            EXP_evalAfunCall_NumAdd,
            2, 1,
            "num num -> num",
        },
        {
            "-",
            EXP_evalAfunCall_NumSub,
            2, 1,
            "num num -> num",
        },
        {
            "*",
            EXP_evalAfunCall_NumMul,
            2, 1,
            "num num -> num",
        },
        {
            "/",
            EXP_evalAfunCall_NumDiv,
            2, 1,
            "num num -> num",
        },

        {
            "neg",
            EXP_evalAfunCall_NumNeg,
            1, 1,
            "num -> num",
        },

        {
            "eq",
            EXP_evalAfunCall_NumEQ,
            2, 1,
            "num num -> bool",
        },

        {
            "gt",
            EXP_evalAfunCall_NumGT,
            2, 1,
            "num num -> bool",
        },
        {
            "lt",
            EXP_evalAfunCall_NumLT,
            2, 1,
            "num num -> bool",
        },
        {
            "ge",
            EXP_evalAfunCall_NumGE,
            2, 1,
            "num num -> bool",
        },
        {
            "le",
            EXP_evalAfunCall_NumLE,
            2, 1,
            "num num -> bool",
        },
    };
    return a;
}



















































































































