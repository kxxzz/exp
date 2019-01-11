#include "a.h"



typedef struct PRIM_NodeInfo
{
    PRIM_NodeType type;
    u32 offset;
    u32 length;
} PRIM_NodeInfo;

typedef vec_t(PRIM_NodeInfo) PRIM_NodeInfoVec;


typedef vec_t(PRIM_Node) PRIM_NodeVec;
typedef vec_t(PRIM_NodeVec) PRIM_NodeVecVec;


typedef struct PRIM_Space
{
    PRIM_NodeInfoVec nodeInfoTable;
    vec_u64 strHashs;
    vec_u64 expHashs;
    vec_char strs;
    PRIM_NodeVec exps;
    PRIM_NodeVecVec expDefStack;
} PRIM_Space;






PRIM_Space* PRIM_newSpace(void)
{
    PRIM_Space* space = zalloc(sizeof(*space));
    return space;
}

void PRIM_spaceFree(PRIM_Space* space)
{
    for (u32 i = 0; i < space->expDefStack.length; ++i)
    {
        vec_free(space->expDefStack.data + i);
    }
    vec_free(&space->expDefStack);
    vec_free(&space->exps);
    vec_free(&space->strs);
    vec_free(&space->expHashs);
    vec_free(&space->strHashs);
    vec_free(&space->nodeInfoTable);
    free(space);
}

u32 PRIM_spaceNodesTotal(PRIM_Space* space)
{
    return space->nodeInfoTable.length;
}







PRIM_NodeType PRIM_nodeType(PRIM_Space* space, PRIM_Node node)
{
    PRIM_NodeInfo* info = space->nodeInfoTable.data + node.id;
    return info->type;
}









PRIM_Node PRIM_addStr(PRIM_Space* space, const char* str)
{
    u32 l = (u32)strlen(str);
    PRIM_NodeInfo info = { PRIM_NodeType_Str, space->strs.length, l };
    vec_pusharr(&space->strs, str, l + 1);
    PRIM_Node node = { space->nodeInfoTable.length };
    vec_push(&space->nodeInfoTable, info);
    return node;
}

PRIM_Node PRIM_addLenStr(PRIM_Space* space, u32 len, const char* str)
{
    PRIM_NodeInfo info = { PRIM_NodeType_Str, space->strs.length, len };
    vec_pusharr(&space->strs, str, len);
    vec_push(&space->strs, 0);
    PRIM_Node node = { space->nodeInfoTable.length };
    vec_push(&space->nodeInfoTable, info);
    return node;
}





void PRIM_addExpEnter(PRIM_Space* space)
{
    PRIM_NodeVec exp = { 0 };
    vec_push(&space->expDefStack, exp);
}

void PRIM_addExpPush(PRIM_Space* space, PRIM_Node x)
{
    assert(space->expDefStack.length > 0);
    PRIM_NodeVec* exps = space->expDefStack.data + space->expDefStack.length - 1;
    vec_push(exps, x);
}

void PRIM_addExpCancel(PRIM_Space* space)
{
    PRIM_NodeVec* exps = space->expDefStack.data + space->expDefStack.length - 1;
    for (u32 i = exps->length - 1; i != (u32)-1; --i)
    {
        PRIM_Node e = exps->data[i];
        PRIM_NodeInfo* einfo = space->nodeInfoTable.data + e.id;
        switch (einfo->type)
        {
        case PRIM_NodeType_Str:
        {
            vec_resize(&space->strs, space->strs.length - einfo->length - 1);
            assert(space->strs.length == einfo->offset);
            break;
        }
        case PRIM_NodeType_Exp:
        {
            vec_resize(&space->exps, space->exps.length - einfo->length);
            assert(space->exps.length == einfo->offset);
            break;
        }
        default:
            assert(false);
            break;
        }
    }
    vec_free(exps);
    vec_resize(&space->expDefStack, space->expDefStack.length - 1);
}

PRIM_Node PRIM_addExpDone(PRIM_Space* space)
{
    PRIM_NodeVec* exps = space->expDefStack.data + space->expDefStack.length - 1;
    PRIM_NodeInfo info = { PRIM_NodeType_Exp, space->exps.length, exps->length };
    vec_concat(&space->exps, exps);
    vec_free(exps);
    vec_resize(&space->expDefStack, space->expDefStack.length - 1);
    PRIM_Node node = { space->nodeInfoTable.length };
    vec_push(&space->nodeInfoTable, info);
    return node;
}









u32 PRIM_strSize(PRIM_Space* space, PRIM_Node node)
{
    PRIM_NodeInfo* info = space->nodeInfoTable.data + node.id;
    assert(PRIM_NodeType_Str == info->type);
    return info->length;
}

const char* PRIM_strCstr(PRIM_Space* space, PRIM_Node node)
{
    PRIM_NodeInfo* info = space->nodeInfoTable.data + node.id;
    assert(PRIM_NodeType_Str == info->type);
    return space->strs.data + info->offset;
}




u32 PRIM_expLen(PRIM_Space* space, PRIM_Node node)
{
    PRIM_NodeInfo* info = space->nodeInfoTable.data + node.id;
    assert(PRIM_NodeType_Exp == info->type);
    return info->length;
}

PRIM_Node* PRIM_expElm(PRIM_Space* space, PRIM_Node node)
{
    PRIM_NodeInfo* info = space->nodeInfoTable.data + node.id;
    assert(PRIM_NodeType_Exp == info->type);
    return space->exps.data + info->offset;
}


















static u32 PRIM_saveExpSL
(
    const PRIM_Space* space, const PRIM_NodeInfo* expInfo, char* buf, u32 bufSize,
    const PRIM_NodeSrcInfoTable* srcInfoTable
)
{
    u32 n = 0;
    u32 bufRemain = bufSize;
    char* bufPtr = buf;

    for (u32 i = 0; i < expInfo->length; ++i)
    {
        u32 en = PRIM_saveSL(space, space->exps.data[expInfo->offset + i], bufPtr, bufRemain, srcInfoTable);
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


u32 PRIM_saveSL
(
    const PRIM_Space* space, PRIM_Node node, char* buf, u32 bufSize,
    const PRIM_NodeSrcInfoTable* srcInfoTable
)
{
    PRIM_NodeInfo* info = space->nodeInfoTable.data + node.id;
    switch (info->type)
    {
    case PRIM_NodeType_Str:
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
    case PRIM_NodeType_Exp:
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

        u32 n1 = PRIM_saveExpSL(space, info, bufPtr, bufRemain, srcInfoTable);
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















typedef struct PRIM_SaveMLctx
{
    const PRIM_Space* space;
    const PRIM_SaveMLopt* opt;
    const u32 bufSize;
    char* const buf;

    u32 n;
    u32 column;
    u32 depth;
} PRIM_SaveMLctx;



static bool PRIM_saveMlForward(PRIM_SaveMLctx* ctx, u32 a)
{
    ctx->n += a;
    ctx->column += a;
    return ctx->column <= ctx->opt->width;
}

static void PRIM_saveMlBack(PRIM_SaveMLctx* ctx, u32 a)
{
    assert(ctx->n >= a);
    assert(ctx->column >= a);
    ctx->n -= a;
    ctx->column -= a;
}




static void PRIM_saveMlAdd(PRIM_SaveMLctx* ctx, const char* s)
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



static void PRIM_saveMlAddIdent(PRIM_SaveMLctx* ctx)
{
    u32 n = ctx->opt->indent * ctx->depth;
    for (u32 i = 0; i < n; ++i)
    {
        PRIM_saveMlAdd(ctx, " ");
    }
}







static void PRIM_saveMlAddNode(PRIM_SaveMLctx* ctx, PRIM_Node node);


static void PRIM_saveMlAddExp(PRIM_SaveMLctx* ctx, const PRIM_NodeInfo* expInfo)
{
    const PRIM_Space* space = ctx->space;
    for (u32 i = 0; i < expInfo->length; ++i)
    {
        PRIM_saveMlAddIdent(ctx);

        PRIM_saveMlAddNode(ctx, space->exps.data[expInfo->offset + i]);

        PRIM_saveMlAdd(ctx, "\n");
    }
}




static void PRIM_saveMlAddNodeExp(PRIM_SaveMLctx* ctx, PRIM_Node node)
{
    const PRIM_Space* space = ctx->space;

    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
    u32 a = PRIM_saveSL(space, node, bufPtr, bufRemain, ctx->opt->srcInfoTable);
    bool ok = PRIM_saveMlForward(ctx, a);

    if (!ok)
    {
        PRIM_NodeInfo* info = space->nodeInfoTable.data + node.id;

        PRIM_saveMlBack(ctx, a);

        PRIM_saveMlAdd(ctx, "[\n");

        ++ctx->depth;
        PRIM_saveMlAddExp(ctx, info);
        --ctx->depth;

        PRIM_saveMlAddIdent(ctx);
        PRIM_saveMlAdd(ctx, "]");
    }
}



static void PRIM_saveMlAddNode(PRIM_SaveMLctx* ctx, PRIM_Node node)
{
    const PRIM_Space* space = ctx->space;
    PRIM_NodeInfo* info = space->nodeInfoTable.data + node.id;
    switch (info->type)
    {
    case PRIM_NodeType_Str:
    {
        u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
        char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
        u32 a = PRIM_saveSL(space, node, bufPtr, bufRemain, ctx->opt->srcInfoTable);
        PRIM_saveMlForward(ctx, a);
        return;
    }
    case PRIM_NodeType_Exp:
    {
        PRIM_saveMlAddNodeExp(ctx, node);
        return;
    }
    default:
        assert(false);
        return;
    }
}



















u32 PRIM_saveML(const PRIM_Space* space, PRIM_Node node, char* buf, u32 bufSize, const PRIM_SaveMLopt* opt)
{
    PRIM_NodeInfo* info = space->nodeInfoTable.data + node.id;
    switch (info->type)
    {
    case PRIM_NodeType_Str:
    {
        return PRIM_saveSL(space, node, buf, bufSize, opt->srcInfoTable);
    }
    case PRIM_NodeType_Exp:
    {
        PRIM_SaveMLctx ctx =
        {
            space, opt, bufSize, buf,
        };
        PRIM_saveMlAddNodeExp(&ctx, node);
        return ctx.n;
    }
    default:
        assert(false);
        return 0;
    }
}




























































































































