#include "a.h"



typedef struct EXP_NodeInfo
{
    EXP_NodeType type;
    u32 offset;
    u32 length;
} EXP_NodeInfo;

typedef vec_t(EXP_NodeInfo) EXP_NodeInfoVec;


typedef vec_t(EXP_Node) EXP_NodeVec;
typedef vec_t(EXP_NodeVec) EXP_NodeVecVec;


typedef struct EXP_Space
{
    EXP_NodeInfoVec nodes;
    //vec_u64 strHashs;
    //vec_u64 expHashs;
    vec_char strs;
    EXP_NodeVec exps;
    EXP_NodeVecVec expDefStack;
} EXP_Space;






EXP_Space* EXP_newSpace(void)
{
    EXP_Space* space = zalloc(sizeof(*space));
    return space;
}

void EXP_spaceFree(EXP_Space* space)
{
    for (u32 i = 0; i < space->expDefStack.length; ++i)
    {
        vec_free(space->expDefStack.data + i);
    }
    vec_free(&space->expDefStack);
    vec_free(&space->exps);
    vec_free(&space->strs);
    //vec_free(&space->expHashs);
    //vec_free(&space->strHashs);
    vec_free(&space->nodes);
    free(space);
}







EXP_NodeType EXP_nodeType(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    return info->type;
}









EXP_Node EXP_addStr(EXP_Space* space, const char* str)
{
    u32 len = (u32)strlen(str);
    EXP_NodeInfo info = { EXP_NodeType_Str, space->strs.length, len };
    vec_pusharr(&space->strs, str, len + 1);
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, info);
    return node;
}

EXP_Node EXP_addLenStr(EXP_Space* space, u32 len, const char* str)
{
    EXP_NodeInfo info = { EXP_NodeType_Str, space->strs.length, len };
    vec_pusharr(&space->strs, str, len);
    vec_push(&space->strs, 0);
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, info);
    return node;
}





void EXP_addExpEnter(EXP_Space* space)
{
    EXP_NodeVec exp = { 0 };
    vec_push(&space->expDefStack, exp);
}

void EXP_addExpPush(EXP_Space* space, EXP_Node x)
{
    assert(space->expDefStack.length > 0);
    EXP_NodeVec* exps = space->expDefStack.data + space->expDefStack.length - 1;
    vec_push(exps, x);
}

void EXP_addExpCancel(EXP_Space* space)
{
    EXP_NodeVec* exps = space->expDefStack.data + space->expDefStack.length - 1;
    vec_free(exps);
    vec_resize(&space->expDefStack, space->expDefStack.length - 1);
}

EXP_Node EXP_addExpDone(EXP_Space* space)
{
    EXP_NodeVec* exps = space->expDefStack.data + space->expDefStack.length - 1;
    EXP_NodeInfo info = { EXP_NodeType_Exp, space->exps.length, exps->length };
    vec_concat(&space->exps, exps);
    vec_free(exps);
    vec_resize(&space->expDefStack, space->expDefStack.length - 1);
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, info);
    return node;
}





void EXP_undoAdd1(EXP_Space* space)
{
    assert(space->nodes.length > 0);
    EXP_NodeInfo* info = space->nodes.data + space->nodes.length - 1;
    switch (info->type)
    {
    case EXP_NodeType_Str:
    {
        vec_resize(&space->strs, space->strs.length - info->length - 1);
        assert(space->strs.length == info->offset);
        break;
    }
    case EXP_NodeType_Exp:
    {
        vec_resize(&space->exps, space->exps.length - info->length);
        assert(space->exps.length == info->offset);
        break;
    }
    default:
        assert(false);
        break;
    }
    vec_resize(&space->nodes, space->nodes.length - 1);
}

void EXP_undoAdd(EXP_Space* space, u32 n)
{
    for (u32 i = 0; i < n; ++i)
    {
        EXP_undoAdd1(space);
    }
}







u32 EXP_strSize(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Str == info->type);
    return info->length;
}

const char* EXP_strCstr(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Str == info->type);
    return space->strs.data + info->offset;
}




u32 EXP_expLen(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Exp == info->type);
    return info->length;
}

EXP_Node* EXP_expElm(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Exp == info->type);
    return space->exps.data + info->offset;
}


















static u32 EXP_saveExpSL
(
    const EXP_Space* space, const EXP_NodeInfo* expInfo, char* buf, u32 bufSize,
    const EXP_NodeSrcInfoTable* srcInfoTable
)
{
    u32 n = 0;
    u32 bufRemain = bufSize;
    char* bufPtr = buf;

    for (u32 i = 0; i < expInfo->length; ++i)
    {
        u32 en = EXP_saveSL(space, space->exps.data[expInfo->offset + i], bufPtr, bufRemain, srcInfoTable);
        if (en < bufRemain)
        {
            bufRemain -= en;
            bufPtr += en;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += en;

        if (i < expInfo->length - 1)
        {
            if (1 < bufRemain)
            {
                *bufPtr = ' ';
                bufRemain -= 1;
                bufPtr += 1;
            }
            else
            {
                bufRemain = 0;
                bufPtr = NULL;
            }
            n += 1;
        }
    }
    return n;
}


u32 EXP_saveSL
(
    const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize,
    const EXP_NodeSrcInfoTable* srcInfoTable
)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    switch (info->type)
    {
    case EXP_NodeType_Str:
    {
        const char* str = space->strs.data + info->offset;
        u32 sreLen = info->length;
        u32 n;
        bool isStrTok = false;
        if (srcInfoTable && srcInfoTable->data[node.id].exist)
        {
            isStrTok = srcInfoTable->data[node.id].isStrTok;
        }
        else
        {
            for (u32 i = 0; i < sreLen; ++i)
            {
                if (strchr("[]\"' \t\n\r\b\f", str[i]))
                {
                    isStrTok = true;
                    break;
                }
            }
        }
        if (isStrTok)
        {
            u32 l = 2;
            for (u32 i = 0; i < sreLen; ++i)
            {
                if (' ' >= str[i])
                {
                    ++l;
                }
                else if (strchr("[]\"'", str[i]))
                {
                    ++l;
                }
                ++l;
            }
            if (buf && (l < bufSize))
            {
                n = 0;
                buf[n++] = '"';
                for (u32 i = 0; i < sreLen; ++i)
                {
                    if (' ' >= str[i])
                    {
                        buf[n++] = '\\';
                    }
                    else if (strchr("[]\"'", str[i]))
                    {
                        buf[n++] = '\\';
                    }
                    buf[n++] = str[i];
                }
                buf[n++] = '"';
                assert(n == l);
            }
            else
            {
                n = l;
            }
        }
        else
        {
            n = snprintf(buf, bufSize, "%s", str);
        }
        return n;
    }
    case EXP_NodeType_Exp:
    {
        u32 n = 0;

        u32 bufRemain = bufSize;
        char* bufPtr = buf;

        if (1 < bufRemain)
        {
            *bufPtr = '[';
            bufRemain -= 1;
            bufPtr += 1;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += 1;

        u32 n1 = EXP_saveExpSL(space, info, bufPtr, bufRemain, srcInfoTable);
        if (n1 < bufRemain)
        {
            bufRemain -= n1;
            bufPtr += n1;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += n1;

        if (1 < bufRemain)
        {
            *bufPtr = ']';
            *(bufPtr + 1) = 0;
            bufRemain -= 1;
            bufPtr += 1;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += 1;
        return n;
    }
    default:
        assert(false);
        return 0;
    }
}















typedef struct EXP_SaveMLctx
{
    const EXP_Space* space;
    const EXP_SaveMLopt* opt;
    const u32 bufSize;
    char* const buf;

    u32 n;
    u32 column;
    u32 depth;
} EXP_SaveMLctx;



static bool EXP_saveMlForward(EXP_SaveMLctx* ctx, u32 a)
{
    ctx->n += a;
    ctx->column += a;
    return ctx->column <= ctx->opt->width;
}

static void EXP_saveMlBack(EXP_SaveMLctx* ctx, u32 a)
{
    assert(ctx->n >= a);
    assert(ctx->column >= a);
    ctx->n -= a;
    ctx->column -= a;
}




static void EXP_saveMlAdd(EXP_SaveMLctx* ctx, const char* s)
{
    u32 a = (u32)strlen(s);
    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    if (bufRemain > 0)
    {
        assert(ctx->buf);
        u32 wn = min(bufRemain - 1, a);
        if (wn > 0)
        {
            char* bufPtr = ctx->buf + ctx->n;
            stzncpy(bufPtr, s, wn + 1);
            bufPtr[wn] = 0;
        }
    }
    ctx->n += a;
    u32 ca = 0;
    for (u32 i = 0; i < a; ++i)
    {
        if ('\n' == s[a - 1 - i])
        {
            ctx->column = 0;
            break;
        }
        else
        {
            ++ca;
        }
    }
    ctx->column += ca;
}



static void EXP_saveMlAddIdent(EXP_SaveMLctx* ctx)
{
    u32 n = ctx->opt->indent * ctx->depth;
    for (u32 i = 0; i < n; ++i)
    {
        EXP_saveMlAdd(ctx, " ");
    }
}







static void EXP_saveMlAddNode(EXP_SaveMLctx* ctx, EXP_Node node);


static void EXP_saveMlAddExp(EXP_SaveMLctx* ctx, const EXP_NodeInfo* expInfo)
{
    const EXP_Space* space = ctx->space;
    for (u32 i = 0; i < expInfo->length; ++i)
    {
        EXP_saveMlAddIdent(ctx);

        EXP_saveMlAddNode(ctx, space->exps.data[expInfo->offset + i]);

        EXP_saveMlAdd(ctx, "\n");
    }
}




static void EXP_saveMlAddNodeExp(EXP_SaveMLctx* ctx, EXP_Node node)
{
    const EXP_Space* space = ctx->space;

    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
    u32 a = EXP_saveSL(space, node, bufPtr, bufRemain, ctx->opt->srcInfoTable);
    bool ok = EXP_saveMlForward(ctx, a);

    if (!ok)
    {
        EXP_NodeInfo* info = space->nodes.data + node.id;

        EXP_saveMlBack(ctx, a);

        EXP_saveMlAdd(ctx, "[\n");

        ++ctx->depth;
        EXP_saveMlAddExp(ctx, info);
        --ctx->depth;

        EXP_saveMlAddIdent(ctx);
        EXP_saveMlAdd(ctx, "]");
    }
}



static void EXP_saveMlAddNode(EXP_SaveMLctx* ctx, EXP_Node node)
{
    const EXP_Space* space = ctx->space;
    EXP_NodeInfo* info = space->nodes.data + node.id;
    switch (info->type)
    {
    case EXP_NodeType_Str:
    {
        u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
        char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
        u32 a = EXP_saveSL(space, node, bufPtr, bufRemain, ctx->opt->srcInfoTable);
        EXP_saveMlForward(ctx, a);
        return;
    }
    case EXP_NodeType_Exp:
    {
        EXP_saveMlAddNodeExp(ctx, node);
        return;
    }
    default:
        assert(false);
        return;
    }
}



















u32 EXP_saveML(const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize, const EXP_SaveMLopt* opt)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    switch (info->type)
    {
    case EXP_NodeType_Str:
    {
        return EXP_saveSL(space, node, buf, bufSize, opt->srcInfoTable);
    }
    case EXP_NodeType_Exp:
    {
        EXP_SaveMLctx ctx =
        {
            space, opt, bufSize, buf,
        };
        EXP_saveMlAddNodeExp(&ctx, node);
        return ctx.n;
    }
    default:
        assert(false);
        return 0;
    }
}




























































































































