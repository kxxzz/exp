#include "exp_eval_a.h"
#include "exp_eval_type_a.h"



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






typedef struct EXP_EvalVerifBlock
{
    bool completed;
    bool haveInOut;
    bool incomplete;

    bool entered;
    vec_u32 ins;

    EXP_Node parent;
    u32 varsCount;
    EXP_EvalVerifDefTable defs;

    u32 numIns;
    u32 numOuts;
    vec_u32 inout;
} EXP_EvalVerifBlock;

typedef vec_t(EXP_EvalVerifBlock) EXP_EvalVerifBlockTable;

static void EXP_evalVerifBlockFree(EXP_EvalVerifBlock* blk)
{
    vec_free(&blk->inout);
    vec_free(&blk->defs);
    vec_free(&blk->ins);
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
        u32 afun;
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




typedef struct EXP_EvalVerifSnapshot
{
    bool allowDsShift;
    u32 dsOff;
    u32 dsLen;
    u32 csOff;
    u32 csLen;
    u32 tvsOff;
    u32 tvsLen;
    u32 tvCount;
} EXP_EvalVerifSnapshot;

typedef vec_t(EXP_EvalVerifSnapshot) EXP_EvalVerifWorldStack;



typedef struct EXP_EvalVerifContext
{
    EXP_Space* space;
    EXP_SpaceSrcInfo* srcInfo;
    EXP_EvalAtypeInfoTable* atypeTable;
    EXP_EvalAfunInfoTable* afunTable;
    EXP_EvalNodeTable* nodeTable;
    EXP_EvalTypeContext* typeContext;

    u32 blockTableBase;
    EXP_EvalVerifBlockTable blockTable;

    EXP_EvalTypeVarSpace typeVarSpace;

    bool allowDsShift;
    vec_u32 dataStack;
    EXP_EvalVerifCallVec callStack;
    u32 tvCount;

    vec_u32 dsBuf;
    EXP_EvalVerifCallVec csBuf;
    EXP_EvalTypeVarSpace tvsBuf;
    EXP_EvalVerifWorldStack worldStack;

    EXP_EvalError error;
    EXP_NodeVec varKeyBuf;
    vec_u32 typeBuf;
    EXP_EvalTypeVarSpace typeVarSpace1;
} EXP_EvalVerifContext;





static EXP_EvalVerifContext EXP_newEvalVerifContext
(
    EXP_Space* space, EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalAtypeInfoTable* atypeTable, EXP_EvalAfunInfoTable* afunTable,
    EXP_EvalNodeTable* nodeTable,
    EXP_EvalTypeContext* typeContext
)
{
    EXP_EvalVerifContext _ctx = { 0 };
    EXP_EvalVerifContext* ctx = &_ctx;
    ctx->space = space;
    ctx->srcInfo = srcInfo;
    ctx->atypeTable = atypeTable;
    ctx->afunTable = afunTable;
    ctx->nodeTable = nodeTable;
    ctx->typeContext = typeContext;

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
    vec_free(&ctx->typeVarSpace1);
    vec_free(&ctx->typeBuf);
    vec_free(&ctx->varKeyBuf);

    vec_free(&ctx->worldStack);
    vec_free(&ctx->csBuf);
    vec_free(&ctx->dsBuf);

    vec_free(&ctx->callStack);
    vec_free(&ctx->dataStack);

    vec_free(&ctx->typeVarSpace);

    for (u32 i = 0; i < ctx->blockTable.length; ++i)
    {
        EXP_EvalVerifBlock* b = ctx->blockTable.data + i;
        EXP_evalVerifBlockFree(b);
    }
    vec_free(&ctx->blockTable);
}








static void EXP_evalVerifPushWorld(EXP_EvalVerifContext* ctx, bool allowDsShift)
{
    EXP_EvalVerifSnapshot snapshot = { 0 };
    snapshot.allowDsShift = ctx->allowDsShift;
    snapshot.dsOff = ctx->dsBuf.length;
    snapshot.dsLen = ctx->dataStack.length;
    snapshot.csOff = ctx->csBuf.length;
    snapshot.csLen = ctx->callStack.length;
    snapshot.tvsOff = ctx->tvsBuf.length;
    snapshot.tvsLen = ctx->typeVarSpace.length;
    snapshot.tvCount = ctx->tvCount;
    vec_push(&ctx->worldStack, snapshot);

    vec_concat(&ctx->dsBuf, &ctx->dataStack);
    vec_concat(&ctx->csBuf, &ctx->callStack);
    vec_concat(&ctx->tvsBuf, &ctx->typeVarSpace);
    ctx->allowDsShift = allowDsShift;
    ctx->dataStack.length = 0;
    ctx->callStack.length = 0;
    ctx->typeVarSpace.length = 0;
    ctx->tvCount = 0;
}


static void EXP_evalVerifPopWorld(EXP_EvalVerifContext* ctx)
{
    assert(0 == ctx->callStack.length);
    assert(ctx->worldStack.length > 0);
    EXP_EvalVerifSnapshot snapshot = vec_last(&ctx->worldStack);
    vec_pop(&ctx->worldStack);

    ctx->allowDsShift = snapshot.allowDsShift;
    ctx->dataStack.length = 0;
    ctx->typeVarSpace.length = 0;
    vec_pusharr(&ctx->dataStack, ctx->dsBuf.data + snapshot.dsOff, snapshot.dsLen);
    vec_pusharr(&ctx->callStack, ctx->csBuf.data + snapshot.csOff, snapshot.csLen);
    vec_pusharr(&ctx->typeVarSpace, ctx->tvsBuf.data + snapshot.tvsOff, snapshot.tvsLen);
    vec_resize(&ctx->dsBuf, snapshot.dsOff);
    vec_resize(&ctx->csBuf, snapshot.csOff);
    vec_resize(&ctx->tvsBuf, snapshot.tvsOff);
    ctx->tvCount = snapshot.tvCount;
}





static bool EXP_evalVerifTypeUnify(EXP_EvalVerifContext* ctx, u32 a, u32 b, u32* u)
{
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;
    bool r = EXP_evalTypeUnify(typeContext, typeVarSpace, a, b, u);
    return r;
}




static bool EXP_evalVerifTypeUnifyVarS1(EXP_EvalVerifContext* ctx, u32 x1, u32 x)
{
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;
    EXP_EvalTypeVarSpace* typeVarSpace1 = &ctx->typeVarSpace1;
    return EXP_evalTypeUnifyVarS1(typeContext, typeVarSpace, x, typeVarSpace1, x1);
}

static u32 EXP_evalVerifTypeVarS1Value(EXP_EvalVerifContext* ctx, u32 x1)
{
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace1 = &ctx->typeVarSpace1;
    // todo
    return -1;
}






static void EXP_evalVerifErrorAtNode(EXP_EvalVerifContext* ctx, EXP_Node node, EXP_EvalErrCode errCode)
{
    ctx->error.code = errCode;
    EXP_SpaceSrcInfo* srcInfo = ctx->srcInfo;
    if (srcInfo)
    {
        assert(node.id < srcInfo->nodes.length);
        EXP_NodeSrcInfo* nodeSrcInfo = srcInfo->nodes.data + node.id;
        ctx->error.file = nodeSrcInfo->file;
        ctx->error.line = nodeSrcInfo->line;
        ctx->error.column = nodeSrcInfo->column;
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




static u32 EXP_evalVerifGetAfun(EXP_EvalVerifContext* ctx, const char* name)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->afunTable->length; ++i)
    {
        u32 idx = ctx->afunTable->length - 1 - i;
        const char* s = ctx->afunTable->data[idx].name;
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









static void EXP_evalVerifSetCallersIncomplete(EXP_EvalVerifContext* ctx)
{
    for (u32 i = 0; i < ctx->callStack.length; ++i)
    {
        u32 j = ctx->callStack.length - 1 - i;
        EXP_EvalVerifCall* call = ctx->callStack.data + j;
        EXP_EvalVerifBlock* blk = EXP_evalVerifGetBlock(ctx, call->srcNode);
        if (blk->incomplete)
        {
            break;
        }
        blk->incomplete = true;
    }
}









static void EXP_evalVerifEnterBlock
(
    EXP_EvalVerifContext* ctx, EXP_Node* seq, u32 len, EXP_Node srcNode, EXP_Node parent,
    EXP_EvalVerifBlockCallback cb, bool isDefScope
)
{
    u32 dataStackP = ctx->dataStack.length;
    EXP_EvalVerifCall call = { srcNode, dataStackP, seq, seq + len, cb };
    vec_push(&ctx->callStack, call);

    EXP_EvalVerifBlock* blk = EXP_evalVerifGetBlock(ctx, srcNode);
    assert(!blk->entered);
    blk->entered = true;
    blk->incomplete = false;
    blk->ins.length = 0;

    blk->parent = parent;

    blk->varsCount = 0;
    blk->defs.length = 0;
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



static void EXP_evalVerifSaveBlock(EXP_EvalVerifContext* ctx)
{
    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;

    assert(curBlock->entered);
    assert(!curBlock->completed);

    if (!curBlock->haveInOut)
    {
        curBlock->haveInOut = true;

        assert(0 == curBlock->numIns);
        assert(0 == curBlock->numOuts);
        assert(0 == curBlock->inout.length);

        curBlock->numIns = curBlock->ins.length;

        for (u32 i = 0; i < curBlock->ins.length; ++i)
        {
            u32 t = curBlock->ins.data[i];
            t = EXP_evalTypeNormForm(typeContext, typeVarSpace, t);
            vec_push(&curBlock->inout, t);
        }

        assert(ctx->dataStack.length + curBlock->ins.length >= curCall->dataStackP);
        curBlock->numOuts = ctx->dataStack.length + curBlock->ins.length - curCall->dataStackP;

        for (u32 i = 0; i < curBlock->numOuts; ++i)
        {
            u32 j = ctx->dataStack.length - curBlock->numOuts + i;
            u32 t = ctx->dataStack.data[j];
            t = EXP_evalTypeNormForm(typeContext, typeVarSpace, t);
            vec_push(&curBlock->inout, t);
        }
    }
    else
    {
        if (curBlock->numIns != curBlock->ins.length)
        {
            EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalUnification);
            return;
        }
        assert(ctx->dataStack.length + curBlock->ins.length >= curCall->dataStackP);
        u32 numOuts = ctx->dataStack.length + curBlock->ins.length - curCall->dataStackP;
        if (curBlock->numOuts != numOuts)
        {
            EXP_evalVerifErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalUnification);
            return;
        }

        for (u32 i = 0; i < curBlock->ins.length; ++i)
        {
            u32 t0 = curBlock->inout.data[i];
            u32 t1 = curBlock->ins.data[i];
            u32 t;
            EXP_evalVerifTypeUnify(ctx, t0, t1, &t);
            curBlock->inout.data[i] = t;
        }

        curBlock->numOuts = numOuts;

        for (u32 i = 0; i < curBlock->numOuts; ++i)
        {
            u32 t0 = curBlock->inout.data[curBlock->numIns + i];
            u32 j = ctx->dataStack.length - curBlock->numOuts + i;
            u32 t1 = ctx->dataStack.data[j];
            u32 t;
            EXP_evalVerifTypeUnify(ctx, t0, t1, &t);
            curBlock->inout.data[curBlock->numIns + i] = t;
            ctx->dataStack.data[j] = t;
        }
    }
}


static void EXP_evalVerifLeaveBlock(EXP_EvalVerifContext* ctx)
{
    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);

    EXP_evalVerifSaveBlock(ctx);

    curBlock->entered = false;
    curBlock->completed = !curBlock->incomplete;
    vec_pop(&ctx->callStack);

    if (!curBlock->completed)
    {
        EXP_evalVerifSetCallersIncomplete(ctx);
    }
}




static void EXP_evalVerifBlockRevert(EXP_EvalVerifContext* ctx)
{
    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);

    assert(curBlock->entered);
    assert(curCall->dataStackP >= curBlock->ins.length);

    ctx->dataStack.length = curCall->dataStackP - curBlock->ins.length;
    for (u32 i = 0; i < curBlock->ins.length; ++i)
    {
        vec_push(&ctx->dataStack, curBlock->ins.data[i]);
    }
}




static void EXP_evalVerifCancelBlock(EXP_EvalVerifContext* ctx)
{
    EXP_evalVerifBlockRevert(ctx);

    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);

    curBlock->entered = false;
    curBlock->ins.length = 0;

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
    assert(curBlock->entered);
    assert(curCall->dataStackP >= curBlock->ins.length);
    if (curCall->dataStackP > argsOffset + curBlock->ins.length)
    {
        u32 n = curCall->dataStackP - argsOffset;
        assert(n > curBlock->ins.length);
        u32 added = n - curBlock->ins.length;
        for (u32 i = 0; i < added; ++i)
        {
            vec_insert(&curBlock->ins, i, funInTypes[i]);
        }
    }
}






static void EXP_evalVerifAfunCall(EXP_EvalVerifContext* ctx, EXP_EvalAfunInfo* afunInfo, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    if (!afunInfo->call)
    {
        EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
        return;
    }

    u32 inEvalType[EXP_EvalAfunIns_MAX];
    for (u32 i = 0; i < afunInfo->numIns; ++i)
    {
        inEvalType[i] = EXP_evalTypeAtom(ctx->typeContext, afunInfo->inAtype[i]);
    }

    if (dataStack->length < afunInfo->numIns)
    {
        u32 n = afunInfo->numIns - dataStack->length;
        if (!EXP_evalVerifShiftDataStack(ctx, n, inEvalType))
        {
            EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
    }

    assert(dataStack->length >= afunInfo->numIns);
    u32 argsOffset = dataStack->length - afunInfo->numIns;
    EXP_evalVerifCurBlockInsUpdate(ctx, argsOffset, inEvalType);

    for (u32 i = 0; i < afunInfo->numIns; ++i)
    {
        u32 x1 = inEvalType[i];
        u32 x = dataStack->data[argsOffset + i];
        if (!EXP_evalVerifTypeUnifyVarS1(ctx, x1, x))
        {
            EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < afunInfo->numOuts; ++i)
    {
        u32 x1 = EXP_evalTypeAtom(ctx->typeContext, afunInfo->outAtype[i]);
        u32 x = EXP_evalVerifTypeVarS1Value(ctx, x1);
        vec_push(dataStack, x);
    }
}




static void EXP_evalVerifBlockCall(EXP_EvalVerifContext* ctx, const EXP_EvalVerifBlock* blk, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    assert(blk->haveInOut);
    assert(dataStack->length >= blk->numIns);
    u32 argsOffset = dataStack->length - blk->numIns;
    EXP_evalVerifCurBlockInsUpdate(ctx, argsOffset, blk->inout.data);

    assert((blk->numIns + blk->numOuts) == blk->inout.length);
    for (u32 i = 0; i < blk->numIns; ++i)
    {
        u32 x1 = blk->inout.data[i];
        u32 x = dataStack->data[argsOffset + i];
        if (!EXP_evalVerifTypeUnifyVarS1(ctx, x1, x))
        {
            EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < blk->numOuts; ++i)
    {
        u32 x1 = blk->inout.data[blk->numIns + i];
        u32 x = EXP_evalVerifTypeVarS1Value(ctx, x1);
        vec_push(dataStack, x);
    }
}













static void EXP_evalVerifRecurFallbackToOtherBranch(EXP_EvalVerifContext* ctx)
{
    EXP_Space* space = ctx->space;

    EXP_EvalVerifCall* curCall = &vec_last(&ctx->callStack);
    EXP_Node lastSrcNode = EXP_Node_Invalid;
    while (ctx->callStack.length > 0)
    {
        lastSrcNode = curCall->srcNode;
        curCall = &vec_last(&ctx->callStack);
        EXP_EvalVerifBlock* curBlock = EXP_evalVerifGetBlock(ctx, curCall->srcNode);
        EXP_Node srcNode = curCall->srcNode;
        EXP_EvalVerifBlockCallback* cb = &curCall->cb;
        // quit this branch until next enter
        if (EXP_EvalVerifBlockCallbackType_Branch0 == cb->type)
        {
            if (EXP_evalIfHasBranch1(space, srcNode))
            {
                curBlock->incomplete = true;
                EXP_evalVerifBlockRevert(ctx);
                curCall->p = EXP_evalIfBranch1(space, srcNode);
                curCall->end = EXP_evalIfBranch1(space, srcNode) + 1;
                cb->type = EXP_EvalVerifBlockCallbackType_NONE;
                return;
            }
        }
        else if (EXP_EvalVerifBlockCallbackType_BranchUnify == cb->type)
        {
            curBlock->incomplete = true;
            EXP_evalVerifBlockRevert(ctx);
            curCall->p = EXP_evalIfBranch0(space, srcNode);
            curCall->end = EXP_evalIfBranch0(space, srcNode) + 1;
            cb->type = EXP_EvalVerifBlockCallbackType_NONE;
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
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

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
                            ctx->typeBuf.length = 0;
                            for (u32 i = 0; i < shiftN; ++i)
                            {
                                u32 t = EXP_evalTypeVar(typeContext, ctx->tvCount++);
                                vec_push(&ctx->typeBuf, t);
                            }
                            if (!EXP_evalVerifShiftDataStack(ctx, shiftN, ctx->typeBuf.data))
                            {
                                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                                return;
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

                        if (curCall->dataStackP > dataStack->length + curBlock->ins.length)
                        {
                            u32 n = curCall->dataStackP - dataStack->length - curBlock->ins.length;
                            u32 added = n - curBlock->ins.length;
                            for (u32 i = 0; i < added; ++i)
                            {
                                EXP_EvalVerifDef* def = curBlock->defs.data + curBlock->defs.length - added + i;
                                assert(def->isVar);
                                vec_insert(&curBlock->ins, i, def->var.valType);
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
                u32 t = EXP_evalTypeVar(typeContext, ctx->tvCount++);
                if (!EXP_evalVerifShiftDataStack(ctx, 1, &t))
                {
                    EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                    return;
                }
            }
            u32 t = vec_last(dataStack);
            vec_pop(dataStack);
            if (curCall->dataStackP > dataStack->length + curBlock->ins.length)
            {
                u32 n = curCall->dataStackP - dataStack->length;
                u32 added = n - curBlock->ins.length;
                assert(1 == added);
                vec_insert(&curBlock->ins, 0, t);
            }
            return;
        }
        default:
            break;
        }

        u32 afun = EXP_evalVerifGetAfun(ctx, name);
        if (afun != -1)
        {
            enode->type = EXP_EvalNodeType_Afun;
            enode->afun = afun;
            EXP_EvalAfunInfo* afunInfo = ctx->afunTable->data + afun;
            EXP_evalVerifAfunCall(ctx, afunInfo, node);
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
                EXP_EvalVerifBlock* funBlk = blockTable->data + fun.id;
                if (funBlk->completed)
                {
                    assert(!funBlk->entered);
                    if (dataStack->length < funBlk->numIns)
                    {
                        u32 n = funBlk->numIns - dataStack->length;
                        if (!EXP_evalVerifShiftDataStack(ctx, n, funBlk->inout.data))
                        {
                            EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                            return;
                        }
                    }
                    EXP_evalVerifBlockCall(ctx, funBlk, node);
                    return;
                }
                else if (!funBlk->entered)
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
                    assert(funBlk->entered);
                    if (funBlk->haveInOut)
                    {
                        EXP_evalVerifBlockCall(ctx, funBlk, node);
                    }
                    else
                    {
                        EXP_evalVerifRecurFallbackToOtherBranch(ctx);
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
                for (u32 i = 0; i < ctx->atypeTable->length; ++i)
                {
                    u32 j = ctx->atypeTable->length - 1 - i;
                    if (ctx->atypeTable->data[j].ctorBySym)
                    {
                        u32 l = EXP_tokSize(space, node);
                        const char* s = EXP_tokCstr(space, node);
                        EXP_EvalValue v = { 0 };
                        if (ctx->atypeTable->data[j].ctorBySym(l, s, &v))
                        {
                            enode->type = EXP_EvalNodeType_Value;
                            enode->value = v;
                            u32 t = EXP_evalTypeAtom(ctx->typeContext, j);
                            vec_push(dataStack, t);
                            return;
                        }
                    }
                }
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalUndefined);
                return;
            }
            enode->type = EXP_EvalNodeType_String;
            u32 t = EXP_evalTypeAtom(ctx->typeContext, EXP_EvalPrimType_STRING);
            vec_push(dataStack, t);
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

            EXP_EvalVerifBlock* nodeBlk = EXP_evalVerifGetBlock(ctx, node);
            if (!nodeBlk->completed)
            {
                EXP_EvalVerifBlockCallback cb = { EXP_EvalVerifBlockCallbackType_Call, .fun = def.fun };
                EXP_evalVerifEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
            }
            else
            {
                EXP_evalVerifBlockCall(ctx, nodeBlk, node);
            }
            return;
        }
    }

    u32 afun = EXP_evalVerifGetAfun(ctx, name);
    if (afun != -1)
    {
        enode->type = EXP_EvalNodeType_CallAfun;
        enode->afun = afun;

        EXP_EvalAfunInfo* afunInfo = ctx->afunTable->data + afun;
        assert(afunInfo->call);

        EXP_EvalVerifBlock* nodeBlk = EXP_evalVerifGetBlock(ctx, node);
        if (!nodeBlk->completed)
        {
            EXP_EvalVerifBlockCallback cb = { EXP_EvalVerifBlockCallbackType_Ncall, .afun = afun };
            EXP_evalVerifEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
        }
        else
        {
            EXP_evalVerifBlockCall(ctx, nodeBlk, node);
        }
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
        EXP_EvalVerifBlock* nodeBlk = EXP_evalVerifGetBlock(ctx, node);
        if (!nodeBlk->completed)
        {
            EXP_EvalVerifBlockCallback cb = { EXP_EvalVerifBlockCallbackType_Cond };
            EXP_evalVerifEnterBlock(ctx, elms + 1, 1, node, curCall->srcNode, cb, false);
        }
        else
        {
            EXP_evalVerifBlockCall(ctx, nodeBlk, node);
        }
        return;
    }
    default:
    {
        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
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
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

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
        return;
    }
    curCall = &vec_last(&ctx->callStack);
    curBlock = blockTable->data + curCall->srcNode.id;
    if (curCall->p == curCall->end)
    {
        EXP_EvalVerifBlockCallback* cb = &curCall->cb;
        EXP_Node srcNode = curCall->srcNode;
#ifndef NDEBUG
        EXP_NodeSrcInfo* nodeSrcInfo = ctx->srcInfo->nodes.data + srcNode.id;
#endif
        switch (cb->type)
        {
        case EXP_EvalVerifBlockCallbackType_NONE:
        {
            EXP_evalVerifLeaveBlock(ctx);
            goto next;
        }
        case EXP_EvalVerifBlockCallbackType_Ncall:
        {
            if (curBlock->ins.length > 0)
            {
                EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            if (dataStack->length < curCall->dataStackP)
            {
                EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            EXP_EvalAfunInfo* afunInfo = ctx->afunTable->data + cb->afun;
            u32 numIns = dataStack->length - curCall->dataStackP;
            if (numIns != afunInfo->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            EXP_evalVerifAfunCall(ctx, afunInfo, srcNode);
            EXP_evalVerifLeaveBlock(ctx);
            goto next;
        }
        case EXP_EvalVerifBlockCallbackType_Call:
        {
            EXP_Node fun = cb->fun;
            EXP_EvalVerifBlock* funBlk = blockTable->data + fun.id;
            if (funBlk->completed)
            {
                assert(!funBlk->entered);
                if (curBlock->numIns > 0)
                {
                    EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    goto next;
                }
                if (curCall->dataStackP != (dataStack->length - funBlk->numIns))
                {
                    EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    goto next;
                }
                EXP_evalVerifBlockCall(ctx, funBlk, srcNode);
                EXP_evalVerifLeaveBlock(ctx);
                goto next;
            }
            else if (!funBlk->entered)
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
                assert(funBlk->entered);
                if (funBlk->haveInOut)
                {
                    if (curBlock->numIns > 0)
                    {
                        EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                        goto next;
                    }
                    if (curCall->dataStackP != (dataStack->length - funBlk->numIns))
                    {
                        EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                        goto next;
                    }
                    EXP_evalVerifBlockCall(ctx, funBlk, srcNode);
                    EXP_evalVerifLeaveBlock(ctx);
                }
                else
                {
                    EXP_evalVerifRecurFallbackToOtherBranch(ctx);
                }
                goto next;
            }
            return;
        }
        case EXP_EvalVerifBlockCallbackType_Cond:
        {
            if (curCall->dataStackP + 1 != dataStack->length)
            {
                EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            u32 t1 = dataStack->data[curCall->dataStackP];
            vec_pop(dataStack);
            u32 t;
            if (!EXP_evalVerifTypeUnify(ctx, EXP_EvalPrimType_BOOL, t1, &t))
            {
                EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            dataStack->data[curCall->dataStackP] = t;
            curCall->p = EXP_evalIfBranch0(space, srcNode);
            curCall->end = EXP_evalIfBranch0(space, srcNode) + 1;
            cb->type = EXP_EvalVerifBlockCallbackType_Branch0;
            goto next;
        }
        case EXP_EvalVerifBlockCallbackType_Branch0:
        {
            if (EXP_evalIfHasBranch1(space, srcNode))
            {
                EXP_evalVerifSaveBlock(ctx);
                EXP_evalVerifBlockRevert(ctx);
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
            assert(EXP_evalIfHasBranch1(space, srcNode));
            EXP_evalVerifLeaveBlock(ctx);
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
    EXP_Space* space, EXP_Node root, EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalAtypeInfoTable* atypeTable, EXP_EvalAfunInfoTable* afunTable,
    EXP_EvalNodeTable* nodeTable,
    EXP_EvalTypeContext* typeContext, vec_u32* typeStack
)
{
    EXP_EvalError error = { 0 };
    //return error;
    if (!EXP_isSeq(space, root))
    {
        return error;
    }
    EXP_EvalVerifContext _ctx = EXP_newEvalVerifContext
    (
        space, srcInfo, atypeTable, afunTable, nodeTable, typeContext
    );
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
    EXP_EvalVerifBlock* rootBlk = EXP_evalVerifGetBlock(ctx, root);
    assert(rootBlk->completed);
    vec_dup(typeStack, &ctx->dataStack);
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

















































































































































