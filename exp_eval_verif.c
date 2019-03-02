#include "exp_eval_a.h"



typedef struct EXP_EvalVerifVar
{
    u32 valType;
    EXP_Node block;
    u32 id;
} EXP_EvalVerifVar;

typedef struct EXP_EvalVerifDef
{
    EXP_Node key;
    bool isVar;
    union
    {
        EXP_Node fun;
        EXP_EvalVerifVar var;
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
    u32 varsCount;

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
    EXP_EvalVerifBlockCallbackType_Ncall,
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
        u32 nfun;
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

typedef vec_t(EXP_EvalVerifCall) EXP_EvalVerifCallVec;




typedef struct EXP_EvalVerifEvalWorld
{
    u32 dsOff;
    u32 dsLen;
    u32 csOff;
    u32 csLen;
    bool allowDsShift;
} EXP_EvalVerifEvalWorld;

typedef vec_t(EXP_EvalVerifEvalWorld) EXP_EvalVerifEvalWorldVec;



typedef struct EXP_EvalVerifContext
{
    EXP_Space* space;
    EXP_EvalValueTypeInfoTable* valueTypeTable;
    EXP_EvalNfunInfoTable* nfunTable;
    EXP_EvalNodeTable* nodeTable;
    EXP_SpaceSrcInfo* srcInfo;

    u32 blockTableBase;
    EXP_EvalVerifBlockTable blockTable;

    bool allowDsShift;
    bool recheckPassFlag;
    EXP_NodeVec recheckNodes;

    vec_u32 dsWorldBuf;
    EXP_EvalVerifCallVec csWorldBuf;
    EXP_EvalVerifEvalWorldVec worldStack;

    vec_u32 dataStack;
    EXP_EvalVerifCallVec callStack;

    EXP_EvalError error;
    EXP_NodeVec varKeyBuf;
    EXP_Node recheckRoot;
} EXP_EvalVerifContext;





static EXP_EvalVerifContext EXP_newEvalVerifContext
(
    EXP_Space* space,
    EXP_EvalValueTypeInfoTable* valueTypeTable,
    EXP_EvalNfunInfoTable* nfunTable,
    EXP_EvalNodeTable* nodeTable,
    EXP_SpaceSrcInfo* srcInfo
)
{
    EXP_EvalVerifContext _ctx = { 0 };
    EXP_EvalVerifContext* ctx = &_ctx;
    ctx->space = space;
    ctx->valueTypeTable = valueTypeTable;
    ctx->nfunTable = nfunTable;
    ctx->nodeTable = nodeTable;
    ctx->srcInfo = srcInfo;

    u32 n = EXP_spaceNodesTotal(space);
    u32 nodeTableLength0 = nodeTable->length;
    vec_resize(nodeTable, n);
    memset(nodeTable->data + nodeTableLength0, 0, sizeof(EXP_EvalNode)*n);

    ctx->blockTableBase = nodeTableLength0;
    vec_resize(&ctx->blockTable, n);
    memset(ctx->blockTable.data, 0, sizeof(EXP_EvalVerifBlock)*ctx->blockTable.length);
    return *ctx;
}

static void EXP_evalVerifContextFree(EXP_EvalVerifContext* ctx)
{
    vec_free(&ctx->varKeyBuf);

    vec_free(&ctx->callStack);
    vec_free(&ctx->dataStack);

    vec_free(&ctx->worldStack);
    vec_free(&ctx->csWorldBuf);
    vec_free(&ctx->dsWorldBuf);

    vec_free(&ctx->recheckNodes);

    for (u32 i = 0; i < ctx->blockTable.length; ++i)
    {
        EXP_EvalVerifBlock* b = ctx->blockTable.data + i;
        EXP_evalVerifBlockFree(b);
    }
    vec_free(&ctx->blockTable);
}








static void EXP_evalVerifPushWorld(EXP_EvalVerifContext* ctx, bool allowDsShift)
{
    EXP_EvalVerifEvalWorld world = { 0 };
    world.dsOff = ctx->dsWorldBuf.length;
    world.dsLen = ctx->dataStack.length;
    world.csOff = ctx->csWorldBuf.length;
    world.csLen = ctx->callStack.length;
    world.allowDsShift = ctx->allowDsShift;
    vec_push(&ctx->worldStack, world);
    vec_concat(&ctx->dsWorldBuf, &ctx->dataStack);
    vec_concat(&ctx->csWorldBuf, &ctx->callStack);
    ctx->dataStack.length = 0;
    ctx->callStack.length = 0;
    ctx->allowDsShift = allowDsShift;
}


static void EXP_evalVerifPopWorld(EXP_EvalVerifContext* ctx)
{
    assert(0 == ctx->callStack.length);
    assert(ctx->worldStack.length > 0);
    EXP_EvalVerifEvalWorld world = vec_last(&ctx->worldStack);
    vec_pop(&ctx->worldStack);
    ctx->dataStack.length = 0;
    vec_pusharr(&ctx->dataStack, ctx->dsWorldBuf.data + world.dsOff, world.dsLen);
    vec_pusharr(&ctx->callStack, ctx->csWorldBuf.data + world.csOff, world.csLen);
    ctx->allowDsShift = world.allowDsShift;
    ctx->recheckPassFlag = false;
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










static EXP_EvalVerifBlock* EXP_evalVerifGetBlock(EXP_EvalVerifContext* ctx, EXP_Node node)
{
    assert(node.id >= ctx->blockTableBase);
    EXP_EvalVerifBlock* b = ctx->blockTable.data + node.id - ctx->blockTableBase;
    return b;
}





static bool EXP_evalVerifGetMatched
(
    EXP_EvalVerifContext* ctx, const char* name, EXP_Node blkNode, EXP_EvalVerifDef* outDef
)
{
    EXP_Space* space = ctx->space;
    while (blkNode.id != EXP_NodeId_Invalid)
    {
        EXP_EvalVerifBlock* blk = EXP_evalVerifGetBlock(ctx, blkNode);
        for (u32 i = 0; i < blk->defs.length; ++i)
        {
            EXP_EvalVerifDef* def = blk->defs.data + blk->defs.length - 1 - i;
            const char* str = EXP_tokCstr(space, def->key);
            if (0 == strcmp(str, name))
            {
                *outDef = *def;
                return true;
            }
        }
        blkNode = blk->parent;
    }
    return false;
}




static u32 EXP_evalVerifGetNfun(EXP_EvalVerifContext* ctx, const char* name)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->nfunTable->length; ++i)
    {
        u32 idx = ctx->nfunTable->length - 1 - i;
        const char* s = ctx->nfunTable->data[idx].name;
        if (0 == strcmp(s, name))
        {
            return idx;
        }
    }
    return -1;
}




static EXP_EvalKey EXP_evalVerifGetKey(EXP_EvalVerifContext* ctx, const char* name)
{
    for (EXP_EvalKey i = 0; i < EXP_NumEvalKeys; ++i)
    {
        EXP_EvalKey k = EXP_NumEvalKeys - 1 - i;
        const char* s = EXP_EvalKeyNameTable[k];
        if (0 == strcmp(s, name))
        {
            return k;
        }
    }
    return -1;
}





static void EXP_evalVerifDefGetBody(EXP_EvalVerifContext* ctx, EXP_Node node, u32* pLen, EXP_Node** pSeq)
{
    EXP_Space* space = ctx->space;
    assert(EXP_seqLen(space, node) >= 2);
    *pLen = EXP_seqLen(space, node) - 2;
    EXP_Node* defCall = EXP_seqElm(space, node);
    *pSeq = defCall + 2;
}







static bool EXP_evalVerifIsFunDef(EXP_EvalVerifContext* ctx, EXP_Node node)
{
    EXP_Space* space = ctx->space;
    if (EXP_isTok(space, node))
    {
        return false;
    }
    if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return false;
    }
    EXP_Node* defCall = EXP_seqElm(space, node);
    const char* kDef = EXP_tokCstr(space, defCall[0]);
    EXP_EvalKey k = EXP_evalVerifGetKey(ctx, kDef);
    if (k != EXP_EvalKey_Def)
    {
        return false;
    }
    if (!EXP_isTok(space, defCall[1]))
    {
        EXP_evalVerifErrorAtNode(ctx, defCall[1], EXP_EvalErrCode_EvalSyntax);
        return false;
    }
    return true;
}










static void EXP_evalVerifLoadDef(EXP_EvalVerifContext* ctx, EXP_Node node, EXP_EvalVerifBlock* blk)
{
    if (!EXP_evalVerifIsFunDef(ctx, node))
    {
        return;
    }
    EXP_Space* space = ctx->space;
    EXP_Node* defCall = EXP_seqElm(space, node);
    EXP_Node name = defCall[1];
    EXP_EvalVerifDef def = { name, false, .fun = node };
    vec_push(&blk->defs, def);
}









static void EXP_evalVerifEnterBlock
(
    EXP_EvalVerifContext* ctx, EXP_Node* seq, u32 len, EXP_Node srcNode, EXP_Node parent, EXP_EvalVerifBlockCallback cb,
    bool isDefScope
)
{
    u32 dataStackP = ctx->dataStack.length;
    EXP_EvalVerifCall call = { srcNode, dataStackP, seq, seq + len, cb };
    vec_push(&ctx->callStack, call);

    EXP_EvalVerifBlock* blk = EXP_evalVerifGetBlock(ctx, srcNode);
    if (EXP_EvalVerifBlockTypeInferState_Done == blk->typeInferState)
    {
        assert(ctx->recheckPassFlag);
        assert(blk->parent.id == parent.id);
        EXP_evalVerifBlockReset(blk);
    }
    assert(EXP_EvalVerifBlockTypeInferState_None == blk->typeInferState);
    blk->typeInferState = EXP_EvalVerifBlockTypeInferState_Entered;
    blk->parent = parent;
    if (isDefScope)
    {
        for (u32 i = 0; i < len; ++i)
        {
            EXP_evalVerifLoadDef(ctx, seq[i], blk);
            if (ctx->error.code)
            {
                return;
            }
        }
    }
}



static void EXP_evalVerifLeaveBlock(EXP_EvalVerifContext* ctx)
{
    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);

    if (EXP_EvalVerifBlockTypeInferState_Done == curBlock->typeInferState)
    {
        assert(ctx->recheckPassFlag);
        assert(1 == ctx->callStack.length);
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
    EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);
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
    EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);
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
    EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);
    assert(EXP_EvalVerifBlockTypeInferState_Entered == curBlock->typeInferState);
    EXP_evalVerifBlockReset(curBlock);

    vec_pop(&ctx->callStack);
}






static bool EXP_evalVerifShiftDataStack(EXP_EvalVerifContext* ctx, u32 n, const u32* a)
{
    if (!ctx->allowDsShift)
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
    EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);
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






static void EXP_evalVerifNfunCall(EXP_EvalVerifContext* ctx, EXP_EvalNfunInfo* nfunInfo, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    vec_u32* dataStack = &ctx->dataStack;

    assert(dataStack->length >= nfunInfo->numIns);
    u32 argsOffset = dataStack->length - nfunInfo->numIns;
    EXP_evalVerifCurBlockInsUpdate(ctx, argsOffset, nfunInfo->inType);

    for (u32 i = 0; i < nfunInfo->numIns; ++i)
    {
        u32 vt1 = dataStack->data[argsOffset + i];
        u32 vt = nfunInfo->inType[i];
        if (!EXP_evalTypeMatch(vt, vt1))
        {
            EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < nfunInfo->numOuts; ++i)
    {
        vec_push(dataStack, nfunInfo->outType[i]);
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


static void EXP_evalVerifRecurFun
(
    EXP_EvalVerifContext* ctx, EXP_EvalVerifCall* curCall, const EXP_EvalVerifBlock* funInfo
)
{
    assert(EXP_EvalVerifBlockTypeInferState_Entered == funInfo->typeInferState);
    assert(!ctx->recheckPassFlag);
    EXP_Space* space = ctx->space;

    EXP_Node lastSrcNode = EXP_Node_Invalid;
    while (ctx->callStack.length > 0)
    {
        lastSrcNode = curCall->srcNode;
        curCall = &vec_last(&ctx->callStack);
        EXP_Node srcNode = curCall->srcNode;
        EXP_EvalVerifBlockCallback* cb = &curCall->cb;
        // quit this branch until recheck pass
        if (EXP_EvalVerifBlockCallbackType_Branch0 == cb->type)
        {
            if (EXP_evalIfHasBranch1(space, srcNode))
            {
                EXP_evalVerifBlockRebase(ctx);
                curCall->p = EXP_evalIfBranch1(space, srcNode);
                curCall->end = EXP_evalIfBranch1(space, srcNode) + 1;
                cb->type = EXP_EvalVerifBlockCallbackType_NONE;
                EXP_evalVerifAddRecheck(ctx, srcNode);
                return;
            }
        }
        else if (EXP_EvalVerifBlockCallbackType_BranchUnify == cb->type)
        {
            EXP_evalVerifBlockRebase(ctx);
            curCall->p = EXP_evalIfBranch0(space, srcNode);
            curCall->end = EXP_evalIfBranch0(space, srcNode) + 1;
            cb->type = EXP_EvalVerifBlockCallbackType_NONE;
            EXP_evalVerifAddRecheck(ctx, srcNode);
            return;
        }
        EXP_evalVerifCancelBlock(ctx);
    }
    EXP_evalVerifErrorAtNode(ctx, lastSrcNode, EXP_EvalErrCode_EvalRecurNoBaseCase);
}













static void EXP_evalVerifEnterWorld
(
    EXP_EvalVerifContext* ctx, EXP_Node* seq, u32 len, EXP_Node src, EXP_Node parent, bool allowDsShift
)
{
    EXP_evalVerifPushWorld(ctx, allowDsShift);
    EXP_evalVerifEnterBlock(ctx, seq, len, src, parent, EXP_EvalBlockCallback_NONE, true);
}






static void EXP_evalVerifNode
(
    EXP_EvalVerifContext* ctx, EXP_Node node, EXP_EvalVerifCall* curCall, EXP_EvalVerifBlock* curBlock
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalNodeTable* nodeTable = ctx->nodeTable;
    EXP_EvalVerifBlockTable* blockTable = &ctx->blockTable;
    vec_u32* dataStack = &ctx->dataStack;

    EXP_EvalNode* enode = nodeTable->data + node.id;
    if (EXP_isTok(space, node))
    {
        const char* name = EXP_tokCstr(space, node);

        EXP_EvalKey k = EXP_evalVerifGetKey(ctx, name);
        switch (k)
        {
        case EXP_EvalKey_VarDefBegin:
        {
            enode->type = EXP_EvalNodeType_VarDefBegin;
            if (curCall->cb.type != EXP_EvalVerifBlockCallbackType_NONE)
            {
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return;
            }
            ctx->varKeyBuf.length = 0;
            for (u32 n = 0;;)
            {
                if (curCall->p == curCall->end)
                {
                    EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
                    return;
                }
                node = *(curCall->p++);
                enode = nodeTable->data + node.id;
                const char* skey = EXP_tokCstr(space, node);
                if (!EXP_isTok(space, node))
                {
                    EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                EXP_EvalKey k = EXP_evalVerifGetKey(ctx, EXP_tokCstr(space, node));
                if (k != -1)
                {
                    if (EXP_EvalKey_VarDefEnd == k)
                    {
                        enode->type = EXP_EvalNodeType_VarDefEnd;
                        if (n > dataStack->length)
                        {
                            u32 shiftN = n - dataStack->length;
                            for (u32 i = 0; i < shiftN; ++i)
                            {
                                u32 a[] = { EXP_EvalValueType_Any };
                                if (!EXP_evalVerifShiftDataStack(ctx, 1, a))
                                {
                                    EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                                    return;
                                }
                            }
                        }

                        u32 off = dataStack->length - n;
                        for (u32 i = 0; i < n; ++i)
                        {
                            u32 vt = dataStack->data[off + i];
                            EXP_EvalVerifVar var = { vt, curCall->srcNode.id, curBlock->varsCount };
                            EXP_EvalVerifDef def = { ctx->varKeyBuf.data[i], true, .var = var };
                            vec_push(&curBlock->defs, def);
                            ++curBlock->varsCount;
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
                                assert(def->isVar);
                                vec_insert(&curBlock->typeInOut, i, def->var.valType);
                            }
                        }
                        return;
                    }
                    EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                vec_push(&ctx->varKeyBuf, node);
                ++n;
            }
        }
        case EXP_EvalKey_Drop:
        {
            enode->type = EXP_EvalNodeType_Drop;
            if (!dataStack->length)
            {
                u32 a[] = { EXP_EvalValueType_Any };
                if (!EXP_evalVerifShiftDataStack(ctx, 1, a))
                {
                    EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                    return;
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
            return;
        }
        default:
            break;
        }

        u32 nfun = EXP_evalVerifGetNfun(ctx, name);
        if (nfun != -1)
        {
            enode->type = EXP_EvalNodeType_Nfun;
            enode->nfun = nfun;
            EXP_EvalNfunInfo* nfunInfo = ctx->nfunTable->data + nfun;
            if (!nfunInfo->call)
            {
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return;
            }
            if (dataStack->length < nfunInfo->numIns)
            {
                u32 n = nfunInfo->numIns - dataStack->length;
                if (!EXP_evalVerifShiftDataStack(ctx, n, nfunInfo->inType))
                {
                    EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                    return;
                }
            }
            EXP_evalVerifNfunCall(ctx, nfunInfo, node);
            return;
        }

        EXP_EvalVerifDef def = { 0 };
        if (EXP_evalVerifGetMatched(ctx, name, curCall->srcNode, &def))
        {
            if (def.isVar)
            {
                enode->type = EXP_EvalNodeType_Var;
                enode->var.block = def.var.block;
                enode->var.id = def.var.id;
                vec_push(dataStack, def.var.valType);
                return;
            }
            else
            {
                enode->type = EXP_EvalNodeType_Fun;
                enode->funDef = def.fun;
                EXP_Node fun = def.fun;
                EXP_EvalVerifBlock* funInfo = blockTable->data + fun.id;
                if (EXP_EvalVerifBlockTypeInferState_Done == funInfo->typeInferState)
                {
                    if (dataStack->length < funInfo->numIns)
                    {
                        u32 n = funInfo->numIns - dataStack->length;
                        if (!EXP_evalVerifShiftDataStack(ctx, n, funInfo->typeInOut.data))
                        {
                            EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                            return;
                        }
                    }
                    EXP_evalVerifFunCall(ctx, funInfo, node);
                    return;
                }
                else if (EXP_EvalVerifBlockTypeInferState_None == funInfo->typeInferState)
                {
                    u32 bodyLen = 0;
                    EXP_Node* body = NULL;
                    EXP_evalVerifDefGetBody(ctx, fun, &bodyLen, &body);

                    --curCall->p;
                    EXP_evalVerifEnterWorld(ctx, body, bodyLen, fun, curCall->srcNode, true);
                    return;
                }
                else
                {
                    EXP_evalVerifRecurFun(ctx, curCall, funInfo);
                    if (ctx->error.code)
                    {
                        return;
                    }
                    return;
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
                            enode->type = EXP_EvalNodeType_Value;
                            enode->value = v;
                            vec_push(dataStack, j);
                            return;
                        }
                    }
                }
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalUndefined);
                return;
            }
            enode->type = EXP_EvalNodeType_String;
            vec_push(dataStack, EXP_EvalPrimValueType_STRING);
            return;
        }
    }

    if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }

    EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);
    const char* name = EXP_tokCstr(space, elms[0]);

    EXP_EvalVerifDef def = { 0 };
    if (EXP_evalVerifGetMatched(ctx, name, curCall->srcNode, &def))
    {
        if (def.isVar)
        {
            enode->type = EXP_EvalNodeType_CallVar;
            enode->var.block = def.var.block;
            enode->var.id = def.var.id;
            vec_push(dataStack, def.var.valType);
            return;
        }
        else
        {
            enode->type = EXP_EvalNodeType_CallFun;
            enode->funDef = def.fun;

            EXP_EvalVerifBlockCallback cb = { EXP_EvalVerifBlockCallbackType_Call, .fun = def.fun };
            EXP_evalVerifEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
            return;
        }
    }

    u32 nfun = EXP_evalVerifGetNfun(ctx, name);
    if (nfun != -1)
    {
        enode->type = EXP_EvalNodeType_CallNfun;
        enode->nfun = nfun;
        EXP_EvalNfunInfo* nfunInfo = ctx->nfunTable->data + nfun;
        assert(nfunInfo->call);
        EXP_EvalVerifBlockCallback cb = { EXP_EvalVerifBlockCallbackType_Ncall, .nfun = nfun };
        EXP_evalVerifEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
        return;
    }

    EXP_EvalKey k = EXP_evalVerifGetKey(ctx, name);
    switch (k)
    {
    case EXP_EvalKey_Def:
    {
        enode->type = EXP_EvalNodeType_Def;
        return;
    }
    case EXP_EvalKey_If:
    {
        enode->type = EXP_EvalNodeType_If;
        if ((len != 3) && (len != 4))
        {
            EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
            return;
        }
        EXP_EvalVerifBlockCallback cb = { EXP_EvalVerifBlockCallbackType_Cond };
        EXP_evalVerifEnterBlock(ctx, elms + 1, 1, node, curCall->srcNode, cb, false);
        return;
    }
    default:
    {
        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }
    }
}








static void EXP_evalVerifRecheck(EXP_EvalVerifContext* ctx)
{
    ctx->dataStack.length = 0;
    assert(0 == ctx->callStack.length);
    EXP_Node node = vec_last(&ctx->recheckNodes);
    vec_pop(&ctx->recheckNodes);
    ctx->recheckRoot = node;
    EXP_Node parent = EXP_evalVerifGetBlock(ctx, node)->parent;
    EXP_EvalVerifCall blk = { parent, 0, &ctx->recheckRoot, &ctx->recheckRoot + 1, EXP_EvalBlockCallback_NONE };
    vec_push(&ctx->callStack, blk);
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
        if (ctx->worldStack.length > 0)
        {
            EXP_evalVerifPopWorld(ctx);
            goto next;
        }
        if (!ctx->recheckPassFlag)
        {
            ctx->recheckPassFlag = true;
        }
        if (ctx->recheckNodes.length > 0)
        {
            EXP_evalVerifRecheck(ctx);
            goto next;
        }
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
        case EXP_EvalVerifBlockCallbackType_Ncall:
        {
            if (curBlock->numIns > 0)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            if (dataStack->length < curCall->dataStackP)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            EXP_EvalNfunInfo* nfunInfo = ctx->nfunTable->data + cb->nfun;
            u32 numIns = dataStack->length - curCall->dataStackP;
            if (numIns != nfunInfo->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            EXP_evalVerifNfunCall(ctx, nfunInfo, curCall->srcNode);
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
                    goto next;
                }
                if (curCall->dataStackP != (dataStack->length - funInfo->numIns))
                {
                    EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    goto next;
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
                    goto next;
                }
                u32 bodyLen = 0;
                EXP_Node* body = NULL;
                EXP_evalVerifDefGetBody(ctx, fun, &bodyLen, &body);

                EXP_evalVerifEnterWorld(ctx, body, bodyLen, fun, srcNode, true);
                goto next;
            }
            else
            {
                EXP_evalVerifRecurFun(ctx, curCall, funInfo);
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
                goto next;
            }
            u32 vt = dataStack->data[curCall->dataStackP];
            vec_pop(dataStack);
            if (!EXP_evalTypeMatch(EXP_EvalPrimValueType_BOOL, vt))
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
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
                        EXP_evalVerifErrorAtNode(ctx, *EXP_evalIfBranch0(space, srcNode), EXP_EvalErrCode_EvalArgs);
                        goto next;
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
                goto next;
            }
            if (b0->numOuts != b1->numOuts)
            {
                EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalBranchUneq);
                goto next;
            }
            assert(b0->typeInOut.length == b1->typeInOut.length);
            for (u32 i = 0; i < b0->typeInOut.length; ++i)
            {
                u32 t;
                bool m = EXP_evalTypeUnify(b0->typeInOut.data[i], b1->typeInOut.data[i], &t);
                if (!m)
                {
                    EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalBranchUneq);
                    goto next;
                }
                // todo
                assert(EXP_evalTypeMatch(b1->typeInOut.data[i], curBlock->typeInOut.data[i]));
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
    EXP_evalVerifNode(ctx, node, curCall, curBlock);
    goto next;
}



























EXP_EvalError EXP_evalVerif
(
    EXP_Space* space, EXP_Node root,
    EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNfunInfoTable* nfunTable,
    EXP_EvalNodeTable* nodeTable, vec_u32* typeStack, EXP_SpaceSrcInfo* srcInfo
)
{
    EXP_EvalError error = { 0 };
    //return error;
    if (!EXP_isSeq(space, root))
    {
        return error;
    }
    EXP_EvalVerifContext _ctx = EXP_newEvalVerifContext(space, valueTypeTable, nfunTable, nodeTable, srcInfo);
    EXP_EvalVerifContext* ctx = &_ctx;

    ctx->allowDsShift = false;
    vec_dup(&ctx->dataStack, typeStack);

    EXP_Node* seq = EXP_seqElm(space, root);
    u32 len = EXP_seqLen(space, root);
    EXP_evalVerifEnterBlock(ctx, seq, len, root, EXP_Node_Invalid, EXP_EvalBlockCallback_NONE, true);
    if (ctx->error.code)
    {
        error = ctx->error;
        EXP_evalVerifContextFree(ctx);
        return error;
    }
    EXP_evalVerifCall(ctx);
    {
        EXP_EvalVerifBlock* blk = EXP_evalVerifGetBlock(ctx, root);
        assert(EXP_EvalVerifBlockTypeInferState_Done == blk->typeInferState);
        assert(0 == blk->numIns);
        typeStack->length = 0;
        vec_pusharr(typeStack, blk->typeInOut.data + blk->numIns, blk->numOuts);
    }
    if (!ctx->error.code)
    {
        for (u32 i = 0; i < ctx->blockTable.length; ++i)
        {
            EXP_EvalVerifBlock* vb = ctx->blockTable.data + i;
            EXP_EvalNode* enode = nodeTable->data + i + ctx->blockTableBase;
            for (u32 i = 0; i < vb->defs.length; ++i)
            {
                EXP_EvalVerifDef* vdef = vb->defs.data + i;
                if (vdef->isVar)
                {
                    ++enode->varsCount;
                }
            }
            assert(enode->varsCount == vb->varsCount);
        }
    }
    error = ctx->error;
    EXP_evalVerifContextFree(ctx);
    return error;
}

















































































































































