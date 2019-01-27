#include "exp_eval_a.h"



//typedef struct EXP_EvalValueTypeDef
//{
//    int x;
//} EXP_EvalValueTypeDef;



typedef struct EXP_EvalVerifValue
{
    u32 type;
    EXP_Node lit;
} EXP_EvalVerifValue;

typedef vec_t(EXP_EvalVerifValue) EXP_EvalVerifDataStack;



typedef struct EXP_EvalVerifDef
{
    EXP_Node key;
    bool isVal;
    union
    {
        EXP_Node fun;
        EXP_EvalVerifValue val;
    };
} EXP_EvalVerifDef;

typedef vec_t(EXP_EvalVerifDef) EXP_EvalVerifDefTable;



typedef enum EXP_EvalBlockInfoState
{
    EXP_EvalBlockInfoState_Uninited = 0,
    EXP_EvalBlockInfoState_Analyzing,
    EXP_EvalBlockInfoState_Inited,
} EXP_EvalBlockInfoState;

typedef struct EXP_EvalBlockInfo
{
    EXP_EvalBlockInfoState state;
    EXP_EvalVerifDefTable defs;
    u32 numIns;
    u32 numOuts;
    vec_u32 typeInOut;
    u32 parent;
} EXP_EvalBlockInfo;

typedef vec_t(EXP_EvalBlockInfo) EXP_EvalBlockInfoTable;

static void EXP_evalBlockInfoFree(EXP_EvalBlockInfo* info)
{
    vec_free(&info->typeInOut);
    vec_free(&info->defs);
}




typedef struct EXP_EvalVerifCall
{
    EXP_Node srcNode;
    u32 dataStackP;
    EXP_Node* seq;
    u32 seqLen;
    u32 p;
    EXP_EvalBlockCallback cb;
} EXP_EvalVerifCall;

typedef vec_t(EXP_EvalVerifCall) EXP_EvalVerifCallStack;





typedef struct EXP_EvalVerifContext
{
    EXP_Space* space;
    EXP_EvalValueTypeInfoTable* valueTypeTable;
    EXP_EvalNativeFunInfoTable* nativeFunTable;
    EXP_NodeSrcInfoTable* srcInfoTable;
    EXP_EvalBlockInfoTable blockTable;
    EXP_EvalVerifDataStack dataStack;
    EXP_EvalVerifCallStack callStack;
    EXP_EvalError error;
} EXP_EvalVerifContext;





static EXP_EvalVerifContext EXP_newEvalVerifContext
(
    EXP_Space* space, EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_NodeSrcInfoTable* srcInfoTable
)
{
    EXP_EvalVerifContext _ctx = { 0 };
    EXP_EvalVerifContext* ctx = &_ctx;
    ctx->space = space;
    ctx->valueTypeTable = valueTypeTable;
    ctx->nativeFunTable = nativeFunTable;
    ctx->srcInfoTable = srcInfoTable;
    vec_resize(&ctx->blockTable, EXP_spaceNodesTotal(space));
    memset(ctx->blockTable.data, 0, sizeof(EXP_EvalBlockInfo)*ctx->blockTable.length);
    return *ctx;
}

static void EXP_evalVerifContextFree(EXP_EvalVerifContext* ctx)
{
    vec_free(&ctx->dataStack);
    for (u32 i = 0; i < ctx->blockTable.length; ++i)
    {
        EXP_EvalBlockInfo* blkInfo = ctx->blockTable.data + i;
        EXP_evalBlockInfoFree(blkInfo);
    }
    vec_free(&ctx->blockTable);
}






static void EXP_evalVerifErrorAtNode(EXP_EvalVerifContext* ctx, EXP_Node node, EXP_EvalErrCode errCode)
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














static bool EXP_evalVerifGetMatched(EXP_EvalVerifContext* ctx, const char* funName, EXP_EvalVerifDef* outDef)
{
    return false;
}

static u32 EXP_evalVerifGetNativeFun(EXP_EvalVerifContext* ctx, const char* funName)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->nativeFunTable->length; ++i)
    {
        u32 idx = ctx->nativeFunTable->length - 1 - i;
        const char* name = ctx->nativeFunTable->data[idx].name;
        if (0 == strcmp(funName, name))
        {
            return idx;
        }
    }
    return -1;
}












static void EXP_evalVerifLoadDef(EXP_EvalVerifContext* ctx, EXP_Node node, EXP_EvalBlockInfo* blkInfo)
{
    EXP_Space* space = ctx->space;
    EXP_EvalBlockInfoTable* blockTable = &ctx->blockTable;
    if (EXP_isTok(space, node))
    {
        return;
    }
    if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }
    EXP_Node* defCall = EXP_seqElm(space, node);
    const char* kDef = EXP_tokCstr(space, defCall[0]);
    u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, kDef);
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
        EXP_evalVerifErrorAtNode(ctx, defCall[1], EXP_EvalErrCode_EvalSyntax);
        return;
    }
    EXP_EvalVerifDef def = { name, false, .fun = node };
    vec_push(&blkInfo->defs, def);
}

















static bool EXP_evalVerifEnterBlock(EXP_EvalVerifContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode)
{
    u32 dataStackP = ctx->dataStack.length;
    EXP_EvalBlockCallback nocb = { EXP_EvalBlockCallbackType_NONE };
    EXP_EvalVerifCall blk = { srcNode, dataStackP, seq, len, 0, nocb };
    vec_push(&ctx->callStack, blk);

    EXP_EvalBlockInfo* blkInfo = ctx->blockTable.data + srcNode.id;
    for (u32 i = 0; i < len; ++i)
    {
        EXP_evalVerifLoadDef(ctx, seq[i], blkInfo);
        if (ctx->error.code)
        {
            return false;
        }
    }
    return true;
}

static void EXP_evalVerifEnterBlockWithCB
(
    EXP_EvalVerifContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode, EXP_EvalBlockCallback cb
)
{
    u32 dataStackP = ctx->dataStack.length;
    assert(cb.type != EXP_EvalBlockCallbackType_NONE);
    EXP_EvalVerifCall blk = { srcNode, dataStackP, seq, len, 0, cb };
    vec_push(&ctx->callStack, blk);
}

static bool EXP_evalVerifLeaveBlock(EXP_EvalVerifContext* ctx)
{
    vec_pop(&ctx->callStack);
    return ctx->callStack.length > 0;
}






static bool EXP_evalVerifValueTypeConvert(EXP_EvalVerifContext* ctx, EXP_EvalVerifValue* v, u32 vt, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    if (v->type != vt)
    {
        EXP_EvalValFromStr fromStr = ctx->valueTypeTable->data[vt].fromStr;
        if (fromStr && (EXP_EvalPrimValueType_Tok == v->type))
        {
            u32 l = EXP_tokSize(space, v->lit);
            const char* s = EXP_tokCstr(space, v->lit);
            EXP_EvalValueData data;
            if (!fromStr(l, s, &data))
            {
                EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                return false;
            }
            v->type = vt;
        }
        else
        {
            EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return false;
        }
    }
    return true;
}



static void EXP_evalVerifNativeFunCall
(
    EXP_EvalVerifContext* ctx, EXP_EvalNativeFunInfo* nativeFunInfo, EXP_Node srcNode
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifDataStack* dataStack = &ctx->dataStack;
    u32 argsOffset = dataStack->length - nativeFunInfo->numIns;
    for (u32 i = 0; i < nativeFunInfo->numIns; ++i)
    {
        EXP_EvalVerifValue* v = dataStack->data + argsOffset + i;
        u32 vt = nativeFunInfo->inType[i];
        if (!EXP_evalVerifValueTypeConvert(ctx, v, vt, srcNode))
        {
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < nativeFunInfo->numOuts; ++i)
    {
        EXP_EvalVerifValue v = { nativeFunInfo->outType[i] };
        vec_push(dataStack, v);
    }
}




static void EXP_evalVerifFunCall
(
    EXP_EvalVerifContext* ctx, EXP_EvalBlockInfo* blkInfo, EXP_Node srcNode
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifDataStack* dataStack = &ctx->dataStack;
    u32 argsOffset = dataStack->length - blkInfo->numIns;
    assert((blkInfo->numIns + blkInfo->numOuts) == blkInfo->typeInOut.length);
    for (u32 i = 0; i < blkInfo->numIns; ++i)
    {
        EXP_EvalVerifValue* v = dataStack->data + argsOffset + i;
        u32 vt = blkInfo->typeInOut.data[i];
        if (!EXP_evalVerifValueTypeConvert(ctx, v, vt, srcNode))
        {
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < blkInfo->numOuts; ++i)
    {
        EXP_EvalVerifValue v = { blkInfo->typeInOut.data[blkInfo->numIns + i] };
        vec_push(dataStack, v);
    }
}




static void EXP_evalVerifCall(EXP_EvalVerifContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifCall* curBlock;
    EXP_EvalBlockInfoTable* blockTable = &ctx->blockTable;
    EXP_EvalVerifDataStack* dataStack = &ctx->dataStack;
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
        case EXP_EvalBlockCallbackType_NativeFun:
        {
            u32 numIns = dataStack->length - curBlock->dataStackP;
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + cb->nativeFun;
            if (numIns != nativeFunInfo->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            EXP_evalVerifNativeFunCall(ctx, nativeFunInfo, curBlock->srcNode);
            break;
        }
        case EXP_EvalBlockCallbackType_Fun:
        {
            if (curBlock->dataStackP > dataStack->length)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            EXP_Node fun = cb->fun;
            EXP_EvalBlockInfo* blkInfo = blockTable->data + fun.id;
            assert(blkInfo->state > EXP_EvalBlockInfoState_Uninited);
            if (EXP_EvalBlockInfoState_Inited == blkInfo->state)
            {
                if (curBlock->dataStackP != (dataStack->length - blkInfo->numIns))
                {
                    EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                if (!EXP_evalVerifLeaveBlock(ctx))
                {
                    return;
                }
                EXP_evalVerifFunCall(ctx, blkInfo, curBlock->srcNode);
                goto next;
            }
            else if (EXP_EvalBlockInfoState_Analyzing == blkInfo->state)
            {
                // todo
                assert(false);
            }
            return;
        }
        case EXP_EvalBlockCallbackType_Branch:
        {
            if (curBlock->dataStackP + 1 != dataStack->length)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            EXP_EvalVerifValue v = dataStack->data[curBlock->dataStackP];
            vec_pop(dataStack);
            if (!EXP_evalVerifValueTypeConvert(ctx, &v, EXP_EvalPrimValueType_Bool, curBlock->srcNode))
            {
                return;
            }
            if (!EXP_evalVerifLeaveBlock(ctx))
            {
                return;
            }
            // todo
            //EXP_evalVerifEnterBlock(ctx, 1, cb->branch[0], curBlock->srcNode);
            //EXP_evalVerifEnterBlock(ctx, 1, cb->branch[1], curBlock->srcNode);
            goto next;
        }
        default:
            assert(false);
            return;
        }
        if (EXP_evalVerifLeaveBlock(ctx))
        {
            goto next;
        }
        return;
    }
    EXP_Node node = curBlock->seq[curBlock->p++];
    if (EXP_isTok(space, node))
    {
        const char* funName = EXP_tokCstr(space, node);
        u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, funName);
        if (nativeFun != -1)
        {
            switch (nativeFun)
            {
            case EXP_EvalPrimFun_PopDefBegin:
            {
                if (curBlock->cb.type |= EXP_EvalBlockCallbackType_NONE)
                {
                    EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                for (;;)
                {
                    EXP_Node key = curBlock->seq[curBlock->p++];
                    const char* skey = EXP_tokCstr(space, key);
                    if (!EXP_isTok(space, key))
                    {
                        EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, EXP_tokCstr(space, key));
                    if (nativeFun != -1)
                    {
                        if (EXP_EvalPrimFun_PopDefEnd == nativeFun)
                        {
                            goto next;
                        }
                        EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    if (!dataStack->length)
                    {
                        EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalStack);
                        return;
                    }
                    EXP_EvalBlockInfo* blkInfo = blockTable->data + curBlock->srcNode.id;
                    EXP_EvalVerifValue val = vec_last(dataStack);
                    EXP_EvalVerifDef def = { key, true, .val = val };
                    vec_push(&blkInfo->defs, def);
                    vec_pop(dataStack);
                }
            }
            case EXP_EvalPrimFun_Drop:
            {
                if (!dataStack->length)
                {
                    EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalStack);
                    return;
                }
                vec_pop(dataStack);
                goto next;
            }
            default:
                break;
            }
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + nativeFun;
            if (!nativeFunInfo->call)
            {
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return;
            }
            if (dataStack->length < nativeFunInfo->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return;
            }
            EXP_evalVerifNativeFunCall(ctx, nativeFunInfo, node);
            goto next;
        }

        EXP_EvalVerifDef def = { 0 };
        if (EXP_evalVerifGetMatched(ctx, funName, &def))
        {
            if (def.isVal)
            {
                vec_push(dataStack, def.val);
                goto next;
            }
            else
            {
                EXP_Node fun = def.fun;
                EXP_EvalBlockInfo* blkInfo = blockTable->data + fun.id;
                assert(blkInfo->state > EXP_EvalBlockInfoState_Uninited);
                if (EXP_EvalBlockInfoState_Inited == blkInfo->state)
                {
                    if (dataStack->length < blkInfo->numIns)
                    {
                        EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    EXP_evalVerifFunCall(ctx, blkInfo, node);
                    goto next;
                }
                else
                {
                    assert(false);
                    return;
                }
            }
        }
        else
        {
            EXP_EvalVerifValue v = { EXP_EvalPrimValueType_Tok, .lit = node };
            vec_push(dataStack, v);
            goto next;
        }
    }
    else if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }
    EXP_Node call = node;
    EXP_Node* elms = EXP_seqElm(space, call);
    u32 len = EXP_seqLen(space, call);
    const char* funName = EXP_tokCstr(space, elms[0]);
    EXP_EvalVerifDef def = { 0 };
    if (EXP_evalVerifGetMatched(ctx, funName, &def))
    {
        if (def.isVal)
        {
            vec_push(dataStack, def.val);
            goto next;
        }
        else
        {
            EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Fun, .fun = def.fun };
            EXP_evalVerifEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
            goto next;
        }
    }
    u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, funName);
    switch (nativeFun)
    {
    case EXP_EvalPrimFun_Def:
    {
        goto next;
    }
    case EXP_EvalPrimFun_If:
    {
        if ((len != 3) && (len != 4))
        {
            EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Branch };
        cb.branch[0] = elms + 2;
        if (3 == len)
        {
            cb.branch[1] = NULL;
        }
        else if (4 == len)
        {
            cb.branch[1] = elms + 3;
        }
        EXP_evalVerifEnterBlockWithCB(ctx, 1, elms + 1, node, cb);
        goto next;
    }
    case EXP_EvalPrimFun_Drop:
    {
        if (!dataStack->length)
        {
            EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalStack);
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
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + nativeFun;
            assert(nativeFunInfo->call);
            EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_NativeFun, .nativeFun = nativeFun };
            EXP_evalVerifEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
            goto next;
        }
        EXP_evalVerifErrorAtNode(ctx, call, EXP_EvalErrCode_EvalSyntax);
        break;
    }
    }
}







EXP_EvalError EXP_evalVerif
(
    EXP_Space* space, EXP_Node root,
    EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_NodeSrcInfoTable* srcInfoTable
)
{
    EXP_EvalError error = { 0 };
    //return error;
    if (!EXP_isSeq(space, root))
    {
        return error;
    }
    EXP_EvalVerifContext ctx = EXP_newEvalVerifContext(space, valueTypeTable, nativeFunTable, srcInfoTable);
    u32 len = EXP_seqLen(space, root);
    EXP_Node* seq = EXP_seqElm(space, root);
    if (!EXP_evalVerifEnterBlock(&ctx, len, seq, root))
    {
        error = ctx.error;
        EXP_evalVerifContextFree(&ctx);
        return error;
    }
    EXP_evalVerifCall(&ctx);
    error = ctx.error;
    EXP_evalVerifContextFree(&ctx);
    return error;
}

















































































































































