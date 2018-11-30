#include "a.h"



void saga_cellFree(saga_Cell* cell)
{
    switch (cell->type)
    {
    case saga_CellType_Str:
    {
        vec_free(&cell->str);
        break;
    }
    case saga_CellType_Vec:
    {
        saga_vecFree(&cell->vec);
        break;
    }
    default:
        assert(false);
        break;
    }
}

void saga_cellDup(saga_Cell* a, const saga_Cell* b)
{
    memset(a, 0, sizeof(*a));
    a->type = b->type;
    switch (b->type)
    {
    case saga_CellType_Str:
    {
        vec_dup(&a->str, &b->str);
        break;
    }
    case saga_CellType_Vec:
    {
        saga_vecDup(&a->vec, &b->vec);
        break;
    }
    default:
        assert(false);
        break;
    }
    a->hasSrcInfo = b->hasSrcInfo;
    a->srcInfo = b->srcInfo;
}









void saga_vecFree(saga_Vec* vec)
{
    for (u32 i = 0; i < vec->length; ++i)
    {
        saga_cellFree(vec->data + i);
    }
    vec_free(vec);
}

void saga_vecDup(saga_Vec* vec, const saga_Vec* a)
{
    for (u32 i = 0; i < vec->length; ++i)
    {
        saga_cellFree(vec->data + i);
    }
    vec->length = 0;
    vec_reserve(vec, a->length);
    for (u32 i = 0; i < a->length; ++i)
    {
        saga_Cell e;
        saga_cellDup(&e, &a->data[i]);
        vec_push(vec, e);
    }
}

void saga_vecConcat(saga_Vec* vec, const saga_Vec* a)
{
    vec_reserve(vec, vec->length + a->length);
    for (u32 i = 0; i < a->length; ++i)
    {
        saga_Cell e;
        saga_cellDup(&e, &a->data[i]);
        vec_push(vec, e);
    }
}












saga_Cell saga_cellStr(const char* s)
{
    saga_Cell cell = { saga_CellType_Str };
    vec_pusharr(&cell.str, s, (u32)strlen(s) + 1);
    return cell;
}

u32 saga_strLen(saga_Cell* a)
{
    assert(saga_CellType_Str == a->type);
    return a->str.length > 0 ? a->str.length - 1 : 0;
}













u32 saga_psVec(const saga_Vec* vec, char* buf, u32 bufSize, bool withSrcInfo)
{
    u32 n = 0;
    u32 bufRemain = bufSize;
    char* bufPtr = buf;

    for (u32 i = 0; i < vec->length; ++i)
    {
        u32 en = saga_ps(vec->data + i, bufPtr, bufRemain, withSrcInfo);
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

        if (i < vec->length - 1)
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


u32 saga_ps(const saga_Cell* cell, char* buf, u32 bufSize, bool withSrcInfo)
{
    switch (cell->type)
    {
    case saga_CellType_Str:
    {
        const char* str = cell->str.data;
        u32 sreLen = (cell->str.length > 0) ? cell->str.length - 1 : 0;
        u32 n;
        bool isStrTok = false;
        if (withSrcInfo && cell->hasSrcInfo)
        {
            isStrTok = cell->srcInfo.isStrTok;
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
            n = snprintf(buf, bufSize, "%s", cell->str.data);
        }
        return n;
    }
    case saga_CellType_Vec:
    {
        u32 n = 0;
        const saga_Vec* vec = &cell->vec;

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

        u32 n1 = saga_psVec(vec, bufPtr, bufRemain, withSrcInfo);
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















typedef struct saga_PPctx
{
    const saga_PPopt* opt;
    const u32 bufSize;
    char* const buf;

    u32 n;
    u32 column;
    u32 depth;
} saga_PPctx;



static bool saga_ppForward(saga_PPctx* ctx, u32 a)
{
    ctx->n += a;
    ctx->column += a;
    return ctx->column <= ctx->opt->width;
}

static void saga_ppBack(saga_PPctx* ctx, u32 a)
{
    assert(ctx->n >= a);
    assert(ctx->column >= a);
    ctx->n -= a;
    ctx->column -= a;
}




static void saga_ppAdd(saga_PPctx* ctx, const char* s)
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
            strncpy(bufPtr, s, wn);
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



static void saga_ppAddIdent(saga_PPctx* ctx)
{
    u32 n = ctx->opt->indent * ctx->depth;
    for (u32 i = 0; i < n; ++i)
    {
        saga_ppAdd(ctx, " ");
    }
}







static void saga_ppAddCell(saga_PPctx* ctx, const saga_Cell* cell, bool withSrcInfo);


static void saga_ppAddVec(saga_PPctx* ctx, const saga_Vec* vec, bool withSrcInfo)
{
    for (u32 i = 0; i < vec->length; ++i)
    {
        saga_ppAddIdent(ctx);

        saga_ppAddCell(ctx, vec->data + i, withSrcInfo);

        saga_ppAdd(ctx, "\n");
    }
}




static void saga_ppAddCellVec(saga_PPctx* ctx, const saga_Vec* vec, bool withSrcInfo)
{
    saga_Cell cell = { saga_CellType_Vec, .vec = *vec };
    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
    u32 a = saga_ps(&cell, bufPtr, bufRemain, withSrcInfo);
    bool ok = saga_ppForward(ctx, a);

    if (!ok)
    {
        saga_ppBack(ctx, a);

        saga_ppAdd(ctx, "[\n");

        ++ctx->depth;
        saga_ppAddVec(ctx, vec, withSrcInfo);
        --ctx->depth;

        saga_ppAddIdent(ctx);
        saga_ppAdd(ctx, "]");
    }
}



static void saga_ppAddCell(saga_PPctx* ctx, const saga_Cell* cell, bool withSrcInfo)
{
    switch (cell->type)
    {
    case saga_CellType_Str:
    {
        u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
        char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
        u32 a = saga_ps(cell, bufPtr, bufRemain, withSrcInfo);
        saga_ppForward(ctx, a);
        return;
    }
    case saga_CellType_Vec:
    {
        const saga_Vec* vec = &cell->vec;
        saga_ppAddCellVec(ctx, vec, withSrcInfo);
        return;
    }
    default:
        assert(false);
        return;
    }
}














u32 saga_ppVec(const saga_Vec* vec, char* buf, u32 bufSize, const saga_PPopt* opt)
{
    saga_PPctx ctx =
    {
        opt, bufSize, buf,
    };
    saga_ppAddVec(&ctx, vec, opt->withSrcInfo);
    return ctx.n;
}




u32 saga_pp(const saga_Cell* cell, char* buf, u32 bufSize, const saga_PPopt* opt)
{
    switch (cell->type)
    {
    case saga_CellType_Str:
    {
        return saga_ps(cell, buf, bufSize, opt->withSrcInfo);
    }
    case saga_CellType_Vec:
    {
        saga_PPctx ctx =
        {
            opt, bufSize, buf,
        };
        saga_ppAddCellVec(&ctx, &cell->vec, opt->withSrcInfo);
        return ctx.n;
    }
    default:
        assert(false);
        return 0;
    }
}









char* saga_ppVecAc(const saga_Vec* vec, const saga_PPopt* opt)
{
    u32 bufSize = saga_ppVec(vec, NULL, 0, opt) + 1;
    char* buf = (char*)malloc(bufSize);
    saga_ppVec(vec, buf, bufSize, opt);
    return buf;
}


char* saga_ppAc(const saga_Cell* cell, const saga_PPopt* opt)
{
    u32 bufSize = saga_pp(cell, NULL, 0, opt) + 1;
    char* buf = (char*)malloc(bufSize);
    saga_pp(cell, buf, bufSize, opt);
    return buf;
}



















































































































