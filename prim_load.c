#include "a.h"



typedef enum PRIM_TokenType
{
    PRIM_TokenType_Text,
    PRIM_TokenType_String,

    PRIM_TokenType_ExpBegin,
    PRIM_TokenType_ExpEnd,

    PRIM_NumTokenTypes
} PRIM_TokenType;



typedef struct PRIM_Token
{
    PRIM_TokenType type;
    u32 begin;
    u32 len;
} PRIM_Token;



typedef struct PRIM_LoadContext
{
    PRIM_Space* space;
    u32 srcLen;
    const char* src;
    u32 cur;
    u32 curLine;
    PRIM_NodeSrcInfoTable* srcInfoTable;
    vec_char tmpStrBuf;
} PRIM_LoadContext;



static PRIM_LoadContext PRIM_newLoadContext
(
    PRIM_Space* space, u32 strSize, const char* str, PRIM_NodeSrcInfoTable* srcInfoTable
)
{
    assert(strSize == strlen(str));
    PRIM_LoadContext ctx = { space, strSize, str, 0, 1, srcInfoTable };
    return ctx;
}

static void PRIM_loadContextFree(PRIM_LoadContext* ctx)
{
    vec_free(&ctx->tmpStrBuf);
}





static bool PRIM_skipSapce(PRIM_LoadContext* ctx)
{
    const char* src = ctx->src;
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            return false;
        }
        else if (' ' >= src[ctx->cur])
        {
            if ('\n' == src[ctx->cur])
            {
                ++ctx->curLine;
            }
            ++ctx->cur;
            continue;
        }
        else if (ctx->cur + 1 < ctx->srcLen)
        {
            if (0 == strncmp("//", src + ctx->cur, 2))
            {
                ctx->cur += 2;
                for (;;)
                {
                    if (ctx->cur >= ctx->srcLen)
                    {
                        return false;
                    }
                    else if ('\n' == src[ctx->cur])
                    {
                        ++ctx->cur;
                        ++ctx->curLine;
                        break;
                    }
                    ++ctx->cur;
                }
                continue;
            }
            else if (0 == strncmp("/*", src + ctx->cur, 2))
            {
                ctx->cur += 2;
                u32 n = 1;
                for (;;)
                {
                    if (ctx->cur >= ctx->srcLen)
                    {
                        return false;
                    }
                    else if (ctx->cur + 1 < ctx->srcLen)
                    {
                        if (0 == strncmp("/*", src + ctx->cur, 2))
                        {
                            ++n;
                            ctx->cur += 2;
                            continue;
                        }
                        else if (0 == strncmp("*/", src + ctx->cur, 2))
                        {
                            --n;
                            if (0 == n)
                            {
                                ctx->cur += 2;
                                break;
                            }
                            ctx->cur += 2;
                            continue;
                        }
                    }
                    else if ('\n' == src[ctx->cur])
                    {
                        ++ctx->curLine;
                    }
                    ++ctx->cur;
                }
                continue;
            }
        }
        break;
    }
    return true;
}












static bool PRIM_readToken_String(PRIM_LoadContext* ctx, PRIM_Token* out)
{
    const char* src = ctx->src;
    char endCh = src[ctx->cur];
    ++ctx->cur;
    PRIM_Token tok = { PRIM_TokenType_String, ctx->cur, 0 };
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            return false;
        }
        else if (endCh == src[ctx->cur])
        {
            break;
        }
        else if ('\\' == src[ctx->cur])
        {
            ctx->cur += 2;
            continue;
        }
        else
        {
            if ('\n' == src[ctx->cur])
            {
                ++ctx->curLine;
            }
            ++ctx->cur;
        }
    }
    tok.len = ctx->cur - tok.begin;
    assert(tok.len > 0);
    *out = tok;
    ++ctx->cur;
    return true;
}



static bool PRIM_readToken_Text(PRIM_LoadContext* ctx, PRIM_Token* out)
{
    PRIM_Token tok = { PRIM_TokenType_Text, ctx->cur, 0 };
    const char* src = ctx->src;
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            break;
        }
        else if (strchr("[]\"' \t\n\r\b\f", src[ctx->cur]))
        {
            break;
        }
        else
        {
            ++ctx->cur;
        }
    }
    tok.len = ctx->cur - tok.begin;
    assert(tok.len > 0);
    *out = tok;
    return true;
}




static bool PRIM_readToken(PRIM_LoadContext* ctx, PRIM_Token* out)
{
    const char* src = ctx->src;
    if (ctx->cur >= ctx->srcLen)
    {
        return false;
    }
    if (!PRIM_skipSapce(ctx))
    {
        return false;
    }
    bool ok = false;
    if ('[' == src[ctx->cur])
    {
        PRIM_Token tok = { PRIM_TokenType_ExpBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (']' == src[ctx->cur])
    {
        PRIM_Token tok = { PRIM_TokenType_ExpEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (('"' == src[ctx->cur]) || ('\'' == src[ctx->cur]))
    {
        ok = PRIM_readToken_String(ctx, out);
    }
    else
    {
        ok = PRIM_readToken_Text(ctx, out);
    }
    return ok;
}









static PRIM_Node PRIM_loadNode(PRIM_LoadContext* ctx);

static bool PRIM_loadEnd(PRIM_LoadContext* ctx)
{
    assert(ctx->srcLen >= ctx->cur);
    if (ctx->srcLen == ctx->cur)
    {
        return true;
    }
    return false;
}

static bool PRIM_loadExpEnd(PRIM_LoadContext* ctx)
{
    if (PRIM_loadEnd(ctx))
    {
        return true;
    }
    u32 cur0 = ctx->cur;
    u32 curLine0 = ctx->curLine;
    PRIM_Token tok;
    if (!PRIM_readToken(ctx, &tok))
    {
        return true;
    }
    switch (tok.type)
    {
    case PRIM_TokenType_ExpEnd:
    {
        return true;
    }
    default:
        break;
    }
    ctx->cur = cur0;
    ctx->curLine = curLine0;
    return false;
}

static PRIM_Node PRIM_loadExp(PRIM_LoadContext* ctx)
{
    PRIM_Space* space = ctx->space;
    PRIM_addExpEnter(space);
    while (!PRIM_loadExpEnd(ctx))
    {
        PRIM_Node e = PRIM_loadNode(ctx);
        if (PRIM_InvalidNodeId == e.id)
        {
            PRIM_addExpCancel(space);
            PRIM_Node node = { PRIM_InvalidNodeId };
            return node;
        }
        PRIM_addExpPush(ctx->space, e);
    }
    PRIM_Node node = PRIM_addExpDone(space);
    return node;
}



static void PRIM_loadNodeSrcInfo(PRIM_LoadContext* ctx, const PRIM_Token* tok, PRIM_NodeSrcInfo* info)
{
    info->offset = ctx->cur;
    info->line = ctx->curLine;
    info->column = 1;
    for (u32 i = 0; i < info->offset; ++i)
    {
        char c = ctx->src[info->offset - i];
        if (strchr("\n\r", c))
        {
            info->column = i;
            break;
        }
    }
    info->isStrTok = PRIM_TokenType_String == tok->type;
}



static PRIM_Node PRIM_loadNode(PRIM_LoadContext* ctx)
{
    PRIM_Space* space = ctx->space;
    PRIM_NodeSrcInfoTable* srcInfoTable = ctx->srcInfoTable;
    PRIM_Node node = { PRIM_InvalidNodeId };
    PRIM_Token tok;
    if (!PRIM_readToken(ctx, &tok))
    {
        return node;
    }
    PRIM_NodeSrcInfo srcInfo = { true };
    PRIM_loadNodeSrcInfo(ctx, &tok, &srcInfo);
    switch (tok.type)
    {
    case PRIM_TokenType_Text:
    {
        const char* str = ctx->src + tok.begin;
        node = PRIM_addLenStr(space, tok.len, str);
        break;
    }
    case PRIM_TokenType_String:
    {
        char endCh = ctx->src[tok.begin - 1];
        const char* src = ctx->src + tok.begin;
        u32 n = 0;
        for (u32 i = 0; i < tok.len; ++i)
        {
            if ('\\' == src[i])
            {
                ++n;
                ++i;
            }
        }
        u32 len = tok.len - n;
        vec_resize(&ctx->tmpStrBuf, len + 1);
        u32 si = 0;
        for (u32 i = 0; i < tok.len; ++i)
        {
            if ('\\' == src[i])
            {
                ++i;
                ctx->tmpStrBuf.data[si++] = src[i];
                continue;
            }
            ctx->tmpStrBuf.data[si++] = src[i];
        }
        ctx->tmpStrBuf.data[len] = 0;
        assert(si == len);
        node = PRIM_addLenStr(space, len, ctx->tmpStrBuf.data);
        break;
    }
    case PRIM_TokenType_ExpBegin:
    {
        node = PRIM_loadExp(ctx);
        if (PRIM_InvalidNodeId == node.id)
        {
            return node;
        }
        break;
    }
    default:
        assert(false);
        return node;
    }
    if (srcInfoTable)
    {
        vec_push(srcInfoTable, srcInfo);
    }
    return node;
}













PRIM_Node PRIM_loadSrcAsCell(PRIM_Space* space, const char* src, PRIM_NodeSrcInfoTable* srcInfoTable)
{
    PRIM_LoadContext ctx = PRIM_newLoadContext(space, (u32)strlen(src), src, srcInfoTable);
    PRIM_Node node = PRIM_loadNode(&ctx);
    if (!PRIM_loadEnd(&ctx))
    {
        PRIM_loadContextFree(&ctx);
        PRIM_Node node = { PRIM_InvalidNodeId };
        return node;
    }
    PRIM_loadContextFree(&ctx);
    return node;
}

PRIM_Node PRIM_loadSrcAsList(PRIM_Space* space, const char* src, PRIM_NodeSrcInfoTable* srcInfoTable)
{
    PRIM_LoadContext ctx = PRIM_newLoadContext(space, (u32)strlen(src), src, srcInfoTable);
    PRIM_addExpEnter(space);
    for (;;)
    {
        PRIM_Node e = PRIM_loadNode(&ctx);
        if (PRIM_InvalidNodeId == e.id) break;
        PRIM_addExpPush(ctx.space, e);
    }
    if (!PRIM_loadEnd(&ctx))
    {
        PRIM_loadContextFree(&ctx);
        PRIM_addExpCancel(space);
        PRIM_Node node = { PRIM_InvalidNodeId };
        return node;
    }
    PRIM_loadContextFree(&ctx);
    PRIM_Node node = PRIM_addExpDone(space);
    return node;
}




























































































