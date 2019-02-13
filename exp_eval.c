#include "exp_eval_a.h"




typedef struct EXP_EvalModInfo
{
    EXP_Node url;
    EXP_Node root;
} EXP_EvalModInfo;

typedef vec_t(EXP_EvalModInfo) EXP_EvalModInfoTable;





typedef struct EXP_EvalVar
{
    EXP_Node key;
    EXP_EvalValue val;
} EXP_EvalVar;

typedef vec_t(EXP_EvalVar) EXP_EvalVarStack;





typedef enum EXP_EvalBlockCallbackType
{
    EXP_EvalBlockCallbackType_NONE,
    EXP_EvalBlockCallbackType_NativeCall,
    EXP_EvalBlockCallbackType_Call,
    EXP_EvalBlockCallbackType_Cond,
} EXP_EvalBlockCallbackType;

typedef struct EXP_EvalBlockCallback
{
    EXP_EvalBlockCallbackType type;
    union
    {
        u32 nativeFun;
        EXP_Node fun;
    };
} EXP_EvalBlockCallback;





typedef struct EXP_EvalCall
{
    EXP_Node srcNode;
    EXP_Node* p;
    EXP_Node* end;
    EXP_EvalBlockCallback cb;
} EXP_EvalCall;

typedef vec_t(EXP_EvalCall) EXP_EvalCallStack;







typedef struct EXP_EvalContext
{
    EXP_Space* space;
    EXP_EvalDataStack* dataStack;
    EXP_EvalValueTypeInfoTable valueTypeTable;
    EXP_EvalNativeFunInfoTable nativeFunTable;
    EXP_NodeSrcInfoTable* srcInfoTable;
    EXP_EvalFunTable funTable;
    EXP_EvalBlockTable blockTable;
    EXP_EvalVarStack varStack;
    EXP_EvalCallStack callStack;
    EXP_EvalValue nativeCallOutBuf[EXP_EvalNativeFunOuts_MAX];
    EXP_EvalError error;
    EXP_NodeVec varKeyBuf;
} EXP_EvalContext;






static EXP_EvalContext EXP_newEvalContext
(
    EXP_Space* space, EXP_EvalDataStack* dataStack, const EXP_EvalNativeEnv* nativeEnv,
    EXP_NodeSrcInfoTable* srcInfoTable
)
{
    EXP_EvalContext _ctx = { 0 };
    EXP_EvalContext* ctx = &_ctx;
    ctx->space = space;
    ctx->dataStack = dataStack;
    ctx->srcInfoTable = srcInfoTable;
    for (u32 i = 0; i < EXP_NumEvalPrimValueTypes; ++i)
    {
        vec_push(&ctx->valueTypeTable, EXP_EvalPrimValueTypeInfoTable[i]);
    }
    for (u32 i = 0; i < EXP_NumEvalPrimFuns; ++i)
    {
        vec_push(&ctx->nativeFunTable, EXP_EvalPrimFunInfoTable[i]);
    }
    if (nativeEnv)
    {
        for (u32 i = 0; i < nativeEnv->numValueTypes; ++i)
        {
            vec_push(&ctx->valueTypeTable, nativeEnv->valueTypes[i]);
        }
        for (u32 i = 0; i < nativeEnv->numFuns; ++i)
        {
            vec_push(&ctx->nativeFunTable, nativeEnv->funs[i]);
        }
    }
    return *ctx;
}


static void EXP_evalContextFree(EXP_EvalContext* ctx)
{
    vec_free(&ctx->varKeyBuf);
    vec_free(&ctx->callStack);
    vec_free(&ctx->varStack);
    vec_free(&ctx->blockTable);
    vec_free(&ctx->funTable);
    vec_free(&ctx->nativeFunTable);
    vec_free(&ctx->valueTypeTable);
}




static u32 EXP_evalGetNativeFun(EXP_EvalContext* ctx, const char* funName)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->nativeFunTable.length; ++i)
    {
        u32 idx = ctx->nativeFunTable.length - 1 - i;
        const char* name = ctx->nativeFunTable.data[idx].name;
        if (0 == strcmp(funName, name))
        {
            return idx;
        }
    }
    return -1;
}







static void EXP_evalDefGetBody(EXP_EvalContext* ctx, EXP_Node node, u32* pLen, EXP_Node** pSeq)
{
    EXP_Space* space = ctx->space;
    assert(EXP_seqLen(space, node) >= 2);
    *pLen = EXP_seqLen(space, node) - 2;
    EXP_Node* defCall = EXP_seqElm(space, node);
    *pSeq = defCall + 2;
}










static EXP_EvalFun* EXP_evalGetMatchedFun(EXP_EvalContext* ctx, const char* name, EXP_Node blk)
{
    EXP_Space* space = ctx->space;
    EXP_EvalFunTable* funTable = &ctx->funTable;
    EXP_EvalBlockTable* blockTable = &ctx->blockTable;
    while (blk.id != EXP_NodeId_Invalid)
    {
        EXP_EvalBlock* blkInfo = blockTable->data + blk.id;
        for (u32 i = 0; i < blkInfo->funsCount; ++i)
        {
            EXP_EvalFun* fun = funTable->data + blkInfo->funsOffset + blkInfo->funsCount - 1 - i;
            const char* str = EXP_tokCstr(space, fun->key);
            if (0 == strcmp(str, name))
            {
                return fun;
            }
        }
        blk = blkInfo->parent;
    }
    return NULL;
}


static EXP_EvalVar* EXP_evalGetMatchedVar(EXP_EvalContext* ctx, const char* name)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->varStack.length; ++i)
    {
        EXP_EvalVar* def = ctx->varStack.data + ctx->varStack.length - 1 - i;
        const char* str = EXP_tokCstr(space, def->key);
        if (0 == strcmp(str, name))
        {
            return def;
        }
    }
    return NULL;
}













static bool EXP_evalEnterBlock(EXP_EvalContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode)
{
    EXP_EvalBlockCallback nocb = { EXP_EvalBlockCallbackType_NONE };
    EXP_EvalCall call = { srcNode, seq, seq + len, nocb };
    vec_push(&ctx->callStack, call);
    return true;
}

static void EXP_evalEnterBlockWithCB
(
    EXP_EvalContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode, EXP_EvalBlockCallback cb
)
{
    assert(cb.type != EXP_EvalBlockCallbackType_NONE);
    EXP_EvalCall call = { srcNode, seq, seq + len, cb };
    vec_push(&ctx->callStack, call);
}

static bool EXP_evalLeaveBlock(EXP_EvalContext* ctx)
{
    EXP_EvalCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalBlock* blkInfo = ctx->blockTable.data + curCall->srcNode.id;
    assert(ctx->varStack.length >= blkInfo->varsCount);
    vec_resize(&ctx->varStack, ctx->varStack.length - blkInfo->varsCount);
    vec_pop(&ctx->callStack);
    return ctx->callStack.length > 0;
}





static bool EXP_evalAtTail(EXP_EvalContext* ctx)
{
    EXP_EvalCall* curCall = &vec_last(&ctx->callStack);
    if (curCall->cb.type != EXP_EvalBlockCallbackType_NONE)
    {
        return false;
    }
    return curCall->p == curCall->end;
}







static void EXP_evalNativeFunCall
(
    EXP_EvalContext* ctx, EXP_EvalNativeFunInfo* nativeFunInfo, EXP_Node srcNode
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalDataStack* dataStack = ctx->dataStack;
    u32 argsOffset = dataStack->length - nativeFunInfo->numIns;
    nativeFunInfo->call(space, dataStack->data + argsOffset, ctx->nativeCallOutBuf);
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < nativeFunInfo->numOuts; ++i)
    {
        EXP_EvalValue v = ctx->nativeCallOutBuf[i];
        vec_push(dataStack, v);
    }
}








static void EXP_evalCall(EXP_EvalContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalCall* curCall;
    EXP_EvalDataStack* dataStack = ctx->dataStack;
next:
    if (ctx->error.code)
    {
        return;
    }
    curCall = &vec_last(&ctx->callStack);
    if (curCall->p == curCall->end)
    {
        EXP_EvalBlockCallback* cb = &curCall->cb;
        switch (cb->type)
        {
        case EXP_EvalBlockCallbackType_NONE:
        {
            break;
        }
        case EXP_EvalBlockCallbackType_NativeCall:
        {
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable.data + cb->nativeFun;
            u32 numIns = nativeFunInfo->numIns;
            EXP_evalNativeFunCall(ctx, nativeFunInfo, curCall->srcNode);
            break;
        }
        case EXP_EvalBlockCallbackType_Call:
        {
            EXP_Node fun = cb->fun;
            u32 bodyLen = 0;
            EXP_Node* body = NULL;
            EXP_evalDefGetBody(ctx, fun, &bodyLen, &body);
            if (!EXP_evalLeaveBlock(ctx))
            {
                return;
            }
            // tail call optimization
            while (EXP_evalAtTail(ctx))
            {
                if (!EXP_evalLeaveBlock(ctx))
                {
                    break;
                }
            }
            if (EXP_evalEnterBlock(ctx, bodyLen, body, fun))
            {
                goto next;
            }
            return;
        }
        case EXP_EvalBlockCallbackType_Cond:
        {
            EXP_EvalValue v = vec_last(dataStack);
            vec_pop(dataStack);
            if (!EXP_evalLeaveBlock(ctx))
            {
                return;
            }
            EXP_Node srcNode = curCall->srcNode;
            if (v.truth)
            {
                if (EXP_evalEnterBlock(ctx, 1, EXP_evalIfBranch0(space, srcNode), srcNode))
                {
                    goto next;
                }
                return;
            }
            else if (EXP_evalIfHasBranch1(space, srcNode))
            {
                if (EXP_evalEnterBlock(ctx, 1, EXP_evalIfBranch1(space, srcNode), srcNode))
                {
                    goto next;
                }
                return;
            }
        }
        default:
            assert(false);
            return;
        }
        if (EXP_evalLeaveBlock(ctx))
        {
            goto next;
        }
        return;
    }
    EXP_Node node = *(curCall->p++);
    if (EXP_isTok(space, node))
    {
        const char* name = EXP_tokCstr(space, node);
        u32 nativeFun = EXP_evalGetNativeFun(ctx, name);
        if (nativeFun != -1)
        {
            switch (nativeFun)
            {
            case EXP_EvalPrimFun_VarDefBegin:
            {
                assert(EXP_EvalBlockCallbackType_NONE == curCall->cb.type);
                ctx->varKeyBuf.length = 0;
                for (u32 n = 0;;)
                {
                    assert(curCall->p < curCall->end);
                    EXP_Node key = *(curCall->p++);
                    const char* skey = EXP_tokCstr(space, key);
                    assert(EXP_isTok(space, key));
                    u32 nativeFun = EXP_evalGetNativeFun(ctx, EXP_tokCstr(space, key));
                    if (nativeFun != -1)
                    {
                        assert(EXP_EvalPrimFun_VarDefEnd == nativeFun);
                        assert(n <= dataStack->length);
                        u32 off = dataStack->length - n;
                        for (u32 i = 0; i < n; ++i)
                        {
                            EXP_EvalValue val = dataStack->data[off + i];
                            EXP_EvalVar def = { ctx->varKeyBuf.data[i], val };
                            vec_push(&ctx->varStack, def);
                        }
                        vec_resize(dataStack, off);
                        ctx->varKeyBuf.length = 0;
                        goto next;
                    }
                    vec_push(&ctx->varKeyBuf, key);
                    ++n;
                }
            }
            case EXP_EvalPrimFun_Drop:
            {
                assert(dataStack->length > 0);
                vec_pop(dataStack);
                goto next;
            }
            default:
                break;
            }
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable.data + nativeFun;
            assert(nativeFunInfo->call);
            assert(dataStack->length >= nativeFunInfo->numIns);
            EXP_evalNativeFunCall(ctx, nativeFunInfo, node);
            goto next;
        }
        EXP_EvalVar* var = EXP_evalGetMatchedVar(ctx, name);
        if (var)
        {
            vec_push(dataStack, var->val);
            goto next;
        }
        EXP_EvalFun* fun = EXP_evalGetMatchedFun(ctx, name, curCall->srcNode);
        if (fun)
        {
            u32 bodyLen = 0;
            EXP_Node* body = NULL;
            EXP_evalDefGetBody(ctx, fun->src, &bodyLen, &body);
            if (EXP_evalEnterBlock(ctx, bodyLen, body, fun->src))
            {
                goto next;
            }
            else
            {
                return;
            }
        }
        else
        {
            bool isQuoted = EXP_tokQuoted(space, node);
            if (!isQuoted)
            {
                for (u32 i = 0; i < ctx->valueTypeTable.length; ++i)
                {
                    u32 j = ctx->valueTypeTable.length - 1 - i;
                    if (ctx->valueTypeTable.data[j].fromStr)
                    {
                        u32 l = EXP_tokSize(space, node);
                        const char* s = EXP_tokCstr(space, node);
                        EXP_EvalValue v = { 0 };
                        if (ctx->valueTypeTable.data[j].fromStr(l, s, &v))
                        {
                            vec_push(dataStack, v);
                            goto next;
                        }
                    }
                }
                assert(false);
                return;
            }
            EXP_EvalValue v = { EXP_EvalPrimValueType_Str, .tok = node };
            vec_push(dataStack, v);
            goto next;
        }
    }
    else
    {
        assert(EXP_evalCheckCall(space, node));
    }
    EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);
    const char* name = EXP_tokCstr(space, elms[0]);
    EXP_EvalVar* var = EXP_evalGetMatchedVar(ctx, name);
    if (var)
    {
        vec_push(dataStack, var->val);
        goto next;
    }
    EXP_EvalFun* fun = EXP_evalGetMatchedFun(ctx, name, curCall->srcNode);
    if (fun)
    {
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Call, .fun = fun->src };
        EXP_evalEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
        goto next;
    }
    u32 nativeFun = EXP_evalGetNativeFun(ctx, name);
    switch (nativeFun)
    {
    case EXP_EvalPrimFun_Def:
    {
        assert(EXP_EvalBlockCallbackType_NONE == curCall->cb.type);
        goto next;
    }
    case EXP_EvalPrimFun_If:
    {
        assert((3 == len) || (4 == len));
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Cond };
        EXP_evalEnterBlockWithCB(ctx, 1, elms + 1, node, cb);
        goto next;
    }
    case EXP_EvalPrimFun_Drop:
    {
        assert(dataStack->length > 0);
        vec_pop(dataStack);
        goto next;
    }
    case EXP_EvalPrimFun_Blk:
    {
        // todo
        goto next;
    }
    default:
    {
        assert(nativeFun != -1);
        EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable.data + nativeFun;
        assert(nativeFunInfo->call);
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_NativeCall,.nativeFun = nativeFun };
        EXP_evalEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
        goto next;
    }
    }
}




















EXP_EvalError EXP_evalVerif
(
    EXP_Space* space, EXP_Node root,
    EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_EvalFunTable* funTable, EXP_EvalBlockTable* blockTable,
    vec_u32* typeStack, EXP_NodeSrcInfoTable* srcInfoTable
);




EXP_EvalError EXP_eval
(
    EXP_Space* space, EXP_EvalDataStack* dataStack, EXP_Node root, const EXP_EvalNativeEnv* nativeEnv,
    vec_u32* typeStack, EXP_NodeSrcInfoTable* srcInfoTable
)
{
    EXP_EvalError error = { 0 };
    if (!EXP_isSeq(space, root))
    {
        return error;
    }
    EXP_EvalContext ctx = EXP_newEvalContext(space, dataStack, nativeEnv, srcInfoTable);
    error = EXP_evalVerif
    (
        space, root, &ctx.valueTypeTable, &ctx.nativeFunTable, &ctx.funTable, &ctx.blockTable, typeStack, srcInfoTable
    );
    if (error.code)
    {
        EXP_evalContextFree(&ctx);
        return error;
    }
    u32 len = EXP_seqLen(space, root);
    EXP_Node* seq = EXP_seqElm(space, root);
    if (!EXP_evalEnterBlock(&ctx, len, seq, root))
    {
        error = ctx.error;
        EXP_evalContextFree(&ctx);
        return error;
    }
    EXP_evalCall(&ctx);
    error = ctx.error;
    EXP_evalContextFree(&ctx);
    return error;
}












EXP_EvalError EXP_evalFile
(
    EXP_Space* space, EXP_EvalDataStack* dataStack, const char* srcFile, const EXP_EvalNativeEnv* nativeEnv,
    vec_u32* typeStack, bool enableSrcInfo
)
{
    EXP_EvalError error = { EXP_EvalErrCode_NONE };
    char* src = NULL;
    u32 srcSize = FILEU_readFile(srcFile, &src);
    if (-1 == srcSize)
    {
        error.code = EXP_EvalErrCode_SrcFile;
        error.file = srcFile;
        return error;
    }
    if (0 == srcSize)
    {
        return error;
    }

    EXP_NodeSrcInfoTable* srcInfoTable = NULL;
    EXP_NodeSrcInfoTable _srcInfoTable = { 0 };
    if (enableSrcInfo)
    {
        srcInfoTable = &_srcInfoTable;
    }
    EXP_Node root = EXP_loadSrcAsList(space, src, srcInfoTable);
    free(src);
    if (EXP_NodeId_Invalid == root.id)
    {
        error.code = EXP_EvalErrCode_ExpSyntax;
        error.file = srcFile;
        if (srcInfoTable)
        {
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 6011)
#endif
            error.line = vec_last(srcInfoTable).line;
            error.column = vec_last(srcInfoTable).column;
#ifdef _MSC_VER
# pragma warning(pop)
#endif
        }
        else
        {
            error.line = -1;
            error.column = -1;
        }
        return error;
    }
    error = EXP_eval(space, dataStack, root, nativeEnv, typeStack, srcInfoTable);
    if (srcInfoTable)
    {
        vec_free(srcInfoTable);
    }
    return error;
}













static bool EXP_evalBoolFromStr(u32 len, const char* str, EXP_EvalValue* pData)
{
    if (0 == strncmp(str, "true", len))
    {
        pData->truth = true;
        return true;
    }
    if (0 == strncmp(str, "false", len))
    {
        pData->truth = true;
        return true;
    }
    return false;
}

static bool EXP_evalNumFromStr(u32 len, const char* str, EXP_EvalValue* pData)
{
    double num;
    u32 r = NSTR_str2num(&num, str, len, NULL);
    if (len == r)
    {
        pData->num = num;
    }
    return len == r;
}

const EXP_EvalValueTypeInfo EXP_EvalPrimValueTypeInfoTable[EXP_NumEvalPrimValueTypes] =
{
    { "bool", EXP_evalBoolFromStr },
    { "num", EXP_evalNumFromStr },
    { "tok" },
};













static void EXP_evalNativeFunCall_Not(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    bool a = ins[0].truth;
    outs[0].truth = !a;
}



static void EXP_evalNativeFunCall_Add(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].num = a + b;
}

static void EXP_evalNativeFunCall_Sub(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].num = a - b;
}

static void EXP_evalNativeFunCall_Mul(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].num = a * b;
}

static void EXP_evalNativeFunCall_Div(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].num = a / b;
}



static void EXP_evalNativeFunCall_Neg(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    outs[0].num = -a;
}




static void EXP_evalNativeFunCall_EQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].truth = a == b;
}

static void EXP_evalNativeFunCall_INEQ(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].truth = a != b;
}

static void EXP_evalNativeFunCall_GT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].truth = a > b;
}

static void EXP_evalNativeFunCall_LT(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].truth = a < b;
}

static void EXP_evalNativeFunCall_GE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].truth = a >= b;
}

static void EXP_evalNativeFunCall_LE(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs)
{
    double a = ins[0].num;
    double b = ins[1].num;
    outs[0].truth = a <= b;
}




const EXP_EvalNativeFunInfo EXP_EvalPrimFunInfoTable[EXP_NumEvalPrimFuns] =
{
    { "def" },
    { "->" },
    { ":" },
    { "if" },
    { "drop" },
    { "blk" },

    {
        "!",
        EXP_evalNativeFunCall_Not,
        1, { EXP_EvalPrimValueType_Bool },
        1, { EXP_EvalPrimValueType_Bool },
    },

    {
        "+",
        EXP_evalNativeFunCall_Add,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Num },
    },
    {
        "-",
        EXP_evalNativeFunCall_Sub,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Num },
    },
    {
        "*",
        EXP_evalNativeFunCall_Mul,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Num },
    },
    {
        "/",
        EXP_evalNativeFunCall_Div,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Num },
    },

    {
        "neg",
        EXP_evalNativeFunCall_Neg,
        1, { EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Num },
    },

    {
        "=",
        EXP_evalNativeFunCall_EQ,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Bool },
    },
    {
        "!=",
        EXP_evalNativeFunCall_INEQ,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Bool },
    },

    {
        ">",
        EXP_evalNativeFunCall_GT,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Bool },
    },
    {
        "<",
        EXP_evalNativeFunCall_LT,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Bool },
    },
    {
        ">=",
        EXP_evalNativeFunCall_GE,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Bool },
    },
    {
        "<=",
        EXP_evalNativeFunCall_LE,
        2, { EXP_EvalPrimValueType_Num, EXP_EvalPrimValueType_Num },
        1, { EXP_EvalPrimValueType_Bool },
    },
};















































































































