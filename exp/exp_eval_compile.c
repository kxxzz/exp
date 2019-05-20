#include "exp_eval_a.h"
#include "exp_eval_type_a.h"
#include "exp_eval_typedecl.h"



typedef struct EXP_EvalCompileVar
{
    u32 valType;
    EXP_Node block;
    u32 id;
} EXP_EvalCompileVar;

typedef struct EXP_EvalCompileNamed
{
    EXP_Node name;
    bool isVar;
    union
    {
        EXP_Node blkSrc;
        EXP_EvalCompileVar var;
    };
} EXP_EvalCompileNamed;

typedef vec_t(EXP_EvalCompileNamed) EXP_EvalCompileNamedTable;






typedef struct EXP_EvalCompileBlock
{
    bool completed;
    bool haveInOut;
    bool incomplete;

    bool entered;
    vec_u32 ins;

    EXP_Node parent;
    u32 varsCount;
    EXP_EvalCompileNamedTable dict;

    u32 numIns;
    u32 numOuts;
    u32 funType;
} EXP_EvalCompileBlock;

typedef vec_t(EXP_EvalCompileBlock) EXP_EvalCompileBlockTable;

static void EXP_evalCompileBlockFree(EXP_EvalCompileBlock* blk)
{
    vec_free(&blk->dict);
    vec_free(&blk->ins);
}





typedef enum EXP_EvalCompileBlockCallbackType
{
    EXP_EvalCompileBlockCallbackType_NONE,
    EXP_EvalCompileBlockCallbackType_Ncall,
    EXP_EvalCompileBlockCallbackType_CallBlock,
    EXP_EvalCompileBlockCallbackType_CallType,
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
        EXP_Node blkSrc;
        u32 funType;
    };
} EXP_EvalCompileBlockCallback;

static EXP_EvalCompileBlockCallback EXP_EvalBlockCallback_NONE = { EXP_EvalCompileBlockCallbackType_NONE };





typedef struct EXP_EvalCompileCall
{
    EXP_Node srcNode;
    u32 dataStackP;
    const EXP_Node* p;
    const EXP_Node* end;
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



typedef struct EXP_EvalCompileVarFetch
{
    EXP_Node name;
    bool hasTypeSignature;
    u32 type;
} EXP_EvalCompileVarFetch;

typedef vec_t(EXP_EvalCompileVarFetch) EXP_EvalCompileVarFetchBuf;










typedef struct EXP_EvalCompileContext
{
    EXP_Space* space;
    EXP_SpaceSrcInfo* srcInfo;
    EXP_EvalAtypeInfoVec* atypeTable;
    EXP_EvalAfunInfoVec* afunTable;
    vec_u32* afunTypeTable;
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
    EXP_EvalCompileVarFetchBuf varFetchBuf;
    vec_u32 typeBuf;
    EXP_EvalTypeVarSpace varRenMap;
    EXP_EvalCompileTypeDeclStack typeDeclStack;
} EXP_EvalCompileContext;





static EXP_EvalCompileContext EXP_newEvalCompileContext
(
    EXP_Space* space, EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalAtypeInfoVec* atypeTable, EXP_EvalAfunInfoVec* afunTable, vec_u32* afunTypeTable,
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
    ctx->afunTypeTable = afunTypeTable;
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
    EXP_evalCompileTypeDeclStackFree(&ctx->typeDeclStack);
    EXP_evalTypeVarSpaceFree(&ctx->varRenMap);
    vec_free(&ctx->typeBuf);
    vec_free(&ctx->varFetchBuf);

    vec_free(&ctx->worldStack);
    vec_free(&ctx->tvBuf);
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







static bool EXP_evalCompileTypeUnify(EXP_EvalCompileContext* ctx, u32 a, u32 b, u32* pU)
{
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;
    return EXP_evalTypeUnify(typeContext, typeVarSpace, a, b, pU);
}



static bool EXP_evalCompileTypeUnifyPat(EXP_EvalCompileContext* ctx, u32 a, u32 pat, u32* pU)
{
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;
    EXP_EvalTypeVarSpace* varRenMap = &ctx->varRenMap;
    return EXP_evalTypeUnifyPat(typeContext, typeVarSpace, a, varRenMap, pat, pU);
}

static void EXP_evalCompilePatContextReset(EXP_EvalCompileContext* ctx)
{
    EXP_EvalTypeVarSpace* varRenMap = &ctx->varRenMap;
    vec_resize(&varRenMap->bvars, 0);
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
    EXP_evalErrorFound(&ctx->error, ctx->srcInfo, errCode, node);
}










static EXP_EvalCompileBlock* EXP_evalCompileGetBlock(EXP_EvalCompileContext* ctx, EXP_Node node)
{
    assert(node.id >= ctx->blockTableBase);
    EXP_EvalCompileBlock* b = ctx->blockTable.data + node.id - ctx->blockTableBase;
    return b;
}





static EXP_EvalCompileNamed* EXP_evalCompileGetMatched(EXP_EvalCompileContext* ctx, EXP_Node name, EXP_Node blkNode)
{
    EXP_Space* space = ctx->space;
    while (blkNode.id != EXP_NodeId_Invalid)
    {
        EXP_EvalCompileBlock* blk = EXP_evalCompileGetBlock(ctx, blkNode);
        for (u32 i = 0; i < blk->dict.length; ++i)
        {
            EXP_EvalCompileNamed* named = blk->dict.data + blk->dict.length - 1 - i;
            if (EXP_nodeDataEq(space, name, named->name))
            {
                return named;
            }
        }
        blkNode = blk->parent;
    }
    return NULL;
}




static u32 EXP_evalCompileGetAfun(EXP_EvalCompileContext* ctx, const char* name)
{
    EXP_Space* space = ctx->space;
    EXP_EvalAfunInfoVec* afunTable = ctx->afunTable;
    for (u32 i = 0; i < afunTable->length; ++i)
    {
        u32 idx = afunTable->length - 1 - i;
        const char* s = afunTable->data[idx].name;
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




static void EXP_evalCompileWordGetBody(EXP_EvalCompileContext* ctx, EXP_Node node, const EXP_Node** pSeq, u32* pLen)
{
    EXP_Space* space = ctx->space;
    assert(EXP_seqLen(space, node) >= 2);
    *pLen = EXP_seqLen(space, node) - 2;
    const EXP_Node* exp = EXP_seqElm(space, node);
    *pSeq = exp + 2;
}







static bool EXP_evalCompileIsDefCall(EXP_EvalCompileContext* ctx, EXP_Node node)
{
    EXP_Space* space = ctx->space;
    if (EXP_isTok(space, node))
    {
        return false;
    }
    if (!EXP_evalCheckCall(space, node))
    {
        return false;
    }
    const EXP_Node* exp = EXP_seqElm(space, node);
    const char* kDef = EXP_tokCstr(space, exp[0]);
    EXP_EvalKey k = EXP_evalCompileGetKey(ctx, kDef);
    if (k != EXP_EvalKey_Def)
    {
        return false;
    }
    if (!EXP_isTok(space, exp[1]))
    {
        EXP_evalCompileErrorAtNode(ctx, exp[1], EXP_EvalErrCode_EvalSyntax);
        return false;
    }
    return true;
}










static void EXP_evalCompileLoadDef(EXP_EvalCompileContext* ctx, EXP_Node node, EXP_EvalCompileBlock* blk)
{
    if (!EXP_evalCompileIsDefCall(ctx, node))
    {
        return;
    }
    EXP_Space* space = ctx->space;
    const EXP_Node* exp = EXP_seqElm(space, node);
    EXP_Node name = exp[1];
    EXP_EvalCompileNamed named = { name, false, .blkSrc = node };
    vec_push(&blk->dict, named);
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
    EXP_EvalCompileContext* ctx, const EXP_Node* seq, u32 len, EXP_Node srcNode, EXP_Node parent,
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
    blk->dict.length = 0;
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
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalTypeVarSpace* typeVarSpace = &ctx->typeVarSpace;
    EXP_EvalCompileCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalCompileBlock* curBlock = EXP_evalCompileGetBlock(ctx, curCall->srcNode);
    vec_u32* typeBuf = &ctx->typeBuf;

    assert(curBlock->entered);
    assert(!curBlock->completed);

    u32 ins, outs;

    if (!curBlock->haveInOut)
    {
        curBlock->haveInOut = true;

        assert(0 == curBlock->numIns);
        assert(0 == curBlock->numOuts);

        curBlock->numIns = curBlock->ins.length;

        typeBuf->length = 0;
        for (u32 i = 0; i < curBlock->ins.length; ++i)
        {
            u32 t = curBlock->ins.data[i];
            t = EXP_evalTypeReduct(typeContext, typeVarSpace, t);
            vec_push(typeBuf, t);
        }
        ins = EXP_evalTypeList(typeContext, typeBuf->data, typeBuf->length);

        const EXP_EvalTypeDesc* insDesc = EXP_evalTypeDescById(typeContext, ins);
        assert(EXP_EvalTypeType_List == insDesc->type);
        assert(!insDesc->list.hasListElm);
        assert(curBlock->numIns == insDesc->list.count);

        assert(ctx->dataStack.length + curBlock->ins.length >= curCall->dataStackP);
        curBlock->numOuts = ctx->dataStack.length + curBlock->ins.length - curCall->dataStackP;

        typeBuf->length = 0;
        for (u32 i = 0; i < curBlock->numOuts; ++i)
        {
            u32 j = ctx->dataStack.length - curBlock->numOuts + i;
            u32 t = ctx->dataStack.data[j];
            t = EXP_evalTypeReduct(typeContext, typeVarSpace, t);
            vec_push(typeBuf, t);
        }
        outs = EXP_evalTypeList(typeContext, typeBuf->data, typeBuf->length);

        const EXP_EvalTypeDesc* outsDesc = EXP_evalTypeDescById(typeContext, outs);
        assert(EXP_EvalTypeType_List == outsDesc->type);
        assert(!outsDesc->list.hasListElm);
        assert(curBlock->numOuts == outsDesc->list.count);
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

        const EXP_EvalTypeDesc* desc = EXP_evalTypeDescById(typeContext, curBlock->funType);
        const EXP_EvalTypeDescFun* fun = &desc->fun;
        u32 funIns = fun->ins;
        u32 funOuts = fun->outs;

        u32 ins1 = EXP_evalTypeList(typeContext, curBlock->ins.data, curBlock->ins.length);
        if (!EXP_evalCompileTypeUnify(ctx, funIns, ins1, &ins))
        {
            EXP_evalCompileErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalUnification);
            return;
        }
        const EXP_EvalTypeDesc* insDesc = EXP_evalTypeDescById(typeContext, ins);
        assert(EXP_EvalTypeType_List == insDesc->type);
        assert(!insDesc->list.hasListElm);
        assert(curBlock->numIns == insDesc->list.count);

        curBlock->numOuts = numOuts;

        const u32* outs1elms = ctx->dataStack.data + ctx->dataStack.length - curBlock->numOuts;
        u32 outs1 = EXP_evalTypeList(typeContext, outs1elms, curBlock->numOuts);
        if (!EXP_evalCompileTypeUnify(ctx, funOuts, outs1, &outs))
        {
            EXP_evalCompileErrorAtNode(ctx, curCall->srcNode, EXP_EvalErrCode_EvalUnification);
            return;
        }
        const EXP_EvalTypeDesc* outsDesc = EXP_evalTypeDescById(typeContext, outs);
        assert(EXP_EvalTypeType_List == outsDesc->type);
        assert(!outsDesc->list.hasListElm);
        assert(curBlock->numOuts == outsDesc->list.count);

        for (u32 i = 0; i < curBlock->numOuts; ++i)
        {
            u32 x = outsDesc->list.elms[i];
            u32 j = ctx->dataStack.length - curBlock->numOuts + i;
            ctx->dataStack.data[j] = x;
        }
    }

    curBlock->funType = EXP_evalTypeFun(typeContext, ins, outs);
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
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalCompileCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalCompileBlock* curBlock = EXP_evalCompileGetBlock(ctx, curCall->srcNode);

    assert(curBlock->entered);
    assert(curCall->dataStackP >= curBlock->ins.length);

    ctx->dataStack.length = curCall->dataStackP - curBlock->ins.length;
    for (u32 i = 0; i < curBlock->ins.length; ++i)
    {
        vec_push(dataStack, curBlock->ins.data[i]);
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









static void EXP_evalCompileFixCurBlockIns(EXP_EvalCompileContext* ctx, u32 argsOffset)
{
    vec_u32* dataStack = &ctx->dataStack;
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
            u32 t = dataStack->data[argsOffset + i];
            vec_insert(&curBlock->ins, i, t);
        }
    }
}
















static bool EXP_evalCompileFunCall
(
    EXP_EvalCompileContext* ctx, u32 funType, u32 numIns, u32* pNumOuts, EXP_Node srcNode
)
{
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    const EXP_EvalTypeDesc* desc = EXP_evalTypeDescById(typeContext, funType);
    assert(EXP_EvalTypeType_Fun == desc->type);
    const EXP_EvalTypeDescFun* fun = &desc->fun;
    u32 funIns = fun->ins;
    u32 funOuts = fun->outs;

    {
        const EXP_EvalTypeDesc* insDesc = EXP_evalTypeDescById(typeContext, funIns);
        const EXP_EvalTypeDesc* outsDesc = EXP_evalTypeDescById(typeContext, funOuts);

        if ((insDesc->type != EXP_EvalTypeType_List) && (insDesc->type != EXP_EvalTypeType_ListVar))
        {
            EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return false;
        }
        if ((outsDesc->type != EXP_EvalTypeType_List) && (outsDesc->type != EXP_EvalTypeType_ListVar))
        {
            EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return false;
        }
    }

    assert(dataStack->length >= numIns);
    u32 argsOffset = dataStack->length - numIns;
    EXP_evalCompileFixCurBlockIns(ctx, argsOffset);

    EXP_evalCompilePatContextReset(ctx);

    u32 args = EXP_evalTypeList(typeContext, dataStack->data + argsOffset, numIns);
    u32 u;
    if (!EXP_evalCompileTypeUnifyPat(ctx, args, funIns, &u))
    {
        EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
        return false;
    }

    vec_resize(dataStack, argsOffset);

    u32 outs = EXP_evalCompileTypeFromPat(ctx, funOuts);
    const EXP_EvalTypeDesc* outsDesc = EXP_evalTypeDescById(typeContext, outs);
    if ((outsDesc->type != EXP_EvalTypeType_List) || outsDesc->list.hasListElm)
    {
        EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalTypeUnsolvable);
        return false;
    }
    u32 outsLen = outsDesc->list.count;
    for (u32 i = 0; i < outsLen; ++i)
    {
        u32 x = outsDesc->list.elms[i];
        vec_push(dataStack, x);
    }
    *pNumOuts = outsLen;
    return true;
}



static void EXP_evalCompileBlockCall
(
    EXP_EvalCompileContext* ctx, const EXP_EvalCompileBlock* blk, EXP_Node srcNode
)
{
    assert(blk->haveInOut);
    u32 numOuts;
    EXP_evalCompileFunCall(ctx, blk->funType, blk->numIns, &numOuts, srcNode);
    assert(numOuts == blk->numOuts);
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
            curBlock->incomplete = true;
            EXP_evalCompileBlockRevert(ctx);
            curCall->p = EXP_evalIfBranch1(space, srcNode);
            curCall->end = EXP_evalIfBranch1(space, srcNode) + 1;
            cb->type = EXP_EvalCompileBlockCallbackType_NONE;
            return;
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
    EXP_EvalCompileContext* ctx, const EXP_Node* seq, u32 len, EXP_Node src, EXP_Node parent, bool allowDsShift
)
{
    EXP_evalCompilePushWorld(ctx, allowDsShift);
    EXP_evalCompileEnterBlock(ctx, seq, len, src, parent, EXP_EvalBlockCallback_NONE, true);
}









static bool EXP_evalCompileVarDefTok
(
    EXP_EvalCompileContext* ctx, EXP_Node node, EXP_EvalCompileCall* curCall, EXP_EvalCompileBlock* curBlock, u32 n
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalNodeTable* nodeTable = ctx->nodeTable;
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    EXP_EvalNode* enode = nodeTable->data + node.id;

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
                    u32 t;
                    EXP_EvalCompileVarFetch* fetch = ctx->varFetchBuf.data + i;
                    if (fetch->hasTypeSignature)
                    {
                        t = fetch->type;
                    }
                    else
                    {
                        t = EXP_evalTypeVar(typeContext, var);
                    }
                    vec_push(&ctx->typeBuf, t);
                }
                if (!EXP_evalCompileShiftDataStack(ctx, shiftN, ctx->typeBuf.data))
                {
                    EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                    return false;
                }
            }

            u32 off = dataStack->length - n;
            for (u32 i = 0; i < n; ++i)
            {
                u32 t = dataStack->data[off + i];
                EXP_EvalCompileVarFetch* fetch = ctx->varFetchBuf.data + i;
                if (fetch->hasTypeSignature)
                {
                    u32 a = t;
                    u32 b = fetch->type;
                    EXP_evalCompileTypeUnify(ctx, a, b, &t);
                }
                EXP_EvalCompileVar var = { t, curCall->srcNode.id, curBlock->varsCount };
                EXP_EvalCompileNamed named = { fetch->name, true, .var = var };
                vec_push(&curBlock->dict, named);
                ++curBlock->varsCount;
            }
            assert(n <= dataStack->length);
            vec_resize(dataStack, off);
            ctx->varFetchBuf.length = 0;

            if (curCall->dataStackP > dataStack->length + curBlock->ins.length)
            {
                u32 n = curCall->dataStackP - dataStack->length - curBlock->ins.length;
                u32 added = n - curBlock->ins.length;
                for (u32 i = 0; i < added; ++i)
                {
                    EXP_EvalCompileNamed* named = curBlock->dict.data + curBlock->dict.length - added + i;
                    assert(named->isVar);
                    vec_insert(&curBlock->ins, i, named->var.valType);
                }
            }
            return false;
        }
        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
        return false;
    }
    EXP_EvalCompileVarFetch fetch = { node };
    vec_push(&ctx->varFetchBuf, fetch);
    return true;
}










static void EXP_evalCompileVarDefTypeSignature(EXP_EvalCompileContext* ctx, EXP_Node node)
{
    u32 t = EXP_evalCompileTypeDecl
    (
        ctx->space,
        ctx->atypeTable,
        ctx->typeContext,
        &ctx->typeDeclStack,
        node,
        ctx->srcInfo,
        &ctx->error
    );
    if (-1 == t)
    {
        return;
    }
    assert(EXP_EvalErrCode_NONE == ctx->error.code);
    EXP_EvalCompileVarFetch* fetch = &vec_last(&ctx->varFetchBuf);
    assert(!fetch->hasTypeSignature);
    fetch->hasTypeSignature = true;
    fetch->type = t;
}










static void EXP_evalCompileApply
(
    EXP_EvalCompileContext* ctx, EXP_Node node, EXP_EvalCompileCall* curCall, EXP_Node blkSrc
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalNodeTable* nodeTable = ctx->nodeTable;
    EXP_EvalCompileBlockTable* blockTable = &ctx->blockTable;
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    EXP_EvalCompileBlock* blk = blockTable->data + blkSrc.id;
    if (blk->completed)
    {
        assert(!blk->entered);
        if (dataStack->length < blk->numIns)
        {
            u32 n = blk->numIns - dataStack->length;

            const EXP_EvalTypeDesc* desc = EXP_evalTypeDescById(typeContext, blk->funType);
            const EXP_EvalTypeDescFun* fun = &desc->fun;
            const EXP_EvalTypeDesc* insDesc = EXP_evalTypeDescById(typeContext, fun->ins);
            assert(EXP_EvalTypeType_List == insDesc->type);
            assert(!insDesc->list.hasListElm);

            EXP_evalCompilePatContextReset(ctx);
            ctx->typeBuf.length = 0;
            for (u32 i = 0; i < n; ++i)
            {
                u32 pat = insDesc->list.elms[i];
                u32 t = EXP_evalCompileTypeFromPat(ctx, pat);
                vec_push(&ctx->typeBuf, t);
            }

            if (!EXP_evalCompileShiftDataStack(ctx, n, ctx->typeBuf.data))
            {
                EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return;
            }
        }
        EXP_evalCompileBlockCall(ctx, blk, node);
    }
    else if (!blk->entered)
    {
        EXP_Node* body = NULL;
        u32 bodyLen = 0;
        EXP_evalCompileWordGetBody(ctx, blkSrc, &body, &bodyLen);

        --curCall->p;
        EXP_evalCompileEnterWorld(ctx, body, bodyLen, blkSrc, curCall->srcNode, true);
    }
    else
    {
        assert(blk->entered);
        if (blk->haveInOut)
        {
            EXP_evalCompileBlockCall(ctx, blk, node);
        }
        else
        {
            EXP_evalCompileRecurFallbackToOtherBranch(ctx);
        }
    }
}











static bool EXP_evalCompileShitfDataStackForFunCall
(
    EXP_EvalCompileContext* ctx, u32 funType, u32* pNumIns, EXP_Node srcNode
)
{
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    const EXP_EvalTypeDesc* desc = EXP_evalTypeDescById(typeContext, funType);
    if (desc->type != EXP_EvalTypeType_Fun)
    {
        EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
        return false;
    }
    const EXP_EvalTypeDesc* funIns = EXP_evalTypeDescById(typeContext, desc->fun.ins);
    if ((funIns->type != EXP_EvalTypeType_List) || funIns->list.hasListElm)
    {
        EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
        return false;
    }
    u32 numIns = funIns->list.count;

    if (dataStack->length < numIns)
    {
        u32 n = numIns - dataStack->length;

        EXP_evalCompilePatContextReset(ctx);
        ctx->typeBuf.length = 0;
        for (u32 i = 0; i < n; ++i)
        {
            u32 pat = funIns->list.elms[i];
            u32 t = EXP_evalCompileTypeFromPat(ctx, pat);
            vec_push(&ctx->typeBuf, t);
        }

        if (!EXP_evalCompileShiftDataStack(ctx, n, ctx->typeBuf.data))
        {
            EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return false;
        }
    }
    *pNumIns = numIns;
    return true;
}












static void EXP_evalCompileBlockMarch
(
    EXP_EvalCompileContext* ctx, EXP_EvalCompileCall* curCall, EXP_EvalCompileBlock* curBlock
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalNodeTable* nodeTable = ctx->nodeTable;
    EXP_EvalCompileBlockTable* blockTable = &ctx->blockTable;
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalAfunInfoVec* afunTable = ctx->afunTable;
    vec_u32* afunTypeTable = ctx->afunTypeTable;

    EXP_Node node = *(curCall->p++);
    EXP_EvalNode* enode = nodeTable->data + node.id;
    if (EXP_isTok(space, node))
    {
        if (EXP_tokQuoted(space, node))
        {
            enode->type = EXP_EvalNodeType_Atom;
            enode->atype = EXP_EvalPrimType_STRING;
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
                ctx->varFetchBuf.length = 0;
                for (u32 n = 0;;)
                {
                    if (curCall->p == curCall->end)
                    {
                        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
                        return;
                    }
                    node = *(curCall->p++);
                    enode = nodeTable->data + node.id;
                    if (EXP_isTok(space, node))
                    {
                        if (!EXP_evalCompileVarDefTok(ctx, node, curCall, curBlock, n))
                        {
                            return;
                        }
                        ++n;
                    }
                    else
                    {
                        EXP_evalCompileVarDefTypeSignature(ctx, node);
                        if (ctx->error.code)
                        {
                            return;
                        }
                    }
                }
            }
            case EXP_EvalKey_GC:
            {
                enode->type = EXP_EvalNodeType_GC;
                return;
            }
            case EXP_EvalKey_Apply:
            {
                enode->type = EXP_EvalNodeType_Apply;
                if (dataStack->length < 1)
                {
                    EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                u32 t = vec_last(dataStack);
                vec_pop(dataStack);

                u32 numIns;
                if (!EXP_evalCompileShitfDataStackForFunCall(ctx, t, &numIns, node))
                {
                    return;
                }
                enode->numIns = numIns;
                EXP_evalCompileFunCall(ctx, t, numIns, &enode->numOuts, node);
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

                u32 t = afunTypeTable->data[afun];
                u32 numIns;
                if (!EXP_evalCompileShitfDataStackForFunCall(ctx, t, &numIns, node))
                {
                    return;
                }
                enode->numIns = numIns;
                EXP_evalCompileFunCall(ctx, t, numIns, &enode->numOuts, node);
                return;
            }

            EXP_EvalCompileNamed* named;
            if (named = EXP_evalCompileGetMatched(ctx, node, curCall->srcNode))
            {
                if (named->isVar)
                {
                    enode->type = EXP_EvalNodeType_Var;
                    enode->var.block = named->var.block;
                    enode->var.id = named->var.id;
                    vec_push(dataStack, named->var.valType);
                    return;
                }
                else
                {
                    enode->type = EXP_EvalNodeType_Word;
                    enode->blkSrc = named->blkSrc;
                    EXP_evalCompileApply(ctx, node, curCall, named->blkSrc);
                    return;
                }
            }
            else
            {
                for (u32 i = 0; i < ctx->atypeTable->length; ++i)
                {
                    u32 j = ctx->atypeTable->length - 1 - i;
                    if (ctx->atypeTable->data[j].fromSymAble)
                    {
                        assert(ctx->atypeTable->data[j].ctorByStr);
                        u32 l = EXP_tokSize(space, node);
                        const char* s = EXP_tokCstr(space, node);
                        if (ctx->atypeTable->data[j].ctorByStr(s, l, NULL))
                        {
                            enode->type = EXP_EvalNodeType_Atom;
                            enode->atype = j;
                            u32 t = EXP_evalTypeAtom(ctx->typeContext, j);
                            vec_push(dataStack, t);
                            return;
                        }
                    }
                }
                EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalUnkWord);
                return;
            }
        }
    }

    if (EXP_isSeqCurly(space, node))
    {
        enode->type = EXP_EvalNodeType_BlockExe;
        enode->blkSrc = node;

        EXP_EvalCompileBlock* blk = blockTable->data + node.id;
        if (blk->completed)
        {
            assert(!blk->entered);
            EXP_evalCompileBlockCall(ctx, blk, node);
        }
        else if (!blk->entered)
        {
            const EXP_Node* body = EXP_seqElm(space, node);
            u32 bodyLen = EXP_seqLen(space, node);
            --curCall->p;
            EXP_evalCompileEnterWorld(ctx, body, bodyLen, node, curCall->srcNode, true);
        }
        else
        {
            assert(blk->entered);
            if (blk->haveInOut)
            {
                EXP_evalCompileBlockCall(ctx, blk, node);
            }
            else
            {
                EXP_evalCompileRecurFallbackToOtherBranch(ctx);
            }
        }
        return;
    }
    else if (EXP_isSeqSquare(space, node))
    {
        EXP_EvalCompileBlock* blk = EXP_evalCompileGetBlock(ctx, node);
        if (!blk->completed)
        {
            const EXP_Node* body = EXP_seqElm(space, node);
            u32 bodyLen = EXP_seqLen(space, node);
            --curCall->p;
            EXP_evalCompileEnterWorld(ctx, body, bodyLen, node, curCall->srcNode, true);
            return;
        }
        enode->type = EXP_EvalNodeType_Block;
        enode->blkSrc = node;
        enode->numIns = blk->numIns;
        enode->numOuts = blk->numOuts;
        assert(blk->completed);
        u32 t = blk->funType;
        vec_push(dataStack, t);
        return;
    }
    else if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }

    const EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);
    const char* name = EXP_tokCstr(space, elms[0]);

    EXP_EvalCompileNamed* named;
    if (named = EXP_evalCompileGetMatched(ctx, elms[0], curCall->srcNode))
    {
        if (named->isVar)
        {
            enode->type = EXP_EvalNodeType_CallVar;
            enode->var.block = named->var.block;
            enode->var.id = named->var.id;

            EXP_EvalCompileBlock* argBlk = EXP_evalCompileGetBlock(ctx, node);
            if (!argBlk->completed)
            {
                EXP_EvalCompileBlockCallback cb = { EXP_EvalCompileBlockCallbackType_CallType, .funType = named->var.valType };
                EXP_evalCompileEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
            }
            else
            {
                EXP_evalCompileBlockCall(ctx, argBlk, node);
            }
            return;
        }
        else
        {
            enode->type = EXP_EvalNodeType_CallWord;
            enode->blkSrc = named->blkSrc;

            EXP_EvalCompileBlock* argBlk = EXP_evalCompileGetBlock(ctx, node);
            if (!argBlk->completed)
            {
                EXP_EvalCompileBlockCallback cb = { EXP_EvalCompileBlockCallbackType_CallBlock, .blkSrc = named->blkSrc };
                EXP_evalCompileEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
            }
            else
            {
                EXP_evalCompileBlockCall(ctx, argBlk, node);
            }
            return;
        }
    }

    u32 afun = EXP_evalCompileGetAfun(ctx, name);
    if (afun != -1)
    {
        enode->type = EXP_EvalNodeType_CallAfun;
        enode->afun = afun;

        EXP_EvalAfunInfo* afunInfo = afunTable->data + afun;
        assert(afunInfo->call);

        EXP_EvalCompileBlock* argBlk = EXP_evalCompileGetBlock(ctx, node);
        if (!argBlk->completed)
        {
            EXP_EvalCompileBlockCallback cb = { EXP_EvalCompileBlockCallbackType_Ncall, .afun = afun };
            EXP_evalCompileEnterBlock(ctx, elms + 1, len - 1, node, curCall->srcNode, cb, false);
        }
        else
        {
            EXP_evalCompileBlockCall(ctx, argBlk, node);
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
        if (len != 4)
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
        EXP_evalCompileErrorAtNode(ctx, node, EXP_EvalErrCode_EvalUnkCall);
        return;
    }
    }
}













static void EXP_evalCompileCall(EXP_EvalCompileContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalNodeTable* nodeTable = ctx->nodeTable;
    EXP_EvalCompileBlockTable* blockTable = &ctx->blockTable;
    vec_u32* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalAfunInfoVec* afunTable = ctx->afunTable;
    vec_u32* afunTypeTable = ctx->afunTypeTable;

    EXP_EvalCompileCall* curCall;
    EXP_EvalCompileBlock* curBlock = NULL;
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
        EXP_EvalNode* srcEnode = nodeTable->data + srcNode.id;
#ifndef NDEBUG
        const EXP_NodeSrcInfo* nodeSrcInfo = EXP_nodeSrcInfo(ctx->srcInfo, srcNode);
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
            EXP_EvalAfunInfo* afunInfo = afunTable->data + cb->afun;
            u32 numIns = dataStack->length - curCall->dataStackP;
            srcEnode->numIns = numIns;
            if (EXP_evalCompileFunCall(ctx, afunTypeTable->data[cb->afun], numIns, &srcEnode->numOuts, srcNode))
            {
                EXP_evalCompileLeaveBlock(ctx);
            }
            goto next;
        }
        case EXP_EvalCompileBlockCallbackType_CallBlock:
        {
            EXP_Node blkSrc = cb->blkSrc;
            EXP_EvalCompileBlock* blk = blockTable->data + blkSrc.id;
            if (blk->completed)
            {
                assert(!blk->entered);
                if (curBlock->numIns > 0)
                {
                    EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    goto next;
                }
                if (curCall->dataStackP != (dataStack->length - blk->numIns))
                {
                    EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    goto next;
                }
                EXP_evalCompileBlockCall(ctx, blk, srcNode);
                EXP_evalCompileLeaveBlock(ctx);
                goto next;
            }
            else if (!blk->entered)
            {
                if (curCall->dataStackP > dataStack->length)
                {
                    EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                    goto next;
                }
                EXP_Node* body = NULL;
                u32 bodyLen = 0;
                EXP_evalCompileWordGetBody(ctx, blkSrc, &body, &bodyLen);
                EXP_evalCompileEnterWorld(ctx, body, bodyLen, blkSrc, srcNode, true);
                goto next;
            }
            else
            {
                assert(blk->entered);
                if (blk->haveInOut)
                {
                    if (curBlock->numIns > 0)
                    {
                        EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                        goto next;
                    }
                    if (curCall->dataStackP != (dataStack->length - blk->numIns))
                    {
                        EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                        goto next;
                    }
                    EXP_evalCompileBlockCall(ctx, blk, srcNode);
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
        case EXP_EvalCompileBlockCallbackType_CallType:
        {
            if (dataStack->length < curCall->dataStackP)
            {
                EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                goto next;
            }
            u32 numIns = dataStack->length - curCall->dataStackP;
            srcEnode->numIns = numIns;
            const EXP_EvalTypeDesc* desc = EXP_evalTypeDescById(typeContext, cb->funType);
            if (desc->type != EXP_EvalTypeType_Fun)
            {
                EXP_evalCompileErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalUnkFunType);
                goto next;
            }
            EXP_evalCompileLeaveBlock(ctx);
            EXP_evalCompileFunCall(ctx, cb->funType, numIns, &srcEnode->numOuts, srcNode);
            goto next;
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
            EXP_evalCompileSaveBlock(ctx);
            EXP_evalCompileBlockRevert(ctx);
            curCall->p = EXP_evalIfBranch1(space, srcNode);
            curCall->end = EXP_evalIfBranch1(space, srcNode) + 1;
            cb->type = EXP_EvalCompileBlockCallbackType_BranchUnify;
            goto next;
        }
        case EXP_EvalCompileBlockCallbackType_BranchUnify:
        {
            EXP_evalCompileLeaveBlock(ctx);
            goto next;
        }
        default:
            assert(false);
            return;
        }
        return;
    }
    EXP_evalCompileBlockMarch(ctx, curCall, curBlock);
    goto next;
}



























EXP_EvalError EXP_evalCompile
(
    EXP_Space* space, EXP_Node root, EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalAtypeInfoVec* atypeTable, EXP_EvalAfunInfoVec* afunTable, vec_u32* afunTypeTable,
    EXP_EvalNodeTable* nodeTable,
    EXP_EvalTypeContext* typeContext, vec_u32* typeStack
)
{
    EXP_EvalError error = { 0 };
    if (!EXP_isSeq(space, root))
    {
        return error;
    }
    EXP_EvalCompileContext _ctx = EXP_newEvalCompileContext
    (
        space, srcInfo, atypeTable, afunTable, afunTypeTable, nodeTable, typeContext
    );
    EXP_EvalCompileContext* ctx = &_ctx;

    ctx->allowDsShift = false;
    vec_dup(&ctx->dataStack, typeStack);

    const EXP_Node* seq = EXP_seqElm(space, root);
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
    if (ctx->error.code)
    {
        error = ctx->error;
        EXP_evalCompileContextFree(ctx);
        return error;
    }
    assert(rootBlk->completed);
    vec_dup(typeStack, &ctx->dataStack);
    if (!ctx->error.code)
    {
        for (u32 i = 0; i < ctx->blockTable.length; ++i)
        {
            EXP_EvalCompileBlock* vb = ctx->blockTable.data + i;
            EXP_EvalNode* enode = nodeTable->data + i + ctx->blockTableBase;
            for (u32 i = 0; i < vb->dict.length; ++i)
            {
                EXP_EvalCompileNamed* vnamed = vb->dict.data + i;
                if (vnamed->isVar)
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

















































































































































