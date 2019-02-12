#include "exp_eval_a.h"




typedef struct EXP_EvalModInfo
{
    EXP_Node url;
    EXP_Node root;
} EXP_EvalModInfo;

typedef vec_t(EXP_EvalModInfo) EXP_EvalModInfoTable;



typedef struct EXP_EvalDef
{
    EXP_Node key;
    bool isVal;
    union
    {
        EXP_Node fun;
        EXP_EvalValue val;
    };
} EXP_EvalDef;

typedef vec_t(EXP_EvalDef) EXP_EvalDefStack;



typedef vec_t(EXP_EvalDef) EXP_EvalDefTable;

typedef struct EXP_EvalBlock
{
    EXP_Node parent;
    u32 defsCount;
    u32 defsOffset;
} EXP_EvalBlock;

typedef vec_t(EXP_EvalBlock) EXP_EvalBlockTable;





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
    u32 defStackP;
    EXP_Node* seq;
    u32 seqLen;
    u32 p;
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
    EXP_EvalDefStack defStack;
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
    vec_free(&ctx->defStack);
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













static void EXP_evalErrorAtNode(EXP_EvalContext* ctx, EXP_Node node, EXP_EvalErrCode errCode)
{
    ctx->error.code = errCode;
    EXP_NodeSrcInfoTable* srcInfoTable = ctx->srcInfoTable;
    if (srcInfoTable)
    {
        assert(node.id < srcInfoTable->length);
        ctx->error.file = NULL;// todo
        ctx->error.line = srcInfoTable->data[node.id].line;
        ctx->error.column = srcInfoTable->data[node.id].column;
    }
}





static void EXP_evalLoadDef(EXP_EvalContext* ctx, EXP_Node node)
{
    EXP_Space* space = ctx->space;
    if (EXP_isTok(space, node))
    {
        return;
    }
    if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }
    EXP_Node* defCall = EXP_seqElm(space, node);
    const char* kDef = EXP_tokCstr(space, defCall[0]);
    u32 nativeFun = EXP_evalGetNativeFun(ctx, kDef);
    if (nativeFun != EXP_EvalPrimFun_Def)
    {
        return;
    }
    EXP_Node name;
    if (EXP_isTok(space, defCall[1]))
    {
        name = defCall[1];
    }
    else
    {
        EXP_evalErrorAtNode(ctx, defCall[1], EXP_EvalErrCode_EvalSyntax);
        return;
    }
    EXP_EvalDef def = { name, false, .fun = node };
    vec_push(&ctx->defStack, def);
}




static void EXP_evalDefGetBody(EXP_EvalContext* ctx, EXP_Node node, u32* pLen, EXP_Node** pSeq)
{
    EXP_Space* space = ctx->space;
    assert(EXP_seqLen(space, node) >= 2);
    *pLen = EXP_seqLen(space, node) - 2;
    EXP_Node* defCall = EXP_seqElm(space, node);
    *pSeq = defCall + 2;
}











static EXP_EvalDef* EXP_evalGetMatched(EXP_EvalContext* ctx, const char* funName)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->defStack.length; ++i)
    {
        EXP_EvalDef* def = ctx->defStack.data + ctx->defStack.length - 1 - i;
        const char* str = EXP_tokCstr(space, def->key);
        if (0 == strcmp(str, funName))
        {
            return def;
        }
    }
    return NULL;
}













static bool EXP_evalEnterBlock(EXP_EvalContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode)
{
    u32 defStackP = ctx->defStack.length;

    EXP_EvalBlockCallback nocb = { EXP_EvalBlockCallbackType_NONE };
    EXP_EvalCall call = { srcNode, defStackP, seq, len, 0, nocb };
    vec_push(&ctx->callStack, call);

    for (u32 i = 0; i < len; ++i)
    {
        EXP_evalLoadDef(ctx, seq[i]);
        if (ctx->error.code)
        {
            return false;;
        }
    }
    return true;
}

static void EXP_evalEnterBlockWithCB
(
    EXP_EvalContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode, EXP_EvalBlockCallback cb
)
{
    u32 defStackP = ctx->defStack.length;
    assert(cb.type != EXP_EvalBlockCallbackType_NONE);
    EXP_EvalCall call = { srcNode, defStackP, seq, len, 0, cb };
    vec_push(&ctx->callStack, call);
}

static bool EXP_evalLeaveBlock(EXP_EvalContext* ctx)
{
    u32 defStackP = vec_last(&ctx->callStack).defStackP;
    vec_resize(&ctx->defStack, defStackP);
    vec_pop(&ctx->callStack);
    return ctx->callStack.length > 0;
}





static bool EXP_evalCurIsTail(EXP_EvalContext* ctx)
{
    EXP_EvalCall* curBlock = &vec_last(&ctx->callStack);
    if (curBlock->cb.type != EXP_EvalBlockCallbackType_NONE)
    {
        return false;
    }
    return curBlock->p == curBlock->seqLen;
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
    EXP_EvalCall* curBlock;
    EXP_EvalDataStack* dataStack = ctx->dataStack;
next:
    if (ctx->error.code)
    {
        return;
    }
    curBlock = &vec_last(&ctx->callStack);
    if (curBlock->p == curBlock->seqLen)
    {
        EXP_EvalBlockCallback* cb = &curBlock->cb;
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
            EXP_evalNativeFunCall(ctx, nativeFunInfo, curBlock->srcNode);
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
            // tail recursion optimization
            //while (EXP_evalCurIsTail(ctx))
            //{
            //    if (!EXP_evalLeaveBlock(ctx))
            //    {
            //        break;
            //    }
            //}
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
            EXP_Node* elms = EXP_seqElm(space, curBlock->srcNode);
            u32 len = EXP_seqLen(space, curBlock->srcNode);
            assert((3 == len) || (4 == len));
            if (v.truth)
            {
                if (EXP_evalEnterBlock(ctx, 1, elms + 2, curBlock->srcNode))
                {
                    goto next;
                }
                return;
            }
            else if (4 == len)
            {
                if (EXP_evalEnterBlock(ctx, 1, elms + 3, curBlock->srcNode))
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
    EXP_Node node = curBlock->seq[curBlock->p++];
    if (EXP_isTok(space, node))
    {
        const char* funName = EXP_tokCstr(space, node);
        u32 nativeFun = EXP_evalGetNativeFun(ctx, funName);
        if (nativeFun != -1)
        {
            switch (nativeFun)
            {
            case EXP_EvalPrimFun_VarDefBegin:
            {
                if (curBlock->cb.type != EXP_EvalBlockCallbackType_NONE)
                {
                    EXP_evalErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                ctx->varKeyBuf.length = 0;
                for (u32 n = 0;;)
                {
                    EXP_Node key = curBlock->seq[curBlock->p++];
                    const char* skey = EXP_tokCstr(space, key);
                    if (!EXP_isTok(space, key))
                    {
                        EXP_evalErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    u32 nativeFun = EXP_evalGetNativeFun(ctx, EXP_tokCstr(space, key));
                    if (nativeFun != -1)
                    {
                        if (EXP_EvalPrimFun_VarDefEnd == nativeFun)
                        {
                            if (n > dataStack->length)
                            {
                                EXP_evalErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalStack);
                                return;
                            }
                            u32 off = dataStack->length - n;
                            for (u32 i = 0; i < n; ++i)
                            {
                                EXP_EvalValue val = dataStack->data[off + i];
                                EXP_EvalDef def = { ctx->varKeyBuf.data[i], true, .val = val };
                                vec_push(&ctx->defStack, def);
                            }
                            vec_resize(dataStack, off);
                            ctx->varKeyBuf.length = 0;
                            goto next;
                        }
                        EXP_evalErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    vec_push(&ctx->varKeyBuf, key);
                    ++n;
                }
            }
            case EXP_EvalPrimFun_Drop:
            {
                if (!dataStack->length)
                {
                    EXP_evalErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalStack);
                    return;
                }
                vec_pop(dataStack);
                goto next;
            }
            default:
                break;
            }
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable.data + nativeFun;
            if (!nativeFunInfo->call)
            {
                EXP_evalErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return;
            }
            if (dataStack->length < nativeFunInfo->numIns)
            {
                EXP_evalErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return;
            }
            EXP_evalNativeFunCall(ctx, nativeFunInfo, node);
            goto next;
        }
        EXP_EvalDef* def = EXP_evalGetMatched(ctx, funName);
        if (def)
        {
            if (def->isVal)
            {
                vec_push(dataStack, def->val);
                goto next;
            }
            else
            {
                u32 bodyLen = 0;
                EXP_Node* body = NULL;
                EXP_evalDefGetBody(ctx, def->fun, &bodyLen, &body);
                if (EXP_evalEnterBlock(ctx, bodyLen, body, def->fun))
                {
                    goto next;
                }
                else
                {
                    return;
                }
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
                EXP_evalErrorAtNode(ctx, node, EXP_EvalErrCode_EvalUndefined);
                return;
            }
            EXP_EvalValue v = { EXP_EvalPrimValueType_Str, .tok = node };
            vec_push(dataStack, v);
            goto next;
        }
    }
    else if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }
    EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);
    const char* funName = EXP_tokCstr(space, elms[0]);
    EXP_EvalDef* def = EXP_evalGetMatched(ctx, funName);
    if (def)
    {
        if (def->isVal)
        {
            vec_push(dataStack, def->val);
            goto next;
        }
        else
        {
            EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Call, .fun = def->fun };
            EXP_evalEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
            goto next;
        }
    }
    u32 nativeFun = EXP_evalGetNativeFun(ctx, funName);
    switch (nativeFun)
    {
    case EXP_EvalPrimFun_Def:
    {
        if (curBlock->cb.type |= EXP_EvalBlockCallbackType_NONE)
        {
            EXP_evalErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
        goto next;
    }
    case EXP_EvalPrimFun_If:
    {
        if ((len != 3) && (len != 4))
        {
            EXP_evalErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Cond };
        EXP_evalEnterBlockWithCB(ctx, 1, elms + 1, node, cb);
        goto next;
    }
    case EXP_EvalPrimFun_Drop:
    {
        if (!dataStack->length)
        {
            EXP_evalErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalStack);
            return;
        }
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
        if (nativeFun != -1)
        {
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable.data + nativeFun;
            assert(nativeFunInfo->call);
            EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_NativeCall, .nativeFun = nativeFun };
            EXP_evalEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
            goto next;
        }
        EXP_evalErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        break;
    }
    }
}




















EXP_EvalError EXP_evalVerif
(
    EXP_Space* space, EXP_Node root,
    EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
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
    error = EXP_evalVerif(space, root, &ctx.valueTypeTable, &ctx.nativeFunTable, typeStack, srcInfoTable);
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















































































































