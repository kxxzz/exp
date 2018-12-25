#include "a.h"



typedef enum saga_TokenType
{
    saga_TokenType_Text,
    saga_TokenType_String,

    saga_TokenType_VecBegin,
    saga_TokenType_VecEnd,

    saga_NumTokenTypes
} saga_TokenType;



typedef struct saga_Token
{
    saga_TokenType type;
    u32 begin;
    u32 len;
} saga_Token;



typedef struct saga_LoadContext
{
    u32 srcLen;
    const char* src;
    u32 cur;
    u32 curLine;
} saga_LoadContext;



static saga_LoadContext saga_newLoadContext(u32 strSize, const char* str)
{
    assert(strSize == strlen(str));
    saga_LoadContext ctx = { strSize, str, 0, 1 };
    return ctx;
}





static bool saga_skipSapce(saga_LoadContext* ctx)
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












static bool saga_readToken_String(saga_LoadContext* ctx, saga_Token* out)
{
    const char* src = ctx->src;
    char endCh = src[ctx->cur];
    ++ctx->cur;
    saga_Token tok = { saga_TokenType_String, ctx->cur, 0 };
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



static bool saga_readToken_Text(saga_LoadContext* ctx, saga_Token* out)
{
    saga_Token tok = { saga_TokenType_Text, ctx->cur, 0 };
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




static bool saga_readToken(saga_LoadContext* ctx, saga_Token* out)
{
    const char* src = ctx->src;
    if (ctx->cur >= ctx->srcLen)
    {
        return false;
    }
    if (!saga_skipSapce(ctx))
    {
        return false;
    }
    bool ok = false;
    if ('[' == src[ctx->cur])
    {
        saga_Token tok = { saga_TokenType_VecBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (']' == src[ctx->cur])
    {
        saga_Token tok = { saga_TokenType_VecEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (('"' == src[ctx->cur]) || ('\'' == src[ctx->cur]))
    {
        ok = saga_readToken_String(ctx, out);
    }
    else
    {
        ok = saga_readToken_Text(ctx, out);
    }
    return ok;
}









static saga_Node* saga_loadNode(saga_LoadContext* ctx);

static bool saga_loadEnd(saga_LoadContext* ctx)
{
    assert(ctx->srcLen >= ctx->cur);
    if (ctx->srcLen == ctx->cur)
    {
        return true;
    }
    return false;
}
static bool saga_loadVecEnd(saga_LoadContext* ctx)
{
    if (saga_loadEnd(ctx))
    {
        return true;
    }
    saga_LoadContext ctx0 = *ctx;
    saga_Token tok;
    if (!saga_readToken(ctx, &tok))
    {
        return true;
    }
    switch (tok.type)
    {
    case saga_TokenType_VecEnd:
    {
        return true;
    }
    default:
        break;
    }
    *ctx = ctx0;
    return false;
}

static bool saga_loadVec(saga_LoadContext* ctx, saga_Node* node)
{
    while (!saga_loadVecEnd(ctx))
    {
        saga_Node* e = NULL;
        e = saga_loadNode(ctx);
        if (!e)
        {
            return false;
        }
        saga_vecPush(node, e);
    }
    return true;
}



static void saga_loadNodeSrcInfo(saga_LoadContext* ctx, const saga_Token* tok, saga_NodeSrcInfo* info)
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
    info->isStrTok = saga_TokenType_String == tok->type;
}



static saga_Node* saga_loadNode(saga_LoadContext* ctx)
{
    saga_Token tok;
    if (!saga_readToken(ctx, &tok))
    {
        return false;
    }
    saga_NodeSrcInfo srcInfo;
    saga_loadNodeSrcInfo(ctx, &tok, &srcInfo);
    saga_Node* node = zalloc(sizeof(*node));
    switch (tok.type)
    {
    case saga_TokenType_Text:
    {
        node->type = saga_NodeType_Str;
        const char* src = ctx->src + tok.begin;
        u32 strLen = tok.len;
        vec_resize(&node->str, strLen + 1);
        stzncpy(node->str.data, src, tok.len + 1);
        node->str.data[tok.len] = 0;
        break;
    }
    case saga_TokenType_String:
    {
        node->type = saga_NodeType_Str;
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
    case saga_TokenType_VecBegin:
    {
        node->type = saga_NodeType_Vec;
        bool ok = saga_loadVec(ctx, node);
        if (!ok)
        {
            saga_nodeFree(node);
            return NULL;
        }
        break;
    }
    default:
        assert(false);
        return NULL;
    }
    node->hasSrcInfo = true;
    node->srcInfo = srcInfo;
    return node;
}













saga_Node* saga_loadCell(const char* str)
{
    saga_LoadContext ctx = saga_newLoadContext((u32)strlen(str), str);
    saga_Node* node = saga_loadNode(&ctx);
    if (!saga_loadEnd(&ctx))
    {
        saga_nodeFree(node);
        return NULL;
    }
    return node;
}

saga_Node* saga_loadSeq(const char* str)
{
    saga_LoadContext ctx = saga_newLoadContext((u32)strlen(str), str);
    saga_Node* node = saga_vec();
    for (;;)
    {
        saga_Node* e = saga_loadNode(&ctx);
        if (!e) break;
        saga_vecPush(node, e);
    }
    if (!saga_loadEnd(&ctx))
    {
        saga_nodeFree(node);
        return NULL;
    }
    return node;
}




























































































