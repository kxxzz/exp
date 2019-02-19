#include "exp_eval_a.h"





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
    EXP_SpaceSrcInfo srcInfo;
    EXP_EvalValueTypeInfoTable valueTypeTable;
    EXP_EvalNativeFunInfoTable nativeFunTable;
    EXP_EvalNodeTable nodeTable;
    EXP_EvalVarStack varStack;
    EXP_EvalCallStack callStack;
    vec_u32 typeStack;
    EXP_EvalValueVec dataStack;
    EXP_NodeVec varKeyBuf;
    EXP_EvalError error;
    EXP_EvalValue nativeCallOutBuf[EXP_EvalNativeFunOuts_MAX];
} EXP_EvalContext;






EXP_EvalContext* EXP_newEvalContext(const EXP_EvalNativeEnv* nativeEnv)
{
    EXP_EvalContext* ctx = zalloc(sizeof(*ctx));
    ctx->space = EXP_newSpace();
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
    return ctx;
}


void EXP_evalContextFree(EXP_EvalContext* ctx)
{
    vec_free(&ctx->varKeyBuf);
    vec_free(&ctx->dataStack);
    vec_free(&ctx->typeStack);
    vec_free(&ctx->callStack);
    vec_free(&ctx->varStack);
    vec_free(&ctx->nodeTable);
    vec_free(&ctx->nativeFunTable);
    vec_free(&ctx->valueTypeTable);
    EXP_spaceSrcInfoFree(&ctx->srcInfo);
    EXP_spaceFree(ctx->space);
    free(ctx);
}












EXP_EvalError EXP_evalLastError(EXP_EvalContext* ctx)
{
    return ctx->error;
}

vec_u32* EXP_evalDataTypeStack(EXP_EvalContext* ctx)
{
    return &ctx->typeStack;
}

EXP_EvalValueVec* EXP_evalDataStack(EXP_EvalContext* ctx)
{
    return &ctx->dataStack;
}










void EXP_evalPushValue(EXP_EvalContext* ctx, u32 type, EXP_EvalValue* val)
{
    vec_push(&ctx->typeStack, type);
    vec_push(&ctx->dataStack, *val);
}

void EXP_evalDrop(EXP_EvalContext* ctx)
{
    assert(ctx->typeStack.length > 0);
    assert(ctx->dataStack.length > 0);
    assert(ctx->typeStack.length == ctx->dataStack.length);
    vec_pop(&ctx->typeStack);
    vec_pop(&ctx->dataStack);
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
    EXP_EvalNode* nodeEval = ctx->nodeTable.data + curCall->srcNode.id;
    assert(EXP_EvalNodeType_Block == nodeEval->type);
    assert(ctx->varStack.length >= nodeEval->block.varsCount);
    vec_resize(&ctx->varStack, ctx->varStack.length - nodeEval->block.varsCount);
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
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
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
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
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
            EXP_evalEnterBlock(ctx, bodyLen, body, fun);
            goto next;
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
        //EXP_EvalFun* fun = EXP_evalGetMatchedFun(ctx, name, curCall->srcNode);
        EXP_Node fun;
        if (fun.id != EXP_NodeId_Invalid)
        {
            u32 bodyLen = 0;
            EXP_Node* body = NULL;
            EXP_evalDefGetBody(ctx, fun, &bodyLen, &body);
            EXP_evalEnterBlock(ctx, bodyLen, body, fun);
            goto next;
        }
        else
        {
            bool isQuoted = EXP_tokQuoted(space, node);
            if (!isQuoted)
            {
                for (u32 i = 0; i < ctx->valueTypeTable.length; ++i)
                {
                    u32 j = ctx->valueTypeTable.length - 1 - i;
                    if (ctx->valueTypeTable.data[j].ctorBySym)
                    {
                        u32 l = EXP_tokSize(space, node);
                        const char* s = EXP_tokCstr(space, node);
                        EXP_EvalValue v = { 0 };
                        if (ctx->valueTypeTable.data[j].ctorBySym(l, s, &v))
                        {
                            vec_push(dataStack, v);
                            goto next;
                        }
                    }
                }
                assert(false);
                return;
            }
            EXP_EvalValue v = { 0 };
            u32 l = EXP_tokSize(space, node);
            const char* s = EXP_tokCstr(space, node);
            v.str = (vec_char*)zalloc(sizeof(vec_char));
            vec_pusharr(v.str, s, l);
            vec_push(v.str, 0);
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
        // todo
        assert(false);
        goto next;
    }
    EXP_Node fun;
    //= EXP_evalGetMatchedFun(ctx, name, curCall->srcNode);
    if (fun.id != EXP_NodeId_Invalid)
    {
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Call, .fun = fun };
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



















void EXP_evalBlock(EXP_EvalContext* ctx, EXP_Node root)
{
    EXP_Space* space = ctx->space;
    u32 len = EXP_seqLen(space, root);
    EXP_Node* seq = EXP_seqElm(space, root);
    EXP_evalEnterBlock(ctx, len, seq, root);
    EXP_evalCall(ctx);
}









EXP_EvalError EXP_evalVerif
(
    EXP_Space* space, EXP_Node root,
    EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_EvalNodeTable* nodeTable, vec_u32* typeStack, EXP_SpaceSrcInfo* srcInfo
);



bool EXP_evalCode(EXP_EvalContext* ctx, const char* code, bool enableSrcInfo)
{
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
        ctx->error.code = EXP_EvalErrCode_EvalUnable;
        ctx->error.file = srcInfo->nodes.data[root.id].file;
        ctx->error.line = srcInfo->nodes.data[root.id].line;
        ctx->error.column = srcInfo->nodes.data[root.id].column;
        return false;
    }
    EXP_EvalError error = EXP_evalVerif
    (
        space, root, &ctx->valueTypeTable, &ctx->nativeFunTable, &ctx->nodeTable, &ctx->typeStack, &ctx->srcInfo
    );
    if (error.code)
    {
        ctx->error = error;
        return false;
    }
    EXP_evalBlock(ctx, root);
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
    bool r = EXP_evalCode(ctx, code, enableSrcInfo);
    free(code);
    return r;
}













static bool EXP_evalBoolFromSym(u32 len, const char* str, EXP_EvalValue* pVal)
{
    if (0 == strncmp(str, "true", len))
    {
        pVal->truth = true;
        return true;
    }
    if (0 == strncmp(str, "false", len))
    {
        pVal->truth = true;
        return true;
    }
    return false;
}

static bool EXP_evalNumFromSym(u32 len, const char* str, EXP_EvalValue* pVal)
{
    double num;
    u32 r = NSTR_str2num(&num, str, len, NULL);
    if (len == r)
    {
        pVal->num = num;
    }
    return len == r;
}



const EXP_EvalValueTypeInfo EXP_EvalPrimValueTypeInfoTable[EXP_NumEvalPrimValueTypes] =
{
    { "bool", EXP_evalBoolFromSym },
    { "num", EXP_evalNumFromSym },
    { "str", NULL },
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















































































































