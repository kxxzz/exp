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









static bool saga_loadNode(saga_LoadContext* ctx, saga_Node* out);

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

static bool saga_loadVec(saga_LoadContext* ctx, saga_Vec* vec)
{
    bool ok;
    saga_Vec vec1 = { 0 };
    while (!saga_loadVecEnd(ctx))
    {
        saga_Node e = { 0 };
        ok = saga_loadNode(ctx, &e);
        if (!ok)
        {
            saga_vecFree(&vec1);
            return false;
        }
        vec_push(&vec1, e);
    }
    if (vec->length > 0)
    {
        saga_vecDup(vec, &vec1);
        saga_vecFree(&vec1);
    }
    else
    {
        *vec = vec1;
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



static bool saga_loadNode(saga_LoadContext* ctx, saga_Node* out)
{
    saga_Token tok;
    if (!saga_readToken(ctx, &tok))
    {
        return false;
    }
    saga_NodeSrcInfo srcInfo;
    saga_loadNodeSrcInfo(ctx, &tok, &srcInfo);
    bool ok = true;
    switch (tok.type)
    {
    case saga_TokenType_Text:
    {
        out->type = saga_NodeType_Term;
        const char* src = ctx->src + tok.begin;
        u32 strLen = tok.len;
        vec_resize(&out->str, strLen + 1);
        strncpy(out->str.data, src, tok.len);
        out->str.data[tok.len] = 0;
        break;
    }
    case saga_TokenType_String:
    {
        out->type = saga_NodeType_Term;
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
        vec_resize(&out->str, strLen + 1);
        u32 si = 0;
        for (u32 i = 0; i < tok.len; ++i)
        {
            if ('\\' == src[i])
            {
                ++i;
                out->str.data[si++] = src[i];
                continue;
            }
            out->str.data[si++] = src[i];
        }
        out->str.data[tok.len] = 0;
        assert(si == strLen);
        break;
    }
    case saga_TokenType_VecBegin:
    {
        out->type = saga_NodeType_Inode;
        memset(&out->vec, 0, sizeof(out->vec));
        ok = saga_loadVec(ctx, &out->vec);
        break;
    }
    default:
        assert(false);
        return false;
    }
    out->hasSrcInfo = true;
    out->srcInfo = srcInfo;
    return ok;
}










bool saga_load(saga_Node* node, const char* str)
{
    saga_LoadContext ctx = saga_newLoadContext((u32)strlen(str), str);
    bool ok = saga_loadNode(&ctx, node);
    return ok;
}




























































































