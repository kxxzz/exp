#include "exp_eval_a.h"






static void EXP_evalArrayInit(EXP_EvalArray* a, u32 size, EXP_EvalContext* ctx)
{
    a->elmSize = 1;
    EXP_evalArrayResize(a, size);
    APNUM_pool_t pool = ctx->numPool;
    for (u32 i = 0; i < a->size; ++i)
    {
        APNUM_rat* n = APNUM_ratNew(pool);
        APNUM_ratFromU32(pool, n, i, 1, false);
        EXP_EvalValue v[1] = { {.a = n } };
        EXP_evalArraySetElm(a, i, v);
    }
}

static void EXP_evalArrayFree(EXP_EvalArray* a)
{
    vec_free(&a->data);
}







EXP_EvalObjectTable EXP_newEvalObjectTable(EXP_EvalAtypeInfoVec* atypeTable)
{
    EXP_EvalObjectTable _objectTable = { 0 };
    EXP_EvalObjectTable* objectTable = &_objectTable;
    vec_resize(objectTable, atypeTable->length + 1);
    memset(objectTable->data, 0, sizeof(objectTable->data[0])*objectTable->length);
    return *objectTable;
}




void EXP_evalObjectTableFree(EXP_EvalObjectTable* objectTable, EXP_EvalAtypeInfoVec* atypeTable)
{
    assert(atypeTable->length + 1 == objectTable->length);

    for (u32 i = 0; i < atypeTable->length; ++i)
    {
        EXP_EvalObjectPtrVec* mpVec = objectTable->data + i;
        EXP_EvalAtypeInfo* atypeInfo = atypeTable->data + i;
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

    {
        EXP_EvalObjectPtrVec* mpVec = &vec_last(objectTable);
        for (u32 i = 0; i < mpVec->length; ++i)
        {
            EXP_evalArrayFree((EXP_EvalArray*)mpVec->data[i]->a);
            free(mpVec->data[i]);
        }
        vec_free(mpVec);
    }
    vec_free(objectTable);
}












void EXP_evalSetGcFlag(EXP_EvalContext* ctx, EXP_EvalValue v)
{
    switch (v.type)
    {
    case EXP_EvalValueType_Object:
    {
        EXP_EvalObject* m = (EXP_EvalObject*)((char*)v.a - offsetof(EXP_EvalObject, a));
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







void EXP_evalGC(EXP_EvalContext* ctx)
{
    EXP_EvalValueVec* varStack = &ctx->varStack;
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
    EXP_EvalAtypeInfoVec* atypeTable = &ctx->atypeTable;
    EXP_EvalObjectTable* objectTable = &ctx->objectTable;

    ctx->gcFlag = !ctx->gcFlag;

    for (u32 i = 0; i < varStack->length; ++i)
    {
        EXP_evalSetGcFlag(ctx, varStack->data[i]);
    }
    for (u32 i = 0; i < dataStack->length; ++i)
    {
        EXP_evalSetGcFlag(ctx, dataStack->data[i]);
    }

    assert(atypeTable->length + 1 == objectTable->length);

    for (u32 i = 0; i < atypeTable->length; ++i)
    {
        EXP_EvalObjectPtrVec* mpVec = objectTable->data + i;
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

    {
        EXP_EvalObjectPtrVec* mpVec = &vec_last(objectTable);
        for (u32 i = 0; i < mpVec->length; ++i)
        {
            bool gcFlag = mpVec->data[i]->gcFlag;
            if (gcFlag != ctx->gcFlag)
            {
                EXP_evalArrayFree((EXP_EvalArray*)mpVec->data[i]->a);
                free(mpVec->data[i]);

                mpVec->data[i] = vec_last(mpVec);
                vec_pop(mpVec);
                --i;
            }
        }
    }
}









EXP_EvalValue EXP_evalNewAtom(EXP_EvalContext* ctx, const char* str, u32 len, u32 atype)
{
    EXP_EvalAtypeInfoVec* atypeTable = &ctx->atypeTable;
    EXP_EvalObjectTable* objectTable = &ctx->objectTable;

    EXP_EvalValue v = { 0 };
    EXP_EvalAtypeInfo* atypeInfo = atypeTable->data + atype;
    if (atypeInfo->allocMemSize > 0)
    {
        EXP_EvalObject* m = (EXP_EvalObject*)zalloc(offsetof(EXP_EvalObject, a) + atypeInfo->allocMemSize);
        v.a = m->a;
        EXP_EvalObjectPtrVec* mpVec = objectTable->data + atype;
        vec_push(mpVec, m);
        v.type = EXP_EvalValueType_Object;
        m->gcFlag = ctx->gcFlag;
    }
    assert(atypeInfo->ctorByStr);
    if (!atypeInfo->ctorByStr(&v, str, len, ctx))
    {
        assert(false);
    }
    return v;
}













EXP_EvalValue EXP_evalNewArray(EXP_EvalContext* ctx, u32 size)
{
    EXP_EvalAtypeInfoVec* atypeTable = &ctx->atypeTable;
    EXP_EvalObjectTable* objectTable = &ctx->objectTable;

    EXP_EvalValue v = { 0 };
    EXP_EvalObject* m = (EXP_EvalObject*)zalloc(offsetof(EXP_EvalObject, a) + sizeof(EXP_EvalArray));
    v.a = m->a;
    EXP_EvalObjectPtrVec* mpVec = objectTable->data + atypeTable->length;
    vec_push(mpVec, m);
    v.type = EXP_EvalValueType_Object;
    m->gcFlag = ctx->gcFlag;
    EXP_evalArrayInit((EXP_EvalArray*)(v.a), size, ctx);
    return v;
}











































