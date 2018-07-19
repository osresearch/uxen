#include "bdd.hxx"
#include "perfcnt.h"

#undef PERFCNT
#define PERFCNT(id) {# id},

static struct {
    const char *name;
} perfcnt_desc[PERFCNT_MAX] = {
#include "perfcnt_defs.h"
};

__int64 perfcnt[PERFCNT_MAX];

void perfcnt_dump()
{
    int i;

    for (i = 0; i < ARRAYSIZE(perfcnt_desc); i++)
        uxen_msg("%d. %s: %I64d", i, perfcnt_desc[i].name, perfcnt[i]);
}
