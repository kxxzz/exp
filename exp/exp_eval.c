#include "exp_eval_a.h"
#include "exp_eval_type_a.h"



typedef struct EXP_EvalAtomMem
{
    bool gcFlag;
    char a[1];
} EXP_EvalAtomMem;




typedef enum EXP_EvalBlockCallbackType
{
    EXP_EvalBlockCallbackType_NONE,
    EXP_EvalBlockCallbackType_Ncall,
    EXP_EvalBlockCallbackType_CallWord,
    EXP_EvalBlockCallbackType_CallVar,
    EXP_EvalBlockCallbackType_Cond,
} EXP_EvalBlockCallbackType;

typedef struct EXP_EvalBlockCallback
{
    EXP_EvalBlockCallbackType type;
    union
    {
        u32 afun;
        EXP_Node blkSrc;
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



typedef vec_t(EXP_EvalAtomMem*) EXP_EvalAtomMemPtrVec;
typedef vec_t(EXP_EvalAtomMemPtrVec) EXP_AtomMemTable;



typedef struct EXP_EvalContext
{
    EXP_Space* space;
    EXP_SpaceSrcInfo srcInfo;
    EXP_EvalAtypeInfoVec atypeTable;
    EXP_EvalAfunInfoVec afunTable;
    EXP_EvalTypeContext* typeContext;
    EXP_EvalNodeTable nodeTable;
    EXP_AtomMemTable atomMemTable;

    vec_u32 typeStack;
    EXP_EvalCallStack callStack;
    EXP_EvalValueVec varStack;
    EXP_EvalValueVec dataStack;

    EXP_EvalError error;
    EXP_EvalValue ncallOutBuf[EXP_EvalAfunOuts_MAX];
    bool gcFlag;

    EXP_EvalFileInfoTable fileInfoTable;
} EXP_EvalContext;






EXP_EvalContext* EXP_newEvalContext(const EXP_EvalAtomTable* addAtomTable)
{
    EXP_EvalContext* ctx = zalloc(sizeof(*ctx));
    ctx->space = EXP_newSpace();
    for (u32 i = 0; i < EXP_NumEvalPrimTypes; ++i)
    {
        vec_push(&ctx->atypeTable, EXP_EvalPrimTypeInfoTable()[i]);
    }
    for (u32 i = 0; i < EXP_NumEvalPrimFuns; ++i)
    {
        vec_push(&ctx->afunTable, EXP_EvalPrimFunInfoTable()[i]);
    }
    if (addAtomTable)
    {
        for (u32 i = 0; i < addAtomTable->numTypes; ++i)
        {
            vec_push(&ctx->atypeTable, addAtomTable->types[i]);
        }
        for (u32 i = 0; i < addAtomTable->numFuns; ++i)
        {
            vec_push(&ctx->afunTable, addAtomTable->funs[i]);
        }
    }
    vec_resize(&ctx->atomMemTable, ctx->atypeTable.length);
    memset(ctx->atomMemTable.data, 0, sizeof(ctx->atomMemTable.data[0])*ctx->atomMemTable.length);
    ctx->typeContext = EXP_newEvalTypeContext();
    return ctx;
}




void EXP_evalContextFree(EXP_EvalContext* ctx)
{
    vec_free(&ctx->fileInfoTable);

    vec_free(&ctx->dataStack);
    vec_free(&ctx->varStack);
    vec_free(&ctx->callStack);
    vec_free(&ctx->typeStack);

    for (u32 i = 0; i < ctx->atomMemTable.length; ++i)
    {
        EXP_EvalAtomMemPtrVec* mpVec = ctx->atomMemTable.data + i;
        EXP_EvalAtypeInfo* atypeInfo = ctx->atypeTable.data + i;
        if ((atypeInfo->allocMemSize > 0) || atypeInfo->aDtor)
        {
            for (u32 i = 0; i < mpVec->length; ++i)
            {
                if (atypeInfo->aDtor)
                {
                    atypeInfo->aDtor(mpVec->data[i]->a);
                }
                if (atypeInfo->allocMemSize > 0)
                {
                    free(mpVec->data[i]);
                }
            }
        }
        else
        {
            assert(0 == atypeInfo->allocMemSize);
            assert(0 == mpVec->length);
        }
        vec_free(mpVec);
    }
    vec_free(&ctx->atomMemTable);

    vec_free(&ctx->nodeTable);
    EXP_evalTypeContextFree(ctx->typeContext);
    vec_free(&ctx->afunTable);
    vec_free(&ctx->atypeTable);
    EXP_spaceSrcInfoFree(&ctx->srcInfo);
    EXP_spaceFree(ctx->space);
    free(ctx);
}






static void EXP_evalErrorAtNode(EXP_EvalContext* ctx, EXP_Node node, EXP_EvalErrCode errCode)
{
    ctx->error.code = errCode;
    EXP_SpaceSrcInfo* srcInfo = &ctx->srcInfo;
    if (srcInfo)
    {
        assert(node.id < srcInfo->nodes.length);
        EXP_NodeSrcInfo* nodeSrcInfo = srcInfo->nodes.data + node.id;
        ctx->error.file = nodeSrcInfo->file;
        ctx->error.line = nodeSrcInfo->line;
        ctx->error.column = nodeSrcInfo->column;
    }
}









EXP_EvalError EXP_evalLastError(EXP_EvalContext* ctx)
{
    return ctx->error;
}

EXP_EvalTypeContext* EXP_evalDataTypeContext(EXP_EvalContext* ctx)
{
    return ctx->typeContext;
}

vec_u32* EXP_evalDataTypeStack(EXP_EvalContext* ctx)
{
    return &ctx->typeStack;
}

EXP_EvalValueVec* EXP_evalDataStack(EXP_EvalContext* ctx)
{
    return &ctx->dataStack;
}




















void EXP_evalPushValue(EXP_EvalContext* ctx, u32 type, EXP_EvalValue val)
{
    vec_push(&ctx->typeStack, type);
    vec_push(&ctx->dataStack, val);
}

void EXP_evalDrop(EXP_EvalContext* ctx)
{
    assert(ctx->typeStack.length > 0);
    assert(ctx->dataStack.length > 0);
    assert(ctx->typeStack.length == ctx->dataStack.length);
    vec_pop(&ctx->typeStack);
    vec_pop(&ctx->dataStack);
}



















static void EXP_evalSetGcFlag(EXP_EvalContext* ctx, EXP_EvalValue v)
{
    switch (v.type)
    {
    case EXP_EvalValueType_AtomA:
    {
        EXP_EvalAtomMem* m = (EXP_EvalAtomMem*)((char*)v.a - offsetof(EXP_EvalAtomMem, a));
        if (m->gcFlag != ctx->gcFlag)
        {
            m->gcFlag = ctx->gcFlag;
        }
        break;
    }
    default:
        break;
    }
}


static void EXP_evalGC(EXP_EvalContext* ctx)
{
    EXP_EvalValueVec* varStack = &ctx->varStack;
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
    EXP_EvalAtypeInfoVec* atypeTable = &ctx->atypeTable;
    EXP_AtomMemTable* atomMemTable = &ctx->atomMemTable;

    ctx->gcFlag = !ctx->gcFlag;

    for (u32 i = 0; i < varStack->length; ++i)
    {
        EXP_evalSetGcFlag(ctx, varStack->data[i]);
    }
    for (u32 i = 0; i < dataStack->length; ++i)
    {
        EXP_evalSetGcFlag(ctx, dataStack->data[i]);
    }

    for (u32 i = 0; i < atomMemTable->length; ++i)
    {
        EXP_EvalAtomMemPtrVec* mpVec = atomMemTable->data + i;
        EXP_EvalAtypeInfo* atypeInfo = atypeTable->data + i;
        if ((atypeInfo->allocMemSize > 0) || atypeInfo->aDtor)
        {
            for (u32 i = 0; i < mpVec->length; ++i)
            {
                bool gcFlag = mpVec->data[i]->gcFlag;
                if (gcFlag != ctx->gcFlag)
                {
                    if (atypeInfo->aDtor)
                    {
                        atypeInfo->aDtor(mpVec->data[i]->a);
                    }
                    if (atypeInfo->allocMemSize > 0)
                    {
                        free(mpVec->data[i]);
                    }
                    mpVec->data[i] = vec_last(mpVec);
                    vec_pop(mpVec);
                    --i;
                }
            }
        }
        else
        {
            assert(0 == atypeInfo->allocMemSize);
            assert(0 == mpVec->length);
        }
    }
}

















static void EXP_evalWordDefGetBody(EXP_EvalContext* ctx, EXP_Node node, u32* pLen, EXP_Node** pSeq)
{
    EXP_Space* space = ctx->space;
    assert(EXP_seqLen(space, node) >= 2);
    *pLen = EXP_seqLen(space, node) - 2;
    EXP_Node* defCall = EXP_seqElm(space, node);
    *pSeq = defCall + 2;
}













static EXP_EvalValue* EXP_evalGetVar(EXP_EvalContext* ctx, const EXP_EvalNodeVar* evar)
{
    EXP_Space* space = ctx->space;
    EXP_EvalNodeTable* nodeTable = &ctx->nodeTable;
    EXP_EvalCallStack* callStack = &ctx->callStack;
    EXP_EvalValueVec* varStack = &ctx->varStack;
    u32 offset = varStack->length;
    for (u32 i = 0; i < callStack->length; ++i)
    {
        EXP_EvalCall* call = callStack->data + callStack->length - 1 - i;
        EXP_EvalNode* blkEnode = nodeTable->data + call->srcNode.id;
        assert(offset >= blkEnode->varsCount);
        offset -= blkEnode->varsCount;
        if (evar->block.id == call->srcNode.id)
        {
            offset += evar->id;
            return &varStack->data[offset];
        }
    }
    return NULL;
}













static void EXP_evalEnterBlock(EXP_EvalContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode)
{
    EXP_EvalBlockCallback nocb = { EXP_EvalBlockCallbackType_NONE };
    EXP_EvalCall call = { srcNode, seq, seq + len, nocb };
    vec_push(&ctx->callStack, call);
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
    EXP_EvalNode* enode = ctx->nodeTable.data + curCall->srcNode.id;
    assert(ctx->varStack.length >= enode->varsCount);
    u32 varStackLength1 = ctx->varStack.length - enode->varsCount;
    vec_resize(&ctx->varStack, varStackLength1);
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







static void EXP_evalAfunCall
(
    EXP_EvalContext* ctx, EXP_EvalAfunInfo* afunInfo, EXP_Node srcNode
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    u32 argsOffset = dataStack->length - afunInfo->numIns;
    afunInfo->call(space, dataStack->data + argsOffset, ctx->ncallOutBuf);
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < afunInfo->numOuts; ++i)
    {
        EXP_EvalValue v = ctx->ncallOutBuf[i];
        vec_push(dataStack, v);
    }
}








static void EXP_evalCall(EXP_EvalContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalCall* curCall;
    EXP_EvalNodeTable* nodeTable = &ctx->nodeTable;
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalAtypeInfoVec* atypeTable = &ctx->atypeTable;
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
        case EXP_EvalBlockCallbackType_Ncall:
        {
            EXP_EvalAfunInfo* afunInfo = ctx->afunTable.data + cb->afun;
            u32 numIns = afunInfo->numIns;
            EXP_evalAfunCall(ctx, afunInfo, curCall->srcNode);
            break;
        }
        case EXP_EvalBlockCallbackType_CallWord:
        {
            EXP_Node blkSrc = cb->blkSrc;
            u32 bodyLen = 0;
            EXP_Node* body = NULL;
            EXP_evalWordDefGetBody(ctx, blkSrc, &bodyLen, &body);
            if (!EXP_evalLeaveBlock(ctx))
            {
                goto next;
            }
            // tail call optimization
            while (EXP_evalAtTail(ctx))
            {
                if (!EXP_evalLeaveBlock(ctx))
                {
                    break;
                }
            }
            EXP_evalEnterBlock(ctx, bodyLen, body, blkSrc);
            goto next;
        }
        case EXP_EvalBlockCallbackType_CallVar:
        {
            EXP_Node blkSrc = cb->blkSrc;
            u32 bodyLen = EXP_seqLen(space, blkSrc);
            EXP_Node* body = EXP_seqElm(space, blkSrc);
            if (!EXP_evalLeaveBlock(ctx))
            {
                goto next;
            }
            // tail call optimization
            while (EXP_evalAtTail(ctx))
            {
                if (!EXP_evalLeaveBlock(ctx))
                {
                    break;
                }
            }
            EXP_evalEnterBlock(ctx, bodyLen, body, blkSrc);
            goto next;
        }
        case EXP_EvalBlockCallbackType_Cond:
        {
            EXP_EvalValue v = vec_last(dataStack);
            vec_pop(dataStack);
            if (!EXP_evalLeaveBlock(ctx))
            {
                goto next;
            }
            EXP_Node srcNode = curCall->srcNode;
            if (v.b)
            {
                EXP_evalEnterBlock(ctx, 1, EXP_evalIfBranch0(space, srcNode), srcNode);
                goto next;
            }
            else if (EXP_evalIfHasBranch1(space, srcNode))
            {
                EXP_evalEnterBlock(ctx, 1, EXP_evalIfBranch1(space, srcNode), srcNode);
                goto next;
            }
        }
        default:
            assert(false);
            goto next;
        }
        if (EXP_evalLeaveBlock(ctx))
        {
            goto next;
        }
        return;
    }
    EXP_Node node = *(curCall->p++);
#ifndef NDEBUG
    EXP_NodeSrcInfo* nodeSrcInfo = ctx->srcInfo.nodes.data + node.id;
#endif
    EXP_EvalNode* enode = nodeTable->data + node.id;
    switch (enode->type)
    {
    case EXP_EvalNodeType_VarDefBegin:
    {
        assert(EXP_isTok(space, node));
        assert(EXP_EvalBlockCallbackType_NONE == curCall->cb.type);
        EXP_Node* begin = curCall->p;
        for (u32 n = 0;;)
        {
            assert(curCall->p < curCall->end);
            node = *(curCall->p++);
            if (EXP_isTok(space, node))
            {
                enode = nodeTable->data + node.id;
                if (EXP_EvalNodeType_VarDefEnd == enode->type)
                {
                    assert(n <= dataStack->length);
                    u32 off = dataStack->length - n;
                    for (u32 i = 0; i < n; ++i)
                    {
                        EXP_EvalValue val = dataStack->data[off + i];
                        vec_push(&ctx->varStack, val);
                    }
                    vec_resize(dataStack, off);
                    goto next;
                }
                else
                {
                    assert(EXP_EvalNodeType_None == enode->type);
                }
                ++n;
            }
        }
    }
    case EXP_EvalNodeType_GC:
    {
        EXP_evalGC(ctx);
        goto next;
    }
    case EXP_EvalNodeType_Apply:
    {
        EXP_EvalValue v = vec_last(dataStack);
        vec_pop(dataStack);
        EXP_Node blkSrc = v.src;
        assert(EXP_isSeqSquare(space, blkSrc));
        u32 bodyLen = EXP_seqLen(space, blkSrc);
        EXP_Node* body = EXP_seqElm(space, blkSrc);
        EXP_evalEnterBlock(ctx, bodyLen, body, blkSrc);
        goto next;
    }
    case EXP_EvalNodeType_Afun:
    {
        assert(EXP_isTok(space, node));
        EXP_EvalAfunInfo* afunInfo = ctx->afunTable.data + enode->afun;
        assert(afunInfo->call);
        assert(dataStack->length >= afunInfo->numIns);
        EXP_evalAfunCall(ctx, afunInfo, node);
        goto next;
    }
    case EXP_EvalNodeType_Var:
    {
        assert(EXP_isTok(space, node));
        EXP_EvalValue* val = EXP_evalGetVar(ctx, &enode->var);
        vec_push(dataStack, *val);
        goto next;
    }
    case EXP_EvalNodeType_Word:
    {
        assert(EXP_isTok(space, node));
        EXP_Node blkSrc = enode->blkSrc;
        u32 bodyLen = 0;
        EXP_Node* body = NULL;
        EXP_evalWordDefGetBody(ctx, blkSrc, &bodyLen, &body);
        EXP_evalEnterBlock(ctx, bodyLen, body, blkSrc);
        goto next;
    }
    case EXP_EvalNodeType_Atom:
    {
        assert(EXP_isTok(space, node));
        u32 l = EXP_tokSize(space, node);
        const char* s = EXP_tokCstr(space, node);
        EXP_EvalValue v = { 0 };
        EXP_EvalAtypeInfo* atypeInfo = atypeTable->data + enode->atype;
        if (atypeInfo->allocMemSize > 0)
        {
            EXP_EvalAtomMem* m = (EXP_EvalAtomMem*)zalloc(offsetof(EXP_EvalAtomMem, a) + atypeInfo->allocMemSize);
            v.a = m->a;
            EXP_EvalAtomMemPtrVec* mpVec = ctx->atomMemTable.data + enode->atype;
            vec_push(mpVec, m);
            v.type = EXP_EvalValueType_AtomA;
            m->gcFlag = ctx->gcFlag;
        }
        assert(atypeInfo->ctorByStr);
        if (atypeInfo->ctorByStr(l, s, &v))
        {
            vec_push(dataStack, v);
        }
        else
        {
            assert(false);
            //EXP_evalErrorAtNode(ctx, node, EXP_EvalErrCode_EvalAtomCtorByStr);
        }
        goto next;
    }
    case EXP_EvalNodeType_Block:
    {
        assert(EXP_isSeqSquare(space, node));
        EXP_EvalValue v = { 0 };
        v.src = enode->blkSrc;
        vec_push(dataStack, v);
        goto next;
    }
    case EXP_EvalNodeType_CallVar:
    {
        assert(EXP_evalCheckCall(space, node));
        EXP_EvalValue* val = EXP_evalGetVar(ctx, &enode->var);
        EXP_Node blkSrc = val->src;
        assert(EXP_isSeqSquare(space, blkSrc));
        EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_CallVar, .blkSrc = blkSrc };
        EXP_evalEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
        goto next;
    }
    case EXP_EvalNodeType_CallWord:
    {
        assert(EXP_evalCheckCall(space, node));
        EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_CallWord, .blkSrc = enode->blkSrc };
        EXP_evalEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
        goto next;
    }
    case EXP_EvalNodeType_CallAfun:
    {
        assert(EXP_evalCheckCall(space, node));
        EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        u32 afun = enode->afun;
        assert(afun != -1);
        EXP_EvalAfunInfo* afunInfo = ctx->afunTable.data + afun;
        assert(afunInfo->call);
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Ncall, .afun = afun };
        EXP_evalEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
        goto next;
    }
    case EXP_EvalNodeType_Def:
    {
        assert(EXP_evalCheckCall(space, node));
        assert(EXP_EvalBlockCallbackType_NONE == curCall->cb.type);
        goto next;
    }
    case EXP_EvalNodeType_If:
    {
        assert(EXP_evalCheckCall(space, node));
        EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        assert((3 == len) || (4 == len));
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Cond };
        EXP_evalEnterBlockWithCB(ctx, 1, elms + 1, node, cb);
        goto next;
    }
    default:
        assert(false);
    }
}



















void EXP_evalBlock(EXP_EvalContext* ctx, EXP_Node root)
{
    EXP_Space* space = ctx->space;
    u32 len = EXP_seqLen(space, root);
    EXP_Node* seq = EXP_seqElm(space, root);
    EXP_evalEnterBlock(ctx, len, seq, root);
    EXP_evalCall(ctx);
}









EXP_EvalError EXP_evalCompile
(
    EXP_Space* space, EXP_Node root, EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalAtypeInfoVec* atypeTable, EXP_EvalAfunInfoVec* afunTable,
    EXP_EvalNodeTable* nodeTable,
    EXP_EvalTypeContext* typeContext, vec_u32* typeStack
);



bool EXP_evalCode(EXP_EvalContext* ctx, const char* filename, const char* code, bool enableSrcInfo)
{
    if (enableSrcInfo)
    {
        EXP_EvalFileInfo fileInfo = { 0 };
        stzncpy(fileInfo.name, filename, EXP_EvalFileName_MAX);
        vec_push(&ctx->fileInfoTable, fileInfo);
    }
    EXP_SpaceSrcInfo* srcInfo = NULL;
    if (enableSrcInfo)
    {
        srcInfo = &ctx->srcInfo;
    }
    EXP_Space* space = ctx->space;
    EXP_Node root = EXP_loadSrcAsList(space, code, srcInfo);
    if (EXP_NodeId_Invalid == root.id)
    {
        ctx->error.code = EXP_EvalErrCode_ExpSyntax;
        ctx->error.file = 0;
        if (srcInfo)
        {
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 6011)
#endif
            ctx->error.line = vec_last(&srcInfo->nodes).line;
            ctx->error.column = vec_last(&srcInfo->nodes).column;
#ifdef _MSC_VER
# pragma warning(pop)
#endif
        }
        else
        {
            ctx->error.line = -1;
            ctx->error.column = -1;
        }
        return false;
    }


    if (!EXP_isSeq(space, root))
    {
        ctx->error.code = EXP_EvalErrCode_EvalSyntax;
        ctx->error.file = srcInfo->nodes.data[root.id].file;
        ctx->error.line = srcInfo->nodes.data[root.id].line;
        ctx->error.column = srcInfo->nodes.data[root.id].column;
        return false;
    }
    EXP_EvalError error = EXP_evalCompile
    (
        space, root, &ctx->srcInfo,
        &ctx->atypeTable, &ctx->afunTable,
        &ctx->nodeTable,
        ctx->typeContext, &ctx->typeStack
    );
    if (error.code)
    {
        ctx->error = error;
        return false;
    }
    EXP_evalBlock(ctx, root);
    if (ctx->error.code)
    {
        return false;
    }
    return true;
}



bool EXP_evalFile(EXP_EvalContext* ctx, const char* fileName, bool enableSrcInfo)
{
    char* code = NULL;
    u32 codeSize = FILEU_readFile(fileName, &code);
    if (-1 == codeSize)
    {
        ctx->error.code = EXP_EvalErrCode_SrcFile;
        ctx->error.file = 0;
        return false;
    }
    if (0 == codeSize)
    {
        return false;
    }
    bool r = EXP_evalCode(ctx, fileName, code, enableSrcInfo);
    free(code);
    return r;
}

























const EXP_EvalFileInfoTable* EXP_evalFileInfoTable(EXP_EvalContext* ctx)
{
    return &ctx->fileInfoTable;
}













const char** EXP_EvalErrCodeNameTable(void)
{
    static const char* a[EXP_NumEvalErrorCodes] =
    {
        "NONE",
        "SrcFile",
        "ExpSyntax",
        "EvalSyntax",
        "EvalUnkWord",
        "EvalUnkCall",
        "EvalArgs",
        "EvalBranchUneq",
        "EvalRecurNoBaseCase",
        "EvalUnification",
        "EvalAtomCtorByStr",
    };
    return a;
}












































































































