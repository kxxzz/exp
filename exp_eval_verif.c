#include "exp_eval_a.h"



//typedef struct EXP_EvalValueTypeDef
//{
//    int x;
//} EXP_EvalValueTypeDef;




typedef struct EXP_EvalVerifDef
{
    EXP_Node key;
    bool isVal;
    union
    {
        EXP_Node fun;
        u32 valType;
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
    EXP_Node parent;
    EXP_EvalVerifDefTable defs;
    u32 numIns;
    u32 numOuts;
    vec_u32 typeInOut;
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
    vec_u32 dataStack;
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
    vec_free(&ctx->callStack);
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














static bool EXP_evalVerifGetMatched
(
    EXP_EvalVerifContext* ctx, const char* funName, EXP_Node srcNode, EXP_EvalVerifDef* outDef
)
{
    EXP_Space* space = ctx->space;
    EXP_Node blk = srcNode;
    while (blk.id != EXP_NodeId_Invalid)
    {
        EXP_EvalBlockInfo* blkInfo = ctx->blockTable.data + blk.id;
        for (u32 i = 0; i < blkInfo->defs.length; ++i)
        {
            EXP_EvalVerifDef* def = blkInfo->defs.data + blkInfo->defs.length - 1 - i;
            const char* str = EXP_tokCstr(space, def->key);
            if (0 == strcmp(str, funName))
            {
                *outDef = *def;
                return true;
            }
        }
        blk = blkInfo->parent;
    }
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









static void EXP_evalVerifDefGetBody(EXP_EvalVerifContext* ctx, EXP_Node node, u32* pLen, EXP_Node** pSeq)
{
    EXP_Space* space = ctx->space;
    assert(EXP_seqLen(space, node) >= 2);
    *pLen = EXP_seqLen(space, node) - 2;
    EXP_Node* defCall = EXP_seqElm(space, node);
    *pSeq = defCall + 2;
}











static bool EXP_evalVerifEnterBlock
(
    EXP_EvalVerifContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode, EXP_Node parent, EXP_EvalBlockCallback cb,
    bool isDefScope
)
{
    u32 dataStackP = ctx->dataStack.length;
    EXP_EvalVerifCall blk = { srcNode, dataStackP, seq, len, 0, cb };
    vec_push(&ctx->callStack, blk);

    EXP_EvalBlockInfo* blkInfo = ctx->blockTable.data + srcNode.id;
    assert(EXP_EvalBlockInfoState_Uninited == blkInfo->state);
    blkInfo->state = EXP_EvalBlockInfoState_Analyzing;
    blkInfo->parent = parent;

    if (isDefScope)
    {
        for (u32 i = 0; i < len; ++i)
        {
            EXP_evalVerifLoadDef(ctx, seq[i], blkInfo);
            if (ctx->error.code)
            {
                return false;
            }
        }
    }
    return true;
}

static bool EXP_evalVerifLeaveBlock(EXP_EvalVerifContext* ctx)
{
    EXP_EvalVerifCall* curBlock = &vec_last(&ctx->callStack);
    EXP_EvalBlockInfo* curBlockInfo = ctx->blockTable.data + curBlock->srcNode.id;

    assert(curBlockInfo->typeInOut.length == curBlockInfo->numIns);
    assert(ctx->dataStack.length + curBlockInfo->numIns >= curBlock->dataStackP);
    curBlockInfo->numOuts = ctx->dataStack.length + curBlockInfo->numIns - curBlock->dataStackP;
    for (u32 i = 0; i < curBlockInfo->numOuts; ++i)
    {
        u32 j = ctx->dataStack.length - curBlockInfo->numOuts + i;
        vec_push(&curBlockInfo->typeInOut, ctx->dataStack.data[j]);
    }

    assert(EXP_EvalBlockInfoState_Analyzing == curBlockInfo->state);
    curBlockInfo->state = EXP_EvalBlockInfoState_Inited;
    vec_pop(&ctx->callStack);
    return ctx->callStack.length > 0;
}






static void EXP_evalVerifCurBlockInsUpdate(EXP_EvalVerifContext* ctx, u32 argsOffset, const u32* funInTypes)
{
    EXP_EvalVerifCall* curBlock = &vec_last(&ctx->callStack);
    EXP_EvalBlockInfo* curBlockInfo = ctx->blockTable.data + curBlock->srcNode.id;
    assert(EXP_EvalBlockInfoState_Analyzing == curBlockInfo->state);
    assert(curBlock->dataStackP >= curBlockInfo->numIns);
    if (curBlock->dataStackP > argsOffset + curBlockInfo->numIns)
    {
        u32 n = curBlock->dataStackP - argsOffset;
        assert(n > curBlockInfo->numIns);
        u32 added = n - curBlockInfo->numIns;
        curBlockInfo->numIns = n;
        for (u32 i = 0; i < added; ++i)
        {
            vec_insert(&curBlockInfo->typeInOut, i, funInTypes[i]);
        }
    }
}






static void EXP_evalVerifNativeFunCall(EXP_EvalVerifContext* ctx, EXP_EvalNativeFunInfo* nativeFunInfo, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    vec_u32* dataStack = &ctx->dataStack;

    u32 argsOffset = dataStack->length - nativeFunInfo->numIns;
    EXP_evalVerifCurBlockInsUpdate(ctx, argsOffset, nativeFunInfo->inType);

    for (u32 i = 0; i < nativeFunInfo->numIns; ++i)
    {
        u32 vt1 = dataStack->data[argsOffset + i];
        u32 vt = nativeFunInfo->inType[i];
        if (!EXP_evalTypeMatch(vt, vt1))
        {
            EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < nativeFunInfo->numOuts; ++i)
    {
        vec_push(dataStack, nativeFunInfo->outType[i]);
    }
}




static void EXP_evalVerifFunCall(EXP_EvalVerifContext* ctx, const EXP_EvalBlockInfo* funInfo, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    vec_u32* dataStack = &ctx->dataStack;

    u32 argsOffset = dataStack->length - funInfo->numIns;
    EXP_evalVerifCurBlockInsUpdate(ctx, argsOffset, funInfo->typeInOut.data);

    assert((funInfo->numIns + funInfo->numOuts) == funInfo->typeInOut.length);
    for (u32 i = 0; i < funInfo->numIns; ++i)
    {
        u32 vt1 = dataStack->data[argsOffset + i];
        u32 vt = funInfo->typeInOut.data[i];
        if (!EXP_evalTypeMatch(vt, vt1))
        {
            EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < funInfo->numOuts; ++i)
    {
        u32 vt = funInfo->typeInOut.data[funInfo->numIns + i];
        vec_push(dataStack, vt);
    }
}




static void EXP_evalVerifCall(EXP_EvalVerifContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifCall* curBlock;
    EXP_EvalBlockInfoTable* blockTable = &ctx->blockTable;
    EXP_EvalBlockInfo* curBlockInfo = NULL;
    vec_u32* dataStack = &ctx->dataStack;
next:
    if (ctx->error.code)
    {
        return;
    }
    curBlock = &vec_last(&ctx->callStack);
    curBlockInfo = blockTable->data + curBlock->srcNode.id;
    if (curBlock->p == curBlock->seqLen)
    {
        EXP_EvalBlockCallback* cb = &curBlock->cb;
        switch (cb->type)
        {
        case EXP_EvalBlockCallbackType_NONE:
        {
            if (EXP_evalVerifLeaveBlock(ctx))
            {
                goto next;
            }
            return;
        }
        case EXP_EvalBlockCallbackType_NativeCall:
        {
            u32 numIns = dataStack->length - curBlock->dataStackP;
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + cb->nativeFun;
            if (numIns != nativeFunInfo->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            if (!EXP_evalVerifLeaveBlock(ctx))
            {
                return;
            }
            EXP_evalVerifNativeFunCall(ctx, nativeFunInfo, curBlock->srcNode);
            goto next;
        }
        case EXP_EvalBlockCallbackType_Call:
        {
            EXP_Node fun = cb->fun;
            EXP_EvalBlockInfo* funInfo = blockTable->data + fun.id;
            if (EXP_EvalBlockInfoState_Inited == funInfo->state)
            {
                if (curBlock->dataStackP != (dataStack->length - funInfo->numIns))
                {
                    EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                if (!EXP_evalVerifLeaveBlock(ctx))
                {
                    return;
                }
                EXP_evalVerifFunCall(ctx, funInfo, curBlock->srcNode);
                goto next;
            }
            else if (EXP_EvalBlockInfoState_Uninited == funInfo->state)
            {
                if (curBlock->dataStackP > dataStack->length)
                {
                    EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                u32 bodyLen = 0;
                EXP_Node* body = NULL;
                EXP_evalVerifDefGetBody(ctx, fun, &bodyLen, &body);
                if (!EXP_evalVerifLeaveBlock(ctx))
                {
                    return;
                }
                if (EXP_evalVerifEnterBlock(ctx, bodyLen, body, fun, curBlock->srcNode, EXP_EvalBlockCallback_NONE, true))
                {
                    goto next;
                }
                return;
            }
            else
            {
                assert(false);
            }
            return;
        }
        case EXP_EvalBlockCallbackType_Cond:
        {
            if (curBlock->dataStackP + 1 != dataStack->length)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            u32 vt = dataStack->data[curBlock->dataStackP];
            vec_pop(dataStack);
            if (!EXP_evalTypeMatch(EXP_EvalPrimValueType_Bool, vt))
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            if (!EXP_evalVerifLeaveBlock(ctx))
            {
                return;
            }
            EXP_EvalBlockCallback cb1 = { EXP_EvalBlockCallbackType_Branch, .branch[0] = cb->branch[1] };
            if (EXP_evalVerifEnterBlock
            (
                ctx, 1, cb->branch[0], *cb->branch[0], curBlock->srcNode, cb1, true
            ))
            {
                goto next;
            }
            return;
        }
        case EXP_EvalBlockCallbackType_Branch:
        {
            if (!EXP_evalVerifLeaveBlock(ctx))
            {
                return;
            }
            assert(curBlockInfo->numIns + curBlockInfo->numOuts == curBlockInfo->typeInOut.length);
            if (cb->branch[0])
            {
                for (u32 i = 0; i < curBlockInfo->numOuts; ++i)
                {
                    vec_pop(dataStack);
                }
                for (u32 i = 0; i < curBlockInfo->numIns; ++i)
                {
                    vec_push(dataStack, curBlockInfo->typeInOut.data[i]);
                }
                EXP_EvalBlockCallback cb1 = { EXP_EvalBlockCallbackType_BranchCheck, .lastBranch = curBlock->srcNode };
                if (EXP_evalVerifEnterBlock
                (
                    ctx, 1, cb->branch[0], *cb->branch[0], curBlock->srcNode, cb1, true
                ))
                {
                    goto next;
                }
                return;
            }
            goto next;
        }
        case EXP_EvalBlockCallbackType_BranchCheck:
        {
            if (!EXP_evalVerifLeaveBlock(ctx))
            {
                return;
            }
            assert(curBlockInfo->numIns + curBlockInfo->numOuts == curBlockInfo->typeInOut.length);
            EXP_EvalBlockInfo* lastBlockInfo = blockTable->data + cb->lastBranch.id;
            if (curBlockInfo->numIns != lastBlockInfo->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalBranchIneq);
                return;
            }
            if (curBlockInfo->numOuts != lastBlockInfo->numOuts)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalBranchIneq);
                return;
            }
            assert(curBlockInfo->typeInOut.length == lastBlockInfo->typeInOut.length);
            for (u32 i = 0; i < curBlockInfo->typeInOut.length; ++i)
            {
                if (curBlockInfo->typeInOut.data[i] != lastBlockInfo->typeInOut.data[i])
                {
                    EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalBranchIneq);
                    return;
                }
            }
            goto next;
        }
        default:
            assert(false);
            return;
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
                if (curBlock->cb.type != EXP_EvalBlockCallbackType_NONE)
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
                            if (curBlock->dataStackP > dataStack->length + curBlockInfo->numIns)
                            {
                                u32 n = curBlock->dataStackP - dataStack->length - curBlockInfo->numIns;
                                u32 added = n - curBlockInfo->numIns;
                                curBlockInfo->numIns = n;
                                for (u32 i = 0; i < added; ++i)
                                {
                                    EXP_EvalVerifDef* def = curBlockInfo->defs.data + curBlockInfo->defs.length - added + i;
                                    assert(def->isVal);
                                    vec_insert(&curBlockInfo->typeInOut, i, def->valType);
                                }
                            }
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
                    u32 vt = vec_last(dataStack);
                    EXP_EvalVerifDef def = { key, true, .valType = vt };
                    vec_push(&curBlockInfo->defs, def);
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
                u32 t = vec_last(dataStack);
                vec_pop(dataStack);
                if (curBlock->dataStackP > dataStack->length + curBlockInfo->numIns)
                {
                    u32 n = curBlock->dataStackP - dataStack->length;
                    u32 added = n - curBlockInfo->numIns;
                    assert(1 == added);
                    curBlockInfo->numIns = n;
                    vec_insert(&curBlockInfo->typeInOut, 0, t);
                }
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
        if (EXP_evalVerifGetMatched(ctx, funName, curBlock->srcNode, &def))
        {
            if (def.isVal)
            {
                vec_push(dataStack, def.valType);
                goto next;
            }
            else
            {
                EXP_Node fun = def.fun;
                EXP_EvalBlockInfo* funInfo = blockTable->data + fun.id;
                if (EXP_EvalBlockInfoState_Inited == funInfo->state)
                {
                    if (dataStack->length < funInfo->numIns)
                    {
                        EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    EXP_evalVerifFunCall(ctx, funInfo, node);
                    goto next;
                }
                else if (EXP_EvalBlockInfoState_Uninited == funInfo->state)
                {
                    u32 bodyLen = 0;
                    EXP_Node* body = NULL;
                    EXP_evalVerifDefGetBody(ctx, fun, &bodyLen, &body);
                    if (EXP_evalVerifEnterBlock
                    (
                        ctx, bodyLen, body, fun, curBlock->srcNode, EXP_EvalBlockCallback_NONE, true
                    ))
                    {
                        goto next;
                    }
                    return;
                }
                else
                {
                    // todo
                    assert(false);
                    return;
                }
            }
        }
        else
        {
            bool isStr = EXP_tokQuoted(space, node);
            if (!isStr)
            {
                for (u32 i = 0; i < ctx->valueTypeTable->length; ++i)
                {
                    u32 j = ctx->valueTypeTable->length - 1 - i;
                    if (ctx->valueTypeTable->data[j].fromStr)
                    {
                        u32 l = EXP_tokSize(space, node);
                        const char* s = EXP_tokCstr(space, node);
                        EXP_EvalValueData d = { 0 };
                        if (ctx->valueTypeTable->data[j].fromStr(l, s, &d))
                        {
                            vec_push(dataStack, j);
                            goto next;
                        }
                    }
                }
            }
            vec_push(dataStack, EXP_EvalPrimValueType_Str);
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
    if (EXP_evalVerifGetMatched(ctx, funName, curBlock->srcNode, &def))
    {
        if (def.isVal)
        {
            vec_push(dataStack, def.valType);
            goto next;
        }
        else
        {
            EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Call, .fun = def.fun };
            EXP_evalVerifEnterBlock(ctx, len - 1, elms + 1, node, curBlock->srcNode, cb, false);
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
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Cond };
        cb.branch[0] = elms + 2;
        if (3 == len)
        {
            cb.branch[1] = NULL;
        }
        else if (4 == len)
        {
            cb.branch[1] = elms + 3;
        }
        EXP_evalVerifEnterBlock(ctx, 1, elms + 1, node, curBlock->srcNode, cb, false);
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
            EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_NativeCall, .nativeFun = nativeFun };
            EXP_evalVerifEnterBlock(ctx, len - 1, elms + 1, node, curBlock->srcNode, cb, false);
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
    if (!EXP_evalVerifEnterBlock(&ctx, len, seq, root, EXP_Node_Invalid, EXP_EvalBlockCallback_NONE, true))
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

















































































































































