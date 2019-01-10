#include "a.h"



typedef enum PRIM_TokenType
{
    PRIM_TokenType_Text,
    PRIM_TokenType_String,

    PRIM_TokenType_ExprBegin,
    PRIM_TokenType_ExprEnd,

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
    u32 srcLen;
    const char* src;
    u32 cur;
    u32 curLine;
} PRIM_LoadContext;



static PRIM_LoadContext PRIM_newLoadContext(u32 strSize, const char* str)
{
    assert(strSize == strlen(str));
    PRIM_LoadContext ctx = { strSize, str, 0, 1 };
    return ctx;
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
        PRIM_Token tok = { PRIM_TokenType_ExprBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (']' == src[ctx->cur])
    {
        PRIM_Token tok = { PRIM_TokenType_ExprEnd, ctx->cur, 1 };
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









static PRIM_NodeBody* PRIM_loadNode(PRIM_LoadContext* ctx);

static bool PRIM_loadEnd(PRIM_LoadContext* ctx)
{
    assert(ctx->srcLen >= ctx->cur);
    if (ctx->srcLen == ctx->cur)
    {
        return true;
    }
    return false;
}
static bool PRIM_loadExprEnd(PRIM_LoadContext* ctx)
{
    if (PRIM_loadEnd(ctx))
    {
        return true;
    }
    PRIM_LoadContext ctx0 = *ctx;
    PRIM_Token tok;
    if (!PRIM_readToken(ctx, &tok))
    {
        return true;
    }
    switch (tok.type)
    {
    case PRIM_TokenType_ExprEnd:
    {
        return true;
    }
    default:
        break;
    }
    *ctx = ctx0;
    return false;
}

static bool PRIM_loadExpr(PRIM_LoadContext* ctx, PRIM_NodeBody* node)
{
    while (!PRIM_loadExprEnd(ctx))
    {
        PRIM_NodeBody* e = NULL;
        e = PRIM_loadNode(ctx);
        if (!e)
        {
            return false;
        }
        PRIM_exprPush(node, e);
    }
    return true;
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



static PRIM_NodeBody* PRIM_loadNode(PRIM_LoadContext* ctx)
{
    PRIM_Token tok;
    if (!PRIM_readToken(ctx, &tok))
    {
        return false;
    }
    PRIM_NodeSrcInfo srcInfo = { true };
    PRIM_loadNodeSrcInfo(ctx, &tok, &srcInfo);
    PRIM_NodeBody* node = zalloc(sizeof(*node));
    switch (tok.type)
    {
    case PRIM_TokenType_Text:
    {
        node->type = PRIM_NodeType_Atom;
        const char* src = ctx->src + tok.begin;
        u32 strLen = tok.len;
        vec_resize(&node->str, strLen + 1);
        stzncpy(node->str.data, src, tok.len + 1);
        node->str.data[tok.len] = 0;
        break;
    }
    case PRIM_TokenType_String:
    {
        node->type = PRIM_NodeType_Atom;
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
        u32 strLen = tok.len - n;
        vec_resize(&node->str, strLen + 1);
        u32 si = 0;
        for (u32 i = 0; i < tok.len; ++i)
        {
            if ('\\' == src[i])
            {
                ++i;
                node->str.data[si++] = src[i];
                continue;
            }
            node->str.data[si++] = src[i];
        }
        node->str.data[tok.len] = 0;
        assert(si == strLen);
        break;
    }
    case PRIM_TokenType_ExprBegin:
    {
        node->type = PRIM_NodeType_Expr;
        bool ok = PRIM_loadExpr(ctx, node);
        if (!ok)
        {
            PRIM_nodeFree(node);
            return NULL;
        }
        break;
    }
    default:
        assert(false);
        return NULL;
    }
    node->srcInfo = srcInfo;
    return node;
}













PRIM_NodeBody* PRIM_loadCell(PRIM_Space* space, const char* str, PRIM_NodeSrcInfoTable* srcInfoTable)
{
    PRIM_LoadContext ctx = PRIM_newLoadContext((u32)strlen(str), str);
    PRIM_NodeBody* node = PRIM_loadNode(&ctx);
    if (!PRIM_loadEnd(&ctx))
    {
        PRIM_nodeFree(node);
        return NULL;
    }
    return node;
}

PRIM_NodeBody* PRIM_loadList(PRIM_Space* space, const char* str, PRIM_NodeSrcInfoTable* srcInfoTable)
{
    PRIM_LoadContext ctx = PRIM_newLoadContext((u32)strlen(str), str);
    PRIM_NodeBody* node = PRIM_expr();
    for (;;)
    {
        PRIM_NodeBody* e = PRIM_loadNode(&ctx);
        if (!e) break;
        PRIM_exprPush(node, e);
    }
    if (!PRIM_loadEnd(&ctx))
    {
        PRIM_nodeFree(node);
        return NULL;
    }
    return node;
}




























































































