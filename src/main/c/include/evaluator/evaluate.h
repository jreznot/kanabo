#pragma once

#include "evaluator.h"
#include "evaluator/debug.h"

struct evaluator_s
{
    const DocumentModel *model;
    const JsonPath      *path;
    size_t               current_step;
    Nodelist            *results;
    EvaluatorErrorCode   code;
};

typedef struct evaluator_s Evaluator;

#define evaluate_nodelist(NAME, TEST, FUNCTION)                         \
    evaluator_trace("evaluating %s across %zu nodes", (NAME), nodelist_length(evaluator->results)); \
    Nodelist *result = nodelist_map(evaluator->results, (FUNCTION), evaluator); \
    evaluator_trace("%s: %s", (TEST), NULL == result ? "failed" : "completed"); \
    evaluator_trace("%s: added %zu nodes", (NAME), nodelist_length(result)); \
    return NULL == result ? false : (nodelist_free(evaluator->results), evaluator->results = result, true);

#define current_step(CTX) ((Step *)vector_get((CTX)->path->steps, (CTX)->current_step))

bool add_to_nodelist_sequence_iterator(Node *each, void *context);
