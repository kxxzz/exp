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
    };
    return a;
}






static bool EXP_evalBoolByStr(const char* str, u32 len, EXP_EvalValue* pVal)
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

static bool EXP_evalNumByStr(const char* str, u32 len, EXP_EvalValue* pVal)
{
    u32 r = false;
    APNUM_pool_t pool = NULL;
    APNUM_rat* n = APNUM_ratNew(pool);
    r = APNUM_ratFromStrWithBaseFmt(pool, n, str);
    if (len == r)
    {
        pVal->a = n;
    }
    return len == r;
}




static bool EXP_evalStringByStr(const char* str, u32 len, EXP_EvalValue* pVal)
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

















void EXP_evalAfunCall_Array(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx);
void EXP_evalAfunCall_AtLoad(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx);
void EXP_evalAfunCall_AtSave(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx);
void EXP_evalAfunCall_Size(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx);
void EXP_evalAfunCall_Map(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx);
void EXP_evalAfunCall_Filter(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx);
void EXP_evalAfunCall_Reduce(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx);














static void EXP_evalAfunCall_Not(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    bool a = ins[0].b;
    outs[0].b = !a;
}



static void EXP_evalAfunCall_NumAdd(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    APNUM_rat* c = APNUM_ratNew(ctx->numPool);
    APNUM_ratAdd(ctx->numPool, c, a, b);
    outs[0].a = c;
}

static void EXP_evalAfunCall_NumSub(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    APNUM_rat* c = APNUM_ratNew(ctx->numPool);
    APNUM_ratSub(ctx->numPool, c, a, b);
    outs[0].a = c;
}

static void EXP_evalAfunCall_NumMul(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    APNUM_rat* c = APNUM_ratNew(ctx->numPool);
    APNUM_ratMul(ctx->numPool, c, a, b);
    outs[0].a = c;
}

static void EXP_evalAfunCall_NumDiv(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    APNUM_rat* c = APNUM_ratNew(ctx->numPool);
    APNUM_ratDiv(ctx->numPool, c, a, b);
    outs[0].a = c;
}



static void EXP_evalAfunCall_NumNeg(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = APNUM_ratNew(ctx->numPool);
    APNUM_ratDup(b, a);
    APNUM_ratNeg(b);
    outs[0].a = b;
}




static void EXP_evalAfunCall_NumEQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    outs[0].b = APNUM_ratEq(a, b);
}

static void EXP_evalAfunCall_NumINEQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    outs[0].b = !APNUM_ratEq(a, b);
}

static void EXP_evalAfunCall_NumGT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    outs[0].b = APNUM_ratCmp(ctx->numPool, a, b) > 0;
}

static void EXP_evalAfunCall_NumLT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    outs[0].b = APNUM_ratCmp(ctx->numPool, a, b) < 0;
}

static void EXP_evalAfunCall_NumGE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    outs[0].b = APNUM_ratCmp(ctx->numPool, a, b) >= 0;
}

static void EXP_evalAfunCall_NumLE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    APNUM_rat* a = ins[0].a;
    APNUM_rat* b = ins[1].a;
    outs[0].b = APNUM_ratCmp(ctx->numPool, a, b) <= 0;
}



























const EXP_EvalAfunInfo* EXP_EvalPrimFunInfoTable(void)
{
    static const EXP_EvalAfunInfo a[EXP_NumEvalPrimFuns] =
    {
        {
            "&",
            "num -> [num]",
            EXP_evalAfunCall_Array,
            EXP_EvalAfunMode_ContextDepend,
        },
        {
            "&>",
            "{A*} [A*] num -> A*",
            EXP_evalAfunCall_AtLoad,
            EXP_EvalAfunMode_ContextDepend,
        },
        {
            "&<",
            "{A*} [A*] num A* ->",
            EXP_evalAfunCall_AtSave,
            EXP_EvalAfunMode_ContextDepend,
        },
        {
            "size",
            "{A*} [A*] -> num",
            EXP_evalAfunCall_Size,
            EXP_EvalAfunMode_ContextDepend,
        },

        {
            "map",
            "{A* B*} [A*] (A* -> B*) -> [B*]",
            EXP_evalAfunCall_Map,
            EXP_EvalAfunMode_HighOrder,
        },
        {
            "filter",
            "{A*} [A*] (A* -> bool) -> [A*]",
            EXP_evalAfunCall_Filter,
            EXP_EvalAfunMode_HighOrder,
        },
        {
            "reduce",
            "{A*} [A*] (A* A* -> A*) -> A*",
            EXP_evalAfunCall_Reduce,
            EXP_EvalAfunMode_HighOrder,
        },

        {
            "not",
            "bool -> bool",
            EXP_evalAfunCall_Not,
        },

        {
            "+",
            "num num -> num",
            EXP_evalAfunCall_NumAdd,
        },
        {
            "-",
            "num num -> num",
            EXP_evalAfunCall_NumSub,
        },
        {
            "*",
            "num num -> num",
            EXP_evalAfunCall_NumMul,
        },
        {
            "/",
            "num num -> num",
            EXP_evalAfunCall_NumDiv,
        },

        {
            "neg",
            "num -> num",
            EXP_evalAfunCall_NumNeg,
        },

        {
            "eq",
            "num num -> bool",
            EXP_evalAfunCall_NumEQ,
        },

        {
            "gt",
            "num num -> bool",
            EXP_evalAfunCall_NumGT,
        },
        {
            "lt",
            "num num -> bool",
            EXP_evalAfunCall_NumLT,
        },
        {
            "ge",
            "num num -> bool",
            EXP_evalAfunCall_NumGE,
        },
        {
            "le",
            "num num -> bool",
            EXP_evalAfunCall_NumLE,
        },
    };
    return a;
}



















































































































