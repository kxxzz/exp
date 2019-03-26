#include "exp_eval_a.h"
#include "exp_eval_type_a.h"



typedef struct EXP_EvalCompileVar
{
    u32 valType;
    EXP_Node block;
    u32 id;
} EXP_EvalCompileVar;

typedef struct EXP_EvalCompileDef
{
    EXP_Node key;
    bool isVar;
    union
    {
        EXP_Node fun;
        EXP_EvalCompileVar var;
    };
} EXP_EvalCompileDef;

typedef vec_t(EXP_EvalCompileDef) EXP_EvalCompileDefTable;






typedef struct EXP_EvalCompileBlock
{
    bool completed;
    bool haveInOut;
    bool incomplete;

    bool entered;
    vec_u32 ins;

    EXP_Node parent;
    u32 varsCount;
    EXP_EvalCompileDefTable defs;

    u32 numIns;
    u32 numOuts;
    vec_u32 inout;
} EXP_EvalCompileBlock;

typedef vec_t(EXP_EvalCompileBlock) EXP_EvalCompileBlockTable;

static void EXP_evalCompileBlockFree(EXP_EvalCompileBlock* blk)
{
    vec_free(&blk->inout);
    vec_free(&blk->defs);
    vec_free(&blk->ins);
}





typedef enum EXP_EvalCompileBlockCallbackType
{
    EXP_EvalCompileBlockCallbackType_NONE,
    EXP_EvalCompileBlockCallbackType_Ncall,
    EXP_EvalCompileBlockCallbackType_Call,
    EXP_EvalCompileBlockCallbackType_Cond,
    EXP_EvalCompileBlockCallbackType_Branch0,
    EXP_EvalCompileBlockCallbackType_Branch1,
    EXP_EvalCompileBlockCallbackType_BranchUnify,
} EXP_EvalCompileBlockCallbackType;

typedef struct EXP_EvalCompileBlockCallback
{
    EXP_EvalCompileBlockCallbackType type;
    union
    {
        u32 afun;
        EXP_Node fun;
    };
} EXP_EvalCompileBlockCallback;

static EXP_EvalCompileBlockCallback EXP_EvalBlockCallback_NONE = { EXP_EvalCompileBlockCallbackType_NONE };





typedef struct EXP_EvalCompileCall
{
    EXP_Node srcNode;
    u32 dataStackP;
    EXP_Node* p;
    EXP_Node* end;
    EXP_EvalCompileBlockCallback cb;
} EXP_EvalCompileCall;

typedef vec_t(EXP_EvalCompileCall) EXP_EvalCompileCallVec;




typedef struct EXP_EvalCompileSnapshot
{
    bool allowDsShift;
    u32 dsOff;
    u32 dsLen;
    u32 csOff;
    u32 csLen;
    u32 tvsOff;
    u32 tvsLen;
    u32 typeVarCount;
} EXP_EvalCompileSnapshot;

typedef vec_t(EXP_EvalCompileSnapshot) EXP_EvalCompileWorldStack;



typedef struct EXP_EvalCompileContext
{
    EXP_Space* space;
    EXP_SpaceSrcInfo* srcInfo;
    EXP_EvalAtypeInfoTable* atypeTable;
    EXP_EvalAfunInfoTable* afunTable;
    EXP_EvalNodeTable* nodeTable;
    EXP_EvalTypeContext* typeContext;

    u32 blockTableBase;
    EXP_EvalCompileBlockTable blockTable;

    EXP_EvalTypeVarSpace typeVarSpace;

    bool allowDsShift;
    vec_u32 dataStack;
    EXP_EvalCompileCallVec callStack;

    vec_u32 dsBuf;
    EXP_EvalCompileCallVec csBuf;
    EXP_EvalTypeBvarVec tvBuf;
    EXP_EvalCompileWorldStack worldStack;

    EXP_EvalError error;
    EXP_NodeVec varKeyBuf;
    vec_u32 typeBuf;
    EXP_EvalTypeVarSpace varRenMap;
} EXP_EvalCompileContext;





static EXP_EvalCompileContext EXP_newEvalVerifContext
(
    EXP_Space* space, EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalAtypeInfoTable* atypeTable, EXP_EvalAfunInfoTable* afunTable,
    EXP_EvalNodeTable* nodeTable,
    EXP_EvalTypeContext* typeContext
)
{
    EXP_EvalCompileContext _ctx = { 0 };
    EXP_EvalCompileContext* ctx = &_ctx;
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
    memset(ctx->blockTable.data, 0, sizeof(EXP_EvalCompileBlock)*ctx->blockTable.length);

    ctx->varRenMap.varCount = -1;
    return *ctx;
}

static void EXP_evalCompileContextFree(EXP_EvalCompileContext* ctx)
{
    EXP_evalTypeVarSpaceFree(&ctx->varRenMap);
    vec_free(&ctx->typeBuf);
    vec_free(&ctx->varKeyBuf);

    vec_free(&ctx->worldStack);
    vec_free(&ctx->csBuf);
    vec_free(&ctx->dsBuf);

    vec_free(&ctx->callStack);
    vec_free(&ctx->dataStack);

    EXP_evalTypeVarSpaceFree(&ctx->typeVarSpace);

    for (u32 i = 0; i < ctx->blockTable.length; ++i)
    {
        EXP_EvalCompileBlock* b = ctx->blockTable.data + i;
        EXP_evalCompileBlockFree(b);
    }
    vec_free(&ctx->blockTable);
}








static void EXP_evalCompilePushWorld(EXP_EvalCompileContext* ctx, bool allowDsShift)
{
    EXP_EvalCompileSnapshot snapshot = { 0 };
    snapshot.allowDsShift = ctx->allowDsShift;
    snapshot.dsOff = ctx->dsBuf.length;
    snapshot.dsLen = ctx->dataStack.length;
    snapshot.csOff = ctx->csBuf.length;
    snapshot.csLen = ctx->callStack.length;
    snapshot.tvsOff = ctx->tvBuf.length;
    snapshot.tvsLen = ctx->typeVarSpace.bvars.length;
    snapshot.typeVarCount = ctx->typeVarSpace.varCount;
    vec_push(&ctx->worldStack, snapshot);

    vec_concat(&ctx->dsBuf, &ctx->dataStack);
    vec_concat(&ctx->csBuf, &ctx->callStack);
    vec_concat(&ctx->tvBuf, &ctx->typeVarSpace.bvars);
    ctx->allowDsShift = allowDsShift;
    ctx->dataStack.length = 0;
    ctx->callStack.length = 0;
    ctx->typeVarSpace.bvars.length = 0;
    ctx->typeVarSpace.varCount = 0;
}


static void EXP_evalCompilePopWorld(EXP_EvalCompileContext* ctx)
{
    assert(0 == ctx->callStack.length);
    assert(ctx->worldStack.length > 0);
    EXP_EvalCompileSnapshot snapshot = vec_last(&ctx->worldStack);
    vec_pop(&ctx->worldStack);

    ctx->allowDsShift = snapshot.allowDsShift;
    ctx->dataStack.length = 0;
    ctx->typeVarSpace.bvars.length = 0;
    vec_pusharr(&ctx->dataStack, ctx->dsBuf.data + snapshot.dsOff, snapshot.dsLen);
    vec_pusharr(&ctx->callStack, ctx->csBuf.data + snapshot.csOff, snapshot.csLen);
    vec_pusharr(&ctx->typeVarSpace.bvars, ctx->tvBuf.data + snapshot.tvsOff, snapshot.tvsLen);
    vec_resize(&ctx->dsBuf, snapshot.dsOff);
    vec_resize(&ctx->csBuf, snapshot.csOff);
    vec_resize(&ctx->tvBuf, snapshot.tvsOff);
    ctx->typeVarSpace.varCount = snapshot.typeVarCount;
}





static u32 EXP_evalCompileTypeNorm(EXP_EvalCompileContext* ctx, u32 x)
{
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;
    return EXP_evalTypeReduct(typeContext, typeVarSpace, x);
}

static bool EXP_evalCompileTypeUnify(EXP_EvalCompileContext* ctx, u32 a, u32 b, u32* pU)
{
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;
    return EXP_evalTypeUnify(typeContext, typeVarSpace, a, b, pU);
}



static bool EXP_evalCompileTypeUnifyPat(EXP_EvalCompileContext* ctx, u32 x, u32 pat, u32* pU)
{
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;
    EXP_EvalTypeVarSpace* varRenMap = &ctx->varRenMap;
    return EXP_evalTypeUnifyPat(typeContext, typeVarSpace, x, varRenMap, pat, pU);
}

static u32 EXP_evalCompileTypeFromPat(EXP_EvalCompileContext* ctx, u32 x)
{
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;
    EXP_EvalTypeVarSpace* varRenMap = &ctx->varRenMap;
    return EXP_evalTypeFromPat(typeContext, typeVarSpace, varRenMap, x);
}








static void EXP_evalCompileErrorAtNode(EXP_EvalCompileContext* ctx, EXP_Node node, EXP_EvalErrCode errCode)
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










static EXP_EvalCompileBlock* EXP_evalCompileGetBlock(EXP_EvalCompileContext* ctx, EXP_Node node)
{
    assert(node.id >= ctx->blockTableBase);
    EXP_EvalCompileBlock* b = ctx->blockTable.data + node.id - ctx->blockTableBase;
    return b;
}





static bool EXP_evalCompileGetMatched
(
    EXP_EvalCompileContext* ctx, const char* name, EXP_Node blkNode, EXP_EvalCompileDef* outDef
)
{
    EXP_Space* space = ctx->space;
    while (blkNode.id != EXP_NodeId_Invalid)
    {
        EXP_EvalCompileBlock* blk = EXP_evalCompileGetBlock(ctx, blkNode);
        for (u32 i = 0; i < blk->defs.length; ++i)
        {
            EXP_EvalCompileDef* def = blk->defs.data + blk->defs.length - 1 - i;
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




static u32 EXP_evalCompileGetAfun(EXP_EvalCompileContext* ctx, const char* name)
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




static EXP_EvalKey EXP_evalCompileGetKey(EXP_EvalCompileContext* ctx, const char* name)
{
    for (EXP_EvalKey i = 0; i < EXP_NumEvalKeys; ++i)
    {
        EXP_EvalKey k = EXP_NumEvalKeys - 1 - i;
        const char* s = EXP_EvalKeyNameTable()[k];
        if (0 == strcmp(s, name))
        {
            return k;
        }
    }
    return -1;
}





static void EXP_evalCompileDefGetBody(EXP_EvalCompileContext* ctx, EXP_Node node, u32* pLen, EXP_Node** pSeq)
{
    EXP_Space* space = ctx->space;
    assert(EXP_seqLen(space, node) >= 2);
    *pLen = EXP_seqLen(space, node) - 2;
    EXP_Node* defCall = EXP_seqElm(space, node);
    *pSeq = defCall + 2;
}







static bool EXP_evalCompileIsFunDef(EXP_EvalCompileContext* ctx, EXP_Node node)
{
    EXP_Space* space = ctx->space;
    if (EXP_isTok(space, node))
    {
        return false;
    }
    if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return false;
    }
    EXP_Node* defCall = EXP_seqElm(space, node);
    const char* kDef = EXP_tokCstr(space, defCall[0]);
    EXP_EvalKey k = EXP_evalCompileGetKey(ctx, kDef);
    if (k != EXP_EvalKey_Def)
    {
        return false;
    }
    if (!EXP_isTok(space, defCall[1]))
    {
        EXP_evalCompileErrorAtNode(ctx, defCall[1], EXP_EvalErrCode_EvalSyntax);
        return false;
    }
    return true;
}










static void EXP_evalCompileLoadDef(EXP_EvalCompileContext* ctx, EXP_Node node, EXP_EvalCompileBlock* blk)
{
    if (!EXP_evalCompileIsFunDef(ctx, node))
    {
        return;
    }
    EXP_Space* space = ctx->space;
    EXP_Node* defCall = EXP_seqElm(space, node);
    EXP_Node name = defCall[1];
    EXP_EvalCompileDef def = { name, false, .fun = node };
    vec_push(&blk->defs, def);
}









static void EXP_evalCompileSetCallersIncomplete(EXP_EvalCompileContext* ctx)
{
    for (u32 i = 0; i < ctx->callStack.length; ++i)
    {
        u32 j = ctx->callStack.length - 1 - i;
        EXP_EvalCompileCall* call = ctx->callStack.data + j;
        EXP_EvalCompileBlock* blk = EXP_evalCompileGetBlock(ctx, call->srcNode);
        if (blk->incomplete)
        {
            break;
        }
        blk->incomplete = true;
    }
}









static void EXP_evalCompileEnterBlock
(
    EXP_EvalCompileContext* ctx, EXP_Node* seq, u32 len, EXP_Node srcNode, EXP_Node parent,
    EXP_EvalCompileBlockCallback cb, bool isDefScope
)
{
    u32 dataStackP = ctx->dataStack.length;
    EXP_EvalCompileCall call = { srcNode, dataStackP, seq, seq + len, cb };
    vec_push(&ctx->callStack, call);

    EXP_EvalCompileBlock* blk = EXP_evalCompileGetBlock(ctx, srcNode);
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
            EXP_evalCompileLoadDef(ctx, seq[i], blk);
            if (ctx->error.code)
            {
                return;
            }
        }
    }
}



static void EXP_evalCompileSaveBlock(EXP_EvalCompileContext* ctx)
{
    EXP_EvalCompileCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalCompileBlock* curBlock = EXP_evalCompileGetBlock(ctx, curCall->srcNode);

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
            t = EXP_evalCompileTypeNorm(ctx, t);
            vec_push(&curBlock->inout, t);
        }

        assert(ctx->dataStack.length + curBlock->ins.length >= curCall->dataStackP);
        curBlock->numOuts = ctx->dataStack.length + curBlock->ins.length - curCall->dataStackP;

        for (u32 i = 0; i < curBlock->numOuts; ++i)
        {
            u32 j = ctx->dataStack.length - curBlock->numOuts + i;
            u32 t = ctx->dataStack.data[j];
            t = EXP_evalCompileTypeNorm(ctx, t);
            vec_push(&curBlock->inout, t);
        }
    }
    else
    {
        if (curBlock->numIns != curBlock->ins.length)
        {
            EXP_evalCompileErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalUnification);
            return;
        }
        assert(ctx->dataStack.length + curBlock->ins.length >= curCall->dataStackP);
        u32 numOuts = ctx->dataStack.length + curBlock->ins.length - curCall->dataStackP;
        if (curBlock->numOuts != numOuts)
        {
            EXP_evalCompileErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalUnification);
            return;
        }

        for (u32 i = 0; i < curBlock->ins.length; ++i)
        {
            u32 a = curBlock->inout.data[i];
            u32 b = curBlock->ins.data[i];
            u32 u;
            EXP_evalCompileTypeUnify(ctx, a, b, &u);
            curBlock->inout.data[i] = u;
        }

        curBlock->numOuts = numOuts;

        for (u32 i = 0; i < curBlock->numOuts; ++i)
        {
            u32 a = curBlock->inout.data[curBlock->numIns + i];
            u32 j = ctx->dataStack.length - curBlock->numOuts + i;
            u32 b = ctx->dataStack.data[j];
            u32 u;
            EXP_evalCompileTypeUnify(ctx, a, b, &u);
            curBlock->inout.data[curBlock->numIns + i] = u;
            ctx->dataStack.data[j] = u;
        }
    }
}


static void EXP_evalCompileLeaveBlock(EXP_EvalCompileContext* ctx)
{
    EXP_EvalCompileCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalCompileBlock* curBlock = EXP_evalCompileGetBlock(ctx, curCall->srcNode);

    EXP_evalCompileSaveBlock(ctx);

    curBlock->entered = false;
    curBlock->completed = !curBlock->incomplete;
    vec_pop(&ctx->callStack);

    if (!curBlock->completed)
    {
        EXP_evalCompileSetCallersIncomplete(ctx);
    }
}




static void EXP_evalCompileBlockRevert(EXP_EvalCompileContext* ctx)
{
    EXP_EvalCompileCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalCompileBlock* curBlock = EXP_evalCompileGetBlock(ctx, curCall->srcNode);

    assert(curBlock->entered);
    assert(curCall->dataStackP >= curBlock->ins.length);

    ctx->dataStack.length = curCall->dataStackP - curBlock->ins.length;
    for (u32 i = 0; i < curBlock->ins.length; ++i)
    {
        vec_push(&ctx->dataStack, curBlock->ins.data[i]);
    }
}




static void EXP_evalCompileCancelBlock(EXP_EvalCompileContext* ctx)
{
    EXP_evalCompileBlockRevert(ctx);

    EXP_EvalCompileCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalCompileBlock* curBlock = EXP_evalCompileGetBlock(ctx, curCall->srcNode);

    curBlock->entered = false;
    curBlock->ins.length = 0;

    vec_pop(&ctx->callStack);
}






static bool EXP_evalCompileShiftDataStack(EXP_EvalCompileContext* ctx, u32 n, const u32* a)
{
    if (!ctx->allowDsShift)
    {
        return false;
    }
    vec_u32* dataStack = &ctx->dataStack;
    vec_insertarr(dataStack, 0, a, n);
    for (u32 i = 0; i < ctx->callStack.length; ++i)
    {
        EXP_EvalCompileCall* c = ctx->callStack.data + i;
        c->dataStackP += n;
    }
    return true;
}









static void EXP_evalCompileCurBlockInsUpdate(EXP_EvalCompileContext* ctx, u32 argsOffset, const u32* funInTypes)
{
    EXP_EvalCompileCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalCompileBlock* curBlock = EXP_evalCompileGetBlock(ctx, curCall->srcNode);
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






static void EXP_evalCompileAfunCall(EXP_EvalCompileContext* ctx, EXP_EvalAfunInfo* afunInfo, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    if (!afunInfo->call)
    {
        EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
        return;
    }

    u32 inEvalType[EXP_EvalAfunIns_MAX];
    for (u32 i = 0; i < afunInfo->numIns; ++i)
    {
        inEvalType[i] = EXP_evalTypeAtom(typeContext, afunInfo->inAtype[i]);
    }

    if (dataStack->length < afunInfo->numIns)
    {
        u32 n = afunInfo->numIns - dataStack->length;
        if (!EXP_evalCompileShiftDataStack(ctx, n, inEvalType))
        {
            EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
    }

    assert(dataStack->length >= afunInfo->numIns);
    u32 argsOffset = dataStack->length - afunInfo->numIns;
    EXP_evalCompileCurBlockInsUpdate(ctx, argsOffset, inEvalType);

    for (u32 i = 0; i < afunInfo->numIns; ++i)
    {
        u32 a = dataStack->data[argsOffset + i];
        u32 b = inEvalType[i];
        u32 u;
        if (!EXP_evalCompileTypeUnify(ctx, a, b, &u))
        {
            EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < afunInfo->numOuts; ++i)
    {
        u32 x = EXP_evalTypeAtom(ctx->typeContext, afunInfo->outAtype[i]);
        vec_push(dataStack, x);
    }
}




static void EXP_evalCompileBlockCall(EXP_EvalCompileContext* ctx, const EXP_EvalCompileBlock* blk, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeVarSpace* varRenMap = &ctx->varRenMap;

    assert(blk->haveInOut);
    assert(dataStack->length >= blk->numIns);
    u32 argsOffset = dataStack->length - blk->numIns;
    EXP_evalCompileCurBlockInsUpdate(ctx, argsOffset, blk->inout.data);

    vec_resize(&varRenMap->bvars, 0);

    assert((blk->numIns + blk->numOuts) == blk->inout.length);
    for (u32 i = 0; i < blk->numIns; ++i)
    {
        u32 a = dataStack->data[argsOffset + i];
        u32 b = blk->inout.data[i];
        u32 u;
        if (!EXP_evalCompileTypeUnifyPat(ctx, a, b, &u))
        {
            EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < blk->numOuts; ++i)
    {
        u32 x = blk->inout.data[blk->numIns + i];
        x = EXP_evalCompileTypeFromPat(ctx, x);
        vec_push(dataStack, x);
    }
}













static void EXP_evalCompileRecurFallbackToOtherBranch(EXP_EvalCompileContext* ctx)
{
    EXP_Space* space = ctx->space;

    EXP_EvalCompileCall* curCall = &vec_last(&ctx->callStack);
    EXP_Node lastSrcNode = EXP_Node_Invalid;
    while (ctx->callStack.length > 0)
    {
        lastSrcNode = curCall->srcNode;
        curCall = &vec_last(&ctx->callStack);
        EXP_EvalCompileBlock* curBlock = EXP_evalCompileGetBlock(ctx, curCall->srcNode);
        EXP_Node srcNode = curCall->srcNode;
        EXP_EvalCompileBlockCallback* cb = &curCall->cb;
        // quit this branch until next enter
        if (EXP_EvalCompileBlockCallbackType_Branch0 == cb->type)
        {
            if (EXP_evalIfHasBranch1(space, srcNode))
            {
                curBlock->incomplete = true;
                EXP_evalCompileBlockRevert(ctx);
                curCall->p = EXP_evalIfBranch1(space, srcNode);
                curCall->end = EXP_evalIfBranch1(space, srcNode) + 1;
                cb->type = EXP_EvalCompileBlockCallbackType_NONE;
                return;
            }
        }
        else if (EXP_EvalCompileBlockCallbackType_BranchUnify == cb->type)
        {
            curBlock->incomplete = true;
            EXP_evalCompileBlockRevert(ctx);
            curCall->p = EXP_evalIfBranch0(space, srcNode);
            curCall->end = EXP_evalIfBranch0(space, srcNode) + 1;
            cb->type = EXP_EvalCompileBlockCallbackType_NONE;
            return;
        }
        EXP_evalCompileCancelBlock(ctx);
    }
    EXP_evalCompileErrorAtNode(ctx, lastSrcNode, EXP_EvalErrCode_EvalRecurNoBaseCase);
}













static void EXP_evalCompileEnterWorld
(
    EXP_EvalCompileContext* ctx, EXP_Node* seq, u32 len, EXP_Node src, EXP_Node parent, bool allowDsShift
)
{
    EXP_evalCompilePushWorld(ctx, allowDsShift);
    EXP_evalCompileEnterBlock(ctx, seq, len, src, parent, EXP_EvalBlockCallback_NONE, true);
}






static void EXP_evalCompileNode
(
    EXP_EvalCompileContext* ctx, EXP_Node node, EXP_EvalCompileCall* curCall, EXP_EvalCompileBlock* curBlock
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalNodeTable* nodeTable = ctx->nodeTable;
    EXP_EvalCompileBlockTable* blockTable = &ctx->blockTable;
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    EXP_EvalNode* enode = nodeTable->data + node.id;
    if (EXP_isTok(space, node))
    {
        if (EXP_tokQuoted(space, node))
        {
            enode->type = EXP_EvalNodeType_String;
            u32 t = EXP_evalTypeAtom(ctx->typeContext, EXP_EvalPrimType_STRING);
            vec_push(dataStack, t);
            return;
        }
        else
        {
            const char* name = EXP_tokCstr(space, node);

            EXP_EvalKey k = EXP_evalCompileGetKey(ctx, name);
            switch (k)
            {
            case EXP_EvalKey_VarDefBegin:
            {
                enode->type = EXP_EvalNodeType_VarDefBegin;
                if (curCall->cb.type != EXP_EvalCompileBlockCallbackType_NONE)
                {
                    EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                ctx->varKeyBuf.length = 0;
                for (u32 n = 0;;)
                {
                    if (curCall->p == curCall->end)
                    {
                        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
                        return;
                    }
                    node = *(curCall->p++);
                    enode = nodeTable->data + node.id;
                    const char* skey = EXP_tokCstr(space, node);
                    if (!EXP_isTok(space, node))
                    {
                        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    EXP_EvalKey k = EXP_evalCompileGetKey(ctx, EXP_tokCstr(space, node));
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
                                    u32 var = EXP_evalTypeNewVar(&ctx->typeVarSpace);
                                    u32 t = EXP_evalTypeVar(typeContext, var);
                                    vec_push(&ctx->typeBuf, t);
                                }
                                if (!EXP_evalCompileShiftDataStack(ctx, shiftN, ctx->typeBuf.data))
                                {
                                    EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                                    return;
                                }
                            }

                            u32 off = dataStack->length - n;
                            for (u32 i = 0; i < n; ++i)
                            {
                                u32 vt = dataStack->data[off + i];
                                EXP_EvalCompileVar var = { vt, curCall->srcNode.id, curBlock->varsCount };
                                EXP_EvalCompileDef def = { ctx->varKeyBuf.data[i], true, .var = var };
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
                                    EXP_EvalCompileDef* def = curBlock->defs.data + curBlock->defs.length - added + i;
                                    assert(def->isVar);
                                    vec_insert(&curBlock->ins, i, def->var.valType);
                                }
                            }
                            return;
                        }
                        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
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
                    u32 var = EXP_evalTypeNewVar(&ctx->typeVarSpace);
                    u32 t = EXP_evalTypeVar(typeContext, var);
                    if (!EXP_evalCompileShiftDataStack(ctx, 1, &t))
                    {
                        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
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

            u32 afun = EXP_evalCompileGetAfun(ctx, name);
            if (afun != -1)
            {
                enode->type = EXP_EvalNodeType_Afun;
                enode->afun = afun;
                EXP_EvalAfunInfo* afunInfo = ctx->afunTable->data + afun;
                EXP_evalCompileAfunCall(ctx, afunInfo, node);
                return;
            }

            EXP_EvalCompileDef def = { 0 };
            if (EXP_evalCompileGetMatched(ctx, name, curCall->srcNode, &def))
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
                    EXP_EvalCompileBlock* funBlk = blockTable->data + fun.id;
                    if (funBlk->completed)
                    {
                        assert(!funBlk->entered);
                        if (dataStack->length < funBlk->numIns)
                        {
                            u32 n = funBlk->numIns - dataStack->length;
                            if (!EXP_evalCompileShiftDataStack(ctx, n, funBlk->inout.data))
                            {
                                EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                                return;
                            }
                        }
                        EXP_evalCompileBlockCall(ctx, funBlk, node);
                        return;
                    }
                    else if (!funBlk->entered)
                    {
                        u32 bodyLen = 0;
                        EXP_Node* body = NULL;
                        EXP_evalCompileDefGetBody(ctx, fun, &bodyLen, &body);

                        --curCall->p;
                        EXP_evalCompileEnterWorld(ctx, body, bodyLen, fun, curCall->srcNode, true);
                        return;
                    }
                    else
                    {
                        assert(funBlk->entered);
                        if (funBlk->haveInOut)
                        {
                            EXP_evalCompileBlockCall(ctx, funBlk, node);
                        }
                        else
                        {
                            EXP_evalCompileRecurFallbackToOtherBranch(ctx);
                        }
                        return;
                    }
                }
            }
            else
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
                EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalUndefined);
                return;
            }
        }
    }

    if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }

    EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);
    const char* name = EXP_tokCstr(space, elms[0]);

    EXP_EvalCompileDef def = { 0 };
    if (EXP_evalCompileGetMatched(ctx, name, curCall->srcNode, &def))
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

            EXP_EvalCompileBlock* nodeBlk = EXP_evalCompileGetBlock(ctx, node);
            if (!nodeBlk->completed)
            {
                EXP_EvalCompileBlockCallback cb = { EXP_EvalCompileBlockCallbackType_Call, .fun = def.fun };
                EXP_evalCompileEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
            }
            else
            {
                EXP_evalCompileBlockCall(ctx, nodeBlk, node);
            }
            return;
        }
    }

    u32 afun = EXP_evalCompileGetAfun(ctx, name);
    if (afun != -1)
    {
        enode->type = EXP_EvalNodeType_CallAfun;
        enode->afun = afun;

        EXP_EvalAfunInfo* afunInfo = ctx->afunTable->data + afun;
        assert(afunInfo->call);

        EXP_EvalCompileBlock* nodeBlk = EXP_evalCompileGetBlock(ctx, node);
        if (!nodeBlk->completed)
        {
            EXP_EvalCompileBlockCallback cb = { EXP_EvalCompileBlockCallbackType_Ncall, .afun = afun };
            EXP_evalCompileEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
        }
        else
        {
            EXP_evalCompileBlockCall(ctx, nodeBlk, node);
        }
        return;
    }

    EXP_EvalKey k = EXP_evalCompileGetKey(ctx, name);
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
            EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
            return;
        }
        EXP_EvalCompileBlock* nodeBlk = EXP_evalCompileGetBlock(ctx, node);
        if (!nodeBlk->completed)
        {
            EXP_EvalCompileBlockCallback cb = { EXP_EvalCompileBlockCallbackType_Cond };
            EXP_evalCompileEnterBlock(ctx, elms + 1, 1, node, curCall->srcNode, cb, false);
        }
        else
        {
            EXP_evalCompileBlockCall(ctx, nodeBlk, node);
        }
        return;
    }
    default:
    {
        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }
    }
}













static void EXP_evalCompileCall(EXP_EvalCompileContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalCompileCall* curCall;
    EXP_EvalCompileBlockTable* blockTable = &ctx->blockTable;
    EXP_EvalCompileBlock* curBlock = NULL;
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
            EXP_evalCompilePopWorld(ctx);
            goto next;
        }
        return;
    }
    curCall = &vec_last(&ctx->callStack);
    curBlock = blockTable->data + curCall->srcNode.id;
    if (curCall->p == curCall->end)
    {
        EXP_EvalCompileBlockCallback* cb = &curCall->cb;
        EXP_Node srcNode = curCall->srcNode;
#ifndef NDEBUG
        EXP_NodeSrcInfo* nodeSrcInfo = ctx->srcInfo->nodes.data + srcNode.id;
#endif
        switch (cb->type)
        {
        case EXP_EvalCompileBlockCallbackType_NONE:
        {
            EXP_evalCompileLeaveBlock(ctx);
            goto next;
        }
        case EXP_EvalCompileBlockCallbackType_Ncall:
        {
            if (curBlock->ins.length > 0)
            {
                EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            if (dataStack->length < curCall->dataStackP)
            {
                EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            EXP_EvalAfunInfo* afunInfo = ctx->afunTable->data + cb->afun;
            u32 numIns = dataStack->length - curCall->dataStackP;
            if (numIns != afunInfo->numIns)
            {
                EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            EXP_evalCompileAfunCall(ctx, afunInfo, srcNode);
            EXP_evalCompileLeaveBlock(ctx);
            goto next;
        }
        case EXP_EvalCompileBlockCallbackType_Call:
        {
            EXP_Node fun = cb->fun;
            EXP_EvalCompileBlock* funBlk = blockTable->data + fun.id;
            if (funBlk->completed)
            {
                assert(!funBlk->entered);
                if (curBlock->numIns > 0)
                {
                    EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    goto next;
                }
                if (curCall->dataStackP != (dataStack->length - funBlk->numIns))
                {
                    EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    goto next;
                }
                EXP_evalCompileBlockCall(ctx, funBlk, srcNode);
                EXP_evalCompileLeaveBlock(ctx);
                goto next;
            }
            else if (!funBlk->entered)
            {
                if (curCall->dataStackP > dataStack->length)
                {
                    EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    goto next;
                }
                u32 bodyLen = 0;
                EXP_Node* body = NULL;
                EXP_evalCompileDefGetBody(ctx, fun, &bodyLen, &body);
                EXP_evalCompileEnterWorld(ctx, body, bodyLen, fun, srcNode, true);
                goto next;
            }
            else
            {
                assert(funBlk->entered);
                if (funBlk->haveInOut)
                {
                    if (curBlock->numIns > 0)
                    {
                        EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                        goto next;
                    }
                    if (curCall->dataStackP != (dataStack->length - funBlk->numIns))
                    {
                        EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                        goto next;
                    }
                    EXP_evalCompileBlockCall(ctx, funBlk, srcNode);
                    EXP_evalCompileLeaveBlock(ctx);
                }
                else
                {
                    EXP_evalCompileRecurFallbackToOtherBranch(ctx);
                }
                goto next;
            }
            return;
        }
        case EXP_EvalCompileBlockCallbackType_Cond:
        {
            if (curCall->dataStackP + 1 != dataStack->length)
            {
                EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            u32 a = EXP_evalTypeAtom(typeContext, EXP_EvalPrimType_BOOL);
            u32 b = dataStack->data[curCall->dataStackP];
            vec_pop(dataStack);
            u32 u;
            if (!EXP_evalCompileTypeUnify(ctx, a, b, &u))
            {
                EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            dataStack->data[curCall->dataStackP] = u;
            curCall->p = EXP_evalIfBranch0(space, srcNode);
            curCall->end = EXP_evalIfBranch0(space, srcNode) + 1;
            cb->type = EXP_EvalCompileBlockCallbackType_Branch0;
            goto next;
        }
        case EXP_EvalCompileBlockCallbackType_Branch0:
        {
            if (EXP_evalIfHasBranch1(space, srcNode))
            {
                EXP_evalCompileSaveBlock(ctx);
                EXP_evalCompileBlockRevert(ctx);
                curCall->p = EXP_evalIfBranch1(space, srcNode);
                curCall->end = EXP_evalIfBranch1(space, srcNode) + 1;
                cb->type = EXP_EvalCompileBlockCallbackType_BranchUnify;
                goto next;
            }
            EXP_evalCompileLeaveBlock(ctx);
            goto next;
        }
        case EXP_EvalCompileBlockCallbackType_BranchUnify:
        {
            assert(EXP_evalIfHasBranch1(space, srcNode));
            EXP_evalCompileLeaveBlock(ctx);
            goto next;
        }
        default:
            assert(false);
            return;
        }
        return;
    }
    EXP_Node node = *(curCall->p++);
    EXP_evalCompileNode(ctx, node, curCall, curBlock);
    goto next;
}



























EXP_EvalError EXP_evalCompile
(
    EXP_Space* space, EXP_Node root, EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalAtypeInfoTable* atypeTable, EXP_EvalAfunInfoTable* afunTable,
    EXP_EvalNodeTable* nodeTable,
    EXP_EvalTypeContext* typeContext, vec_u32* typeStack
)
{
    EXP_EvalError error = { 0 };
    if (!EXP_isSeq(space, root))
    {
        return error;
    }
    EXP_EvalCompileContext _ctx = EXP_newEvalVerifContext
    (
        space, srcInfo, atypeTable, afunTable, nodeTable, typeContext
    );
    EXP_EvalCompileContext* ctx = &_ctx;

    ctx->allowDsShift = false;
    vec_dup(&ctx->dataStack, typeStack);

    EXP_Node* seq = EXP_seqElm(space, root);
    u32 len = EXP_seqLen(space, root);
    EXP_evalCompileEnterBlock(ctx, seq, len, root, EXP_Node_Invalid, EXP_EvalBlockCallback_NONE, true);
    if (ctx->error.code)
    {
        error = ctx->error;
        EXP_evalCompileContextFree(ctx);
        return error;
    }
    EXP_evalCompileCall(ctx);
    EXP_EvalCompileBlock* rootBlk = EXP_evalCompileGetBlock(ctx, root);
    assert(rootBlk->completed);
    vec_dup(typeStack, &ctx->dataStack);
    if (!ctx->error.code)
    {
        for (u32 i = 0; i < ctx->blockTable.length; ++i)
        {
            EXP_EvalCompileBlock* vb = ctx->blockTable.data + i;
            EXP_EvalNode* enode = nodeTable->data + i + ctx->blockTableBase;
            for (u32 i = 0; i < vb->defs.length; ++i)
            {
                EXP_EvalCompileDef* vdef = vb->defs.data + i;
                if (vdef->isVar)
                {
                    ++enode->varsCount;
                }
            }
            assert(enode->varsCount == vb->varsCount);
        }
    }
    error = ctx->error;
    EXP_evalCompileContextFree(ctx);
    return error;
}

















































































































































