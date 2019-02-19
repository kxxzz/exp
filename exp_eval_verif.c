#include "exp_eval_a.h"



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



typedef enum EXP_EvalVerifBlockTypeInferState
{
    EXP_EvalVerifBlockTypeInferState_None = 0,
    EXP_EvalVerifBlockTypeInferState_Entered,
    EXP_EvalVerifBlockTypeInferState_Done,
} EXP_EvalVerifBlockTypeInferState;

typedef struct EXP_EvalVerifBlock
{
    EXP_Node parent;
    EXP_EvalVerifDefTable defs;

    EXP_EvalVerifBlockTypeInferState typeInferState;
    u32 numIns;
    u32 numOuts;
    vec_u32 typeInOut;
} EXP_EvalVerifBlock;

typedef vec_t(EXP_EvalVerifBlock) EXP_EvalVerifBlockTable;

static void EXP_evalVerifBlockFree(EXP_EvalVerifBlock* info)
{
    vec_free(&info->typeInOut);
    vec_free(&info->defs);
}

static void EXP_evalVerifBlockReset(EXP_EvalVerifBlock* info)
{
    info->parent = EXP_Node_Invalid;
    info->defs.length = 0;

    info->typeInferState = EXP_EvalVerifBlockTypeInferState_None;
    info->numIns = 0;
    info->numOuts = 0;
    info->typeInOut.length = 0;
}





typedef enum EXP_EvalVerifBlockCallbackType
{
    EXP_EvalVerifBlockCallbackType_NONE,
    EXP_EvalVerifBlockCallbackType_NativeCall,
    EXP_EvalVerifBlockCallbackType_Call,
    EXP_EvalVerifBlockCallbackType_Cond,
    EXP_EvalVerifBlockCallbackType_Branch0,
    EXP_EvalVerifBlockCallbackType_Branch1,
    EXP_EvalVerifBlockCallbackType_BranchUnify,
} EXP_EvalVerifBlockCallbackType;

typedef struct EXP_EvalVerifBlockCallback
{
    EXP_EvalVerifBlockCallbackType type;
    union
    {
        u32 nativeFun;
        EXP_Node fun;
    };
} EXP_EvalVerifBlockCallback;

static EXP_EvalVerifBlockCallback EXP_EvalBlockCallback_NONE = { EXP_EvalVerifBlockCallbackType_NONE };





typedef struct EXP_EvalVerifCall
{
    EXP_Node srcNode;
    u32 dataStackP;
    EXP_Node* p;
    EXP_Node* end;
    EXP_EvalVerifBlockCallback cb;
} EXP_EvalVerifCall;

typedef vec_t(EXP_EvalVerifCall) EXP_EvalVerifCallStack;





typedef struct EXP_EvalVerifContext
{
    EXP_Space* space;
    EXP_EvalValueTypeInfoTable* valueTypeTable;
    EXP_EvalNativeFunInfoTable* nativeFunTable;
    EXP_SpaceSrcInfo* srcInfo;
    EXP_EvalVerifBlockTable blockTable;
    vec_u32 dataStack;
    bool dataStackShiftEnable;
    EXP_EvalVerifCallStack callStack;
    EXP_NodeVec recheckNodes;
    bool recheckFlag;
    EXP_EvalError error;
    EXP_NodeVec varKeyBuf;
} EXP_EvalVerifContext;





static EXP_EvalVerifContext EXP_newEvalVerifContext
(
    EXP_Space* space, EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_SpaceSrcInfo* srcInfo
)
{
    EXP_EvalVerifContext _ctx = { 0 };
    EXP_EvalVerifContext* ctx = &_ctx;
    ctx->space = space;
    ctx->valueTypeTable = valueTypeTable;
    ctx->nativeFunTable = nativeFunTable;
    ctx->srcInfo = srcInfo;
    vec_resize(&ctx->blockTable, EXP_spaceNodesTotal(space));
    memset(ctx->blockTable.data, 0, sizeof(EXP_EvalVerifBlock)*ctx->blockTable.length);
    return *ctx;
}

static void EXP_evalVerifContextFree(EXP_EvalVerifContext* ctx)
{
    vec_free(&ctx->varKeyBuf);
    vec_free(&ctx->recheckNodes);
    vec_free(&ctx->callStack);
    vec_free(&ctx->dataStack);
    for (u32 i = 0; i < ctx->blockTable.length; ++i)
    {
        EXP_EvalVerifBlock* b = ctx->blockTable.data + i;
        EXP_evalVerifBlockFree(b);
    }
    vec_free(&ctx->blockTable);
}






static void EXP_evalVerifErrorAtNode(EXP_EvalVerifContext* ctx, EXP_Node node, EXP_EvalErrCode errCode)
{
    ctx->error.code = errCode;
    EXP_SpaceSrcInfo* srcInfo = ctx->srcInfo;
    if (srcInfo)
    {
        assert(node.id < srcInfo->nodes.length);
        ctx->error.file = srcInfo->nodes.data[node.id].file;
        ctx->error.line = srcInfo->nodes.data[node.id].line;
        ctx->error.column = srcInfo->nodes.data[node.id].column;
    }
}














static bool EXP_evalVerifGetMatched
(
    EXP_EvalVerifContext* ctx, const char* name, EXP_Node blk, EXP_EvalVerifDef* outDef
)
{
    EXP_Space* space = ctx->space;
    while (blk.id != EXP_NodeId_Invalid)
    {
        EXP_EvalVerifBlock* blkInfo = ctx->blockTable.data + blk.id;
        for (u32 i = 0; i < blkInfo->defs.length; ++i)
        {
            EXP_EvalVerifDef* def = blkInfo->defs.data + blkInfo->defs.length - 1 - i;
            const char* str = EXP_tokCstr(space, def->key);
            if (0 == strcmp(str, name))
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












static void EXP_evalVerifLoadDef(EXP_EvalVerifContext* ctx, EXP_Node node, EXP_EvalVerifBlock* blkInfo)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifBlockTable* blockTable = &ctx->blockTable;
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
    EXP_EvalVerifContext* ctx, EXP_Node* seq, u32 len, EXP_Node srcNode, EXP_Node parent, EXP_EvalVerifBlockCallback cb,
    bool isDefScope
)
{
    u32 dataStackP = ctx->dataStack.length;
    EXP_EvalVerifCall call = { srcNode, dataStackP, seq, seq + len, cb };
    vec_push(&ctx->callStack, call);

    EXP_EvalVerifBlock* blkInfo = ctx->blockTable.data + srcNode.id;
    if (ctx->recheckFlag && (blkInfo->typeInferState != EXP_EvalVerifBlockTypeInferState_None))
    {
        assert(blkInfo->parent.id == parent.id);
        EXP_evalVerifBlockReset(blkInfo);
    }
    assert(EXP_EvalVerifBlockTypeInferState_None == blkInfo->typeInferState);
    blkInfo->typeInferState = EXP_EvalVerifBlockTypeInferState_Entered;
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



static void EXP_evalVerifLeaveBlock(EXP_EvalVerifContext* ctx)
{
    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = ctx->blockTable.data + curCall->srcNode.id;

    if (ctx->recheckFlag && (1 == ctx->callStack.length))
    {
        assert(EXP_EvalVerifBlockTypeInferState_Done == curBlock->typeInferState);
        vec_pop(&ctx->callStack);
        return;
    }
    assert(curBlock->typeInOut.length == curBlock->numIns);
    assert(ctx->dataStack.length + curBlock->numIns >= curCall->dataStackP);
    curBlock->numOuts = ctx->dataStack.length + curBlock->numIns - curCall->dataStackP;
    for (u32 i = 0; i < curBlock->numOuts; ++i)
    {
        u32 j = ctx->dataStack.length - curBlock->numOuts + i;
        vec_push(&curBlock->typeInOut, ctx->dataStack.data[j]);
    }

    assert(EXP_EvalVerifBlockTypeInferState_Entered == curBlock->typeInferState);
    curBlock->typeInferState = EXP_EvalVerifBlockTypeInferState_Done;

    vec_pop(&ctx->callStack);
}



static void EXP_evalVerifBlockSaveInfo(EXP_EvalVerifContext* ctx, EXP_EvalVerifBlock* nodeInfo)
{
    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = ctx->blockTable.data + curCall->srcNode.id;
    assert(curBlock->typeInOut.length == curBlock->numIns);
    assert(ctx->dataStack.length + curBlock->numIns >= curCall->dataStackP);

    if (EXP_EvalVerifBlockTypeInferState_Done == nodeInfo->typeInferState)
    {
        return;
    }
    nodeInfo->numIns = curBlock->numIns;
    for (u32 i = 0; i < nodeInfo->numIns; ++i)
    {
        vec_push(&nodeInfo->typeInOut, nodeInfo->typeInOut.data[i]);
    }
    nodeInfo->numOuts = ctx->dataStack.length + curBlock->numIns - curCall->dataStackP;
    for (u32 i = 0; i < nodeInfo->numOuts; ++i)
    {
        u32 j = ctx->dataStack.length - nodeInfo->numOuts + i;
        vec_push(&nodeInfo->typeInOut, ctx->dataStack.data[j]);
    }
    nodeInfo->typeInferState = EXP_EvalVerifBlockTypeInferState_Done;
}



static void EXP_evalVerifBlockRebase(EXP_EvalVerifContext* ctx)
{
    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = ctx->blockTable.data + curCall->srcNode.id;
    assert(curBlock->typeInOut.length == curBlock->numIns);
    assert(0 == curBlock->numOuts);

    assert(curCall->dataStackP >= curBlock->numIns);
    ctx->dataStack.length = curCall->dataStackP - curBlock->numIns;
    for (u32 i = 0; i < curBlock->numIns; ++i)
    {
        vec_push(&ctx->dataStack, curBlock->typeInOut.data[i]);
    }
}

static void EXP_evalVerifCancelBlock(EXP_EvalVerifContext* ctx)
{
    EXP_evalVerifBlockRebase(ctx);

    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = ctx->blockTable.data + curCall->srcNode.id;
    assert(EXP_EvalVerifBlockTypeInferState_Entered == curBlock->typeInferState);
    EXP_evalVerifBlockReset(curBlock);

    vec_pop(&ctx->callStack);
}






static bool EXP_evalVerifShiftDataStack(EXP_EvalVerifContext* ctx, u32 n, const u32* a)
{
    if (!ctx->dataStackShiftEnable)
    {
        return false;
    }
    vec_u32* dataStack = &ctx->dataStack;
    vec_insertarr(dataStack, 0, a, n);
    for (u32 i = 0; i < ctx->callStack.length; ++i)
    {
        EXP_EvalVerifCall* c = ctx->callStack.data + i;
        c->dataStackP += n;
    }
    return true;
}









static void EXP_evalVerifCurBlockInsUpdate(EXP_EvalVerifContext* ctx, u32 argsOffset, const u32* funInTypes)
{
    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = ctx->blockTable.data + curCall->srcNode.id;
    assert(EXP_EvalVerifBlockTypeInferState_Entered == curBlock->typeInferState);
    assert(curCall->dataStackP >= curBlock->numIns);
    if (curCall->dataStackP > argsOffset + curBlock->numIns)
    {
        u32 n = curCall->dataStackP - argsOffset;
        assert(n > curBlock->numIns);
        u32 added = n - curBlock->numIns;
        curBlock->numIns = n;
        for (u32 i = 0; i < added; ++i)
        {
            vec_insert(&curBlock->typeInOut, i, funInTypes[i]);
        }
    }
}






static void EXP_evalVerifNativeFunCall(EXP_EvalVerifContext* ctx, EXP_EvalNativeFunInfo* nativeFunInfo, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    vec_u32* dataStack = &ctx->dataStack;

    assert(dataStack->length >= nativeFunInfo->numIns);
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




static void EXP_evalVerifFunCall(EXP_EvalVerifContext* ctx, const EXP_EvalVerifBlock* funInfo, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    vec_u32* dataStack = &ctx->dataStack;

    assert(dataStack->length >= funInfo->numIns);
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










static void EXP_evalVerifAddRecheck(EXP_EvalVerifContext* ctx, EXP_Node node)
{
    vec_push(&ctx->recheckNodes, node);
}


static bool EXP_evalVerifRecurFun
(
    EXP_EvalVerifContext* ctx, EXP_EvalVerifCall* curCall, const EXP_EvalVerifBlock* funInfo
)
{
    assert(EXP_EvalVerifBlockTypeInferState_Entered == funInfo->typeInferState);
    assert(!ctx->recheckFlag);
    EXP_Space* space = ctx->space;

    EXP_Node lastSrcNode = EXP_Node_Invalid;
    while (ctx->callStack.length > 0)
    {
        lastSrcNode = curCall->srcNode;
        curCall = &vec_last(&ctx->callStack);
        EXP_Node srcNode = curCall->srcNode;
        EXP_EvalVerifBlockCallback* cb = &curCall->cb;
        if (EXP_EvalVerifBlockCallbackType_Branch0 == cb->type)
        {
            if (EXP_evalIfHasBranch1(space, srcNode))
            {
                EXP_evalVerifBlockRebase(ctx);
                curCall->p = EXP_evalIfBranch1(space, srcNode);
                curCall->end = EXP_evalIfBranch1(space, srcNode) + 1;
                cb->type = EXP_EvalVerifBlockCallbackType_NONE;
                EXP_evalVerifAddRecheck(ctx, srcNode);
                return true;
            }
        }
        else if (EXP_EvalVerifBlockCallbackType_BranchUnify == cb->type)
        {
            EXP_evalVerifBlockRebase(ctx);
            curCall->p = EXP_evalIfBranch0(space, srcNode);
            curCall->end = EXP_evalIfBranch0(space, srcNode) + 1;
            cb->type = EXP_EvalVerifBlockCallbackType_NONE;
            EXP_evalVerifAddRecheck(ctx, srcNode);
            return true;
        }
        EXP_evalVerifCancelBlock(ctx);
    }
    EXP_evalVerifErrorAtNode(ctx, lastSrcNode, EXP_EvalErrCode_EvalRecurNoBaseCase);
    return false;
}






static bool EXP_evalVerifNode
(
    EXP_EvalVerifContext* ctx, EXP_Node node, EXP_EvalVerifCall* curCall, EXP_EvalVerifBlock* curBlock
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifBlockTable* blockTable = &ctx->blockTable;
    vec_u32* dataStack = &ctx->dataStack;

    if (EXP_isTok(space, node))
    {
        const char* funName = EXP_tokCstr(space, node);
        u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, funName);
        if (nativeFun != -1)
        {
            switch (nativeFun)
            {
            case EXP_EvalPrimFun_VarDefBegin:
            {
                if (curCall->cb.type != EXP_EvalVerifBlockCallbackType_NONE)
                {
                    EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                    return false;
                }
                ctx->varKeyBuf.length = 0;
                for (u32 n = 0;;)
                {
                    if (curCall->p == curCall->end)
                    {
                        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
                        return false;
                    }
                    node = *(curCall->p++);
                    const char* skey = EXP_tokCstr(space, node);
                    if (!EXP_isTok(space, node))
                    {
                        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                        return false;
                    }
                    u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, EXP_tokCstr(space, node));
                    if (nativeFun != -1)
                    {
                        if (EXP_EvalPrimFun_VarDefEnd == nativeFun)
                        {
                            if (n > dataStack->length)
                            {
                                for (u32 i = 0; i < n - dataStack->length; ++i)
                                {
                                    u32 a[] = { EXP_EvalValueType_Any };
                                    if (!EXP_evalVerifShiftDataStack(ctx, 1, a))
                                    {
                                        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalStack);
                                        return false;
                                    }
                                }
                            }

                            u32 off = dataStack->length - n;
                            for (u32 i = 0; i < n; ++i)
                            {
                                u32 vt = dataStack->data[off + i];
                                EXP_EvalVerifDef def = { ctx->varKeyBuf.data[i], true, .valType = vt };
                                vec_push(&curBlock->defs, def);
                            }
                            assert(n <= dataStack->length);
                            vec_resize(dataStack, off);
                            ctx->varKeyBuf.length = 0;

                            if (curCall->dataStackP > dataStack->length + curBlock->numIns)
                            {
                                u32 n = curCall->dataStackP - dataStack->length - curBlock->numIns;
                                u32 added = n - curBlock->numIns;
                                curBlock->numIns = n;
                                for (u32 i = 0; i < added; ++i)
                                {
                                    EXP_EvalVerifDef* def = curBlock->defs.data + curBlock->defs.length - added + i;
                                    assert(def->isVal);
                                    vec_insert(&curBlock->typeInOut, i, def->valType);
                                }
                            }
                            return true;
                        }
                        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                        return false;
                    }
                    vec_push(&ctx->varKeyBuf, node);
                    ++n;
                }
            }
            case EXP_EvalPrimFun_Drop:
            {
                if (!dataStack->length)
                {
                    u32 a[] = { EXP_EvalValueType_Any };
                    if (!EXP_evalVerifShiftDataStack(ctx, 1, a))
                    {
                        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalStack);
                        return false;
                    }
                }
                u32 t = vec_last(dataStack);
                vec_pop(dataStack);
                if (curCall->dataStackP > dataStack->length + curBlock->numIns)
                {
                    u32 n = curCall->dataStackP - dataStack->length;
                    u32 added = n - curBlock->numIns;
                    assert(1 == added);
                    curBlock->numIns = n;
                    vec_insert(&curBlock->typeInOut, 0, t);
                }
                return true;
            }
            default:
                break;
            }
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + nativeFun;
            if (!nativeFunInfo->call)
            {
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return false;
            }
            if (dataStack->length < nativeFunInfo->numIns)
            {
                u32 n = nativeFunInfo->numIns - dataStack->length;
                if (!EXP_evalVerifShiftDataStack(ctx, n, nativeFunInfo->inType))
                {
                    EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalStack);
                    return false;
                }
            }
            EXP_evalVerifNativeFunCall(ctx, nativeFunInfo, node);
            return true;
        }

        EXP_EvalVerifDef def = { 0 };
        if (EXP_evalVerifGetMatched(ctx, funName, curCall->srcNode, &def))
        {
            if (def.isVal)
            {
                vec_push(dataStack, def.valType);
                return true;
            }
            else
            {
                EXP_Node fun = def.fun;
                EXP_EvalVerifBlock* funInfo = blockTable->data + fun.id;
                if (EXP_EvalVerifBlockTypeInferState_Done == funInfo->typeInferState)
                {
                    if (dataStack->length < funInfo->numIns)
                    {
                        u32 n = funInfo->numIns - dataStack->length;
                        if (!EXP_evalVerifShiftDataStack(ctx, n, funInfo->typeInOut.data))
                        {
                            EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalStack);
                            return false;
                        }
                    }
                    EXP_evalVerifFunCall(ctx, funInfo, node);
                    return true;
                }
                else if (EXP_EvalVerifBlockTypeInferState_None == funInfo->typeInferState)
                {
                    u32 bodyLen = 0;
                    EXP_Node* body = NULL;
                    EXP_evalVerifDefGetBody(ctx, fun, &bodyLen, &body);
                    if (!EXP_evalVerifEnterBlock
                    (
                        ctx, body, bodyLen, fun, curCall->srcNode, EXP_EvalBlockCallback_NONE, true
                    ))
                    {
                        return false;
                    }
                    return true;
                }
                else
                {
                    if (!EXP_evalVerifRecurFun(ctx, curCall, funInfo))
                    {
                        return false;
                    }
                    return true;
                }
            }
        }
        else
        {
            bool isQuoted = EXP_tokQuoted(space, node);
            if (!isQuoted)
            {
                for (u32 i = 0; i < ctx->valueTypeTable->length; ++i)
                {
                    u32 j = ctx->valueTypeTable->length - 1 - i;
                    if (ctx->valueTypeTable->data[j].ctorBySym)
                    {
                        u32 l = EXP_tokSize(space, node);
                        const char* s = EXP_tokCstr(space, node);
                        EXP_EvalValue v = { 0 };
                        if (ctx->valueTypeTable->data[j].ctorBySym(l, s, &v))
                        {
                            vec_push(dataStack, j);
                            return true;
                        }
                    }
                }
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalUndefined);
                return false;
            }
            vec_push(dataStack, EXP_EvalPrimValueType_Str);
            return true;
        }
    }
    else if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return false;
    }
    EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);
    const char* funName = EXP_tokCstr(space, elms[0]);
    EXP_EvalVerifDef def = { 0 };
    if (EXP_evalVerifGetMatched(ctx, funName, curCall->srcNode, &def))
    {
        if (def.isVal)
        {
            vec_push(dataStack, def.valType);
            return true;
        }
        else
        {
            EXP_EvalVerifBlockCallback cb = { EXP_EvalVerifBlockCallbackType_Call, .fun = def.fun };
            EXP_evalVerifEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
            return true;
        }
    }
    u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, funName);
    switch (nativeFun)
    {
    case EXP_EvalPrimFun_Def:
    {
        return true;
    }
    case EXP_EvalPrimFun_If:
    {
        if ((len != 3) && (len != 4))
        {
            EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
            return false;
        }
        EXP_EvalVerifBlockCallback cb = { EXP_EvalVerifBlockCallbackType_Cond };
        EXP_evalVerifEnterBlock(ctx, elms + 1, 1, node, curCall->srcNode, cb, false);
        return true;
    }
    case EXP_EvalPrimFun_Blk:
    {
        // todo
        return true;
    }
    default:
    {
        if (nativeFun != -1)
        {
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + nativeFun;
            assert(nativeFunInfo->call);
            EXP_EvalVerifBlockCallback cb = { EXP_EvalVerifBlockCallbackType_NativeCall, .nativeFun = nativeFun };
            EXP_evalVerifEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
            return true;
        }
        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return false;
    }
    }
}






static void EXP_evalVerifCall(EXP_EvalVerifContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifCall* curCall;
    EXP_EvalVerifBlockTable* blockTable = &ctx->blockTable;
    EXP_EvalVerifBlock* curBlock = NULL;
    vec_u32* dataStack = &ctx->dataStack;
next:
    if (ctx->error.code)
    {
        return;
    }
    if (0 == ctx->callStack.length)
    {
        return;
    }
    curCall = &vec_last(&ctx->callStack);
    curBlock = blockTable->data + curCall->srcNode.id;
    if (curCall->p == curCall->end)
    {
        EXP_EvalVerifBlockCallback* cb = &curCall->cb;
        switch (cb->type)
        {
        case EXP_EvalVerifBlockCallbackType_NONE:
        {
            EXP_evalVerifLeaveBlock(ctx);
            goto next;
        }
        case EXP_EvalVerifBlockCallbackType_NativeCall:
        {
            if (curBlock->numIns > 0)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            if (dataStack->length < curCall->dataStackP)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            u32 numIns = dataStack->length - curCall->dataStackP;
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + cb->nativeFun;
            if (numIns != nativeFunInfo->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            EXP_evalVerifNativeFunCall(ctx, nativeFunInfo, curCall->srcNode);
            EXP_evalVerifLeaveBlock(ctx);
            goto next;
        }
        case EXP_EvalVerifBlockCallbackType_Call:
        {
            EXP_Node srcNode = curCall->srcNode;
            EXP_Node fun = cb->fun;
            EXP_EvalVerifBlock* funInfo = blockTable->data + fun.id;
            if (EXP_EvalVerifBlockTypeInferState_Done == funInfo->typeInferState)
            {
                if (curBlock->numIns > 0)
                {
                    EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                if (curCall->dataStackP != (dataStack->length - funInfo->numIns))
                {
                    EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                EXP_evalVerifFunCall(ctx, funInfo, srcNode);
                EXP_evalVerifLeaveBlock(ctx);
                goto next;
            }
            else if (EXP_EvalVerifBlockTypeInferState_None == funInfo->typeInferState)
            {
                if (curCall->dataStackP > dataStack->length)
                {
                    EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                u32 bodyLen = 0;
                EXP_Node* body = NULL;
                EXP_evalVerifDefGetBody(ctx, fun, &bodyLen, &body);
                curCall->cb.type = EXP_EvalVerifBlockCallbackType_NONE;
                if (!EXP_evalVerifEnterBlock(ctx, body, bodyLen, fun, srcNode, EXP_EvalBlockCallback_NONE, true))
                {
                    return;
                }
                goto next;
            }
            else
            {
                if (!EXP_evalVerifRecurFun(ctx, curCall, funInfo))
                {
                    return;
                }
                goto next;
            }
            return;
        }
        case EXP_EvalVerifBlockCallbackType_Cond:
        {
            EXP_Node srcNode = curCall->srcNode;
            if (curCall->dataStackP + 1 != dataStack->length)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            u32 vt = dataStack->data[curCall->dataStackP];
            vec_pop(dataStack);
            if (!EXP_evalTypeMatch(EXP_EvalPrimValueType_Bool, vt))
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            curCall->p = EXP_evalIfBranch0(space, srcNode);
            curCall->end = EXP_evalIfBranch0(space, srcNode) + 1;
            cb->type = EXP_EvalVerifBlockCallbackType_Branch0;
            goto next;
        }
        case EXP_EvalVerifBlockCallbackType_Branch0:
        {
            EXP_Node srcNode = curCall->srcNode;
            EXP_EvalVerifBlock* b0 = blockTable->data + EXP_evalIfBranch0(space, srcNode)->id;
            EXP_evalVerifBlockSaveInfo(ctx, b0);
            assert(EXP_EvalVerifBlockTypeInferState_Done == b0->typeInferState);
            assert(b0->numIns + b0->numOuts == b0->typeInOut.length);
            if (EXP_evalIfHasBranch1(space, srcNode))
            {
                if (dataStack->length < b0->numOuts)
                {
                    u32 n = b0->numOuts - dataStack->length;
                    if (!EXP_evalVerifShiftDataStack(ctx, n, b0->typeInOut.data))
                    {
                        EXP_evalVerifErrorAtNode(ctx, *EXP_evalIfBranch0(space, srcNode), EXP_EvalErrCode_EvalStack);
                        return;
                    }
                }
                for (u32 i = 0; i < b0->numOuts; ++i)
                {
                    vec_pop(dataStack);
                }
                for (u32 i = 0; i < b0->numIns; ++i)
                {
                    vec_push(dataStack, b0->typeInOut.data[i]);
                }
                curCall->p = EXP_evalIfBranch1(space, srcNode);
                curCall->end = EXP_evalIfBranch1(space, srcNode) + 1;
                cb->type = EXP_EvalVerifBlockCallbackType_BranchUnify;
                goto next;
            }
            EXP_evalVerifLeaveBlock(ctx);
            goto next;
        }
        case EXP_EvalVerifBlockCallbackType_BranchUnify:
        {
            EXP_Node srcNode = curCall->srcNode;
            assert(EXP_evalIfHasBranch1(space, srcNode));
            EXP_EvalVerifBlock* b0 = blockTable->data + EXP_evalIfBranch0(space, srcNode)->id;
            EXP_EvalVerifBlock* b1 = blockTable->data + EXP_evalIfBranch1(space, srcNode)->id;
            EXP_evalVerifBlockSaveInfo(ctx, b1);
            assert(EXP_EvalVerifBlockTypeInferState_Done == b0->typeInferState);
            assert(EXP_EvalVerifBlockTypeInferState_Done == b1->typeInferState);
            assert(b1->numIns + b1->numOuts == b1->typeInOut.length);

            EXP_evalVerifLeaveBlock(ctx);
            assert(b1->numIns == curBlock->numIns);
            assert(b1->numOuts == curBlock->numOuts);
            assert(b1->typeInOut.length == curBlock->typeInOut.length);

            if (b0->numIns != b1->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalBranchUneq);
                return;
            }
            if (b0->numOuts != b1->numOuts)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalBranchUneq);
                return;
            }
            assert(b0->typeInOut.length == b1->typeInOut.length);
            for (u32 i = 0; i < b0->typeInOut.length; ++i)
            {
                u32 t;
                bool m = EXP_evalTypeUnify(b0->typeInOut.data[i], b1->typeInOut.data[i], &t);
                if (!m)
                {
                    EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalBranchUneq);
                    return;
                }
                assert(b1->typeInOut.data[i] == curBlock->typeInOut.data[i]);
                b0->typeInOut.data[i] = t;
                b1->typeInOut.data[i] = t;
                curBlock->typeInOut.data[i] = t;
            }
            goto next;
        }
        default:
            assert(false);
            return;
        }
        return;
    }
    EXP_Node node = *(curCall->p++);
    if (EXP_evalVerifNode(ctx, node, curCall, curBlock))
    {
        goto next;
    }
    return;
}








static void EXP_evalVerifRecheck(EXP_EvalVerifContext* ctx)
{
    EXP_Space* space = ctx->space;
    ctx->dataStackShiftEnable = true;
    ctx->recheckFlag = true;
    for (u32 i = 0; i < ctx->recheckNodes.length; ++i)
    {
        ctx->dataStack.length = 0;
        EXP_Node* pNode = ctx->recheckNodes.data + i;
        EXP_Node parent = ctx->blockTable.data[pNode->id].parent;
        EXP_EvalVerifCall blk = { parent, 0, pNode, pNode + 1, EXP_EvalBlockCallback_NONE };
        vec_push(&ctx->callStack, blk);
        EXP_evalVerifCall(ctx);
        if (ctx->error.code)
        {
            return;
        }
    }
}









EXP_EvalError EXP_evalVerif
(
    EXP_Space* space, EXP_Node root,
    EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_EvalFunTable* funTable, EXP_EvalBlockTable* blockTable, vec_u32* typeStack,
    EXP_SpaceSrcInfo* srcInfo
)
{
    EXP_EvalError error = { 0 };
    //return error;
    if (!EXP_isSeq(space, root))
    {
        return error;
    }
    EXP_EvalVerifContext _ctx = EXP_newEvalVerifContext(space, valueTypeTable, nativeFunTable, srcInfo);
    EXP_EvalVerifContext* ctx = &_ctx;

    vec_dup(&ctx->dataStack, typeStack);

    u32 len = EXP_seqLen(space, root);
    EXP_Node* seq = EXP_seqElm(space, root);
    if (!EXP_evalVerifEnterBlock(ctx, seq, len, root, EXP_Node_Invalid, EXP_EvalBlockCallback_NONE, true))
    {
        error = ctx->error;
        EXP_evalVerifContextFree(ctx);
        return error;
    }
    EXP_evalVerifCall(ctx);
    if (!ctx->error.code)
    {
        vec_dup(typeStack, &ctx->dataStack);
    }
    if (!ctx->error.code)
    {
        EXP_evalVerifRecheck(ctx);
    }
    if (!ctx->error.code)
    {
        vec_resize(blockTable, ctx->blockTable.length);
        for (u32 i = 0; i < ctx->blockTable.length; ++i)
        {
            EXP_EvalVerifBlock* vb = ctx->blockTable.data + i;
            EXP_EvalBlock b = { vb->parent };
            b.funsOffset = funTable->length;
            for (u32 i = 0; i < vb->defs.length; ++i)
            {
                EXP_EvalVerifDef* vdef = vb->defs.data + i;
                if (vdef->isVal)
                {
                    ++b.varsCount;
                    continue;
                }
                ++b.funsCount;
                EXP_EvalFun fun = { vdef->key, vdef->fun };
                vec_push(funTable, fun);
            }
            blockTable->data[i] = b;
        }
    }
    error = ctx->error;
    EXP_evalVerifContextFree(ctx);
    return error;
}

















































































































































