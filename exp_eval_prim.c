#include "exp_eval_a.h"





const char* EXP_EvalPrimNameTable[EXP_NumEvalPrims] =
{
    "def",
    "->",
    ":",
    "if",
    "drop",
};






static bool EXP_evalBoolFromSym(u32 len, const char* str, EXP_EvalValue* pVal)
{
    if (0 == strncmp(str, "true", len))
    {
        pVal->b = true;
        return true;
    }
    if (0 == strncmp(str, "false", len))
    {
        pVal->b = true;
        return true;
    }
    return false;
}

static bool EXP_evalFloatFromSym(u32 len, const char* str, EXP_EvalValue* pVal)
{
    f64 f;
    u32 r = NSTR_str2num(&f, str, len, NULL);
    if (f < 0)
    {
        return false;
    }
    if (len == r)
    {
        pVal->f = f;
    }
    return len == r;
}



const EXP_EvalValueTypeInfo EXP_EvalPrimValueTypeInfoTable[EXP_NumEvalPrimValueTypes] =
{
    { "bool", EXP_evalBoolFromSym },
    { "float", EXP_evalFloatFromSym },
    { "string", NULL },
};













static void EXP_evalNativeFunCall_Not(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    bool a = ins[0].b;
    outs[0].b = !a;
}



static void EXP_evalNativeFunCall_FloatAdd(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a + b;
}

static void EXP_evalNativeFunCall_FloatSub(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a - b;
}

static void EXP_evalNativeFunCall_FloatMul(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a * b;
}

static void EXP_evalNativeFunCall_FloatDiv(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].f = a / b;
}



static void EXP_evalNativeFunCall_FloatNeg(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    outs[0].f = -a;
}




static void EXP_evalNativeFunCall_FloatEQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a == b;
}

static void EXP_evalNativeFunCall_FloatINEQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a != b;
}

static void EXP_evalNativeFunCall_FloatGT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a > b;
}

static void EXP_evalNativeFunCall_FloatLT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a < b;
}

static void EXP_evalNativeFunCall_FloatGE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a >= b;
}

static void EXP_evalNativeFunCall_FloatLE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    f64 a = ins[0].f;
    f64 b = ins[1].f;
    outs[0].b = a <= b;
}




const EXP_EvalNativeFunInfo EXP_EvalPrimFunInfoTable[EXP_NumEvalPrimFuns] =
{
    {
        "!",
        EXP_evalNativeFunCall_Not,
        1, { EXP_EvalPrimValueType_BOOL },
        1, { EXP_EvalPrimValueType_BOOL },
    },

    {
        "+",
        EXP_evalNativeFunCall_FloatAdd,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_FLOAT },
    },
    {
        "-",
        EXP_evalNativeFunCall_FloatSub,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_FLOAT },
    },
    {
        "*",
        EXP_evalNativeFunCall_FloatMul,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_FLOAT },
    },
    {
        "/",
        EXP_evalNativeFunCall_FloatDiv,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_FLOAT },
    },

    {
        "neg",
        EXP_evalNativeFunCall_FloatNeg,
        1, { EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_FLOAT },
    },

    {
        "=",
        EXP_evalNativeFunCall_FloatEQ,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_BOOL },
    },
    {
        "!=",
        EXP_evalNativeFunCall_FloatINEQ,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_BOOL },
    },

    {
        ">",
        EXP_evalNativeFunCall_FloatGT,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_BOOL },
    },
    {
        "<",
        EXP_evalNativeFunCall_FloatLT,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_BOOL },
    },
    {
        ">=",
        EXP_evalNativeFunCall_FloatGE,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_BOOL },
    },
    {
        "<=",
        EXP_evalNativeFunCall_FloatLE,
        2, { EXP_EvalPrimValueType_FLOAT, EXP_EvalPrimValueType_FLOAT },
        1, { EXP_EvalPrimValueType_BOOL },
    },
};



















































































































