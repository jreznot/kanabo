#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#endif

#ifdef __APPLE__
#define _DARWIN_SOURCE
#endif

#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <tgmath.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "evaluator.h"
#include "evaluator/private.h"
#include "log.h"
#include "conditions.h"

struct meta_context
{
    evaluator_context *context;
    nodelist *target;
};

typedef struct meta_context meta_context;

static bool evaluate_step(Step* each, void *context);
static bool evaluate_root_step(evaluator_context *context);
static bool evaluate_single_step(evaluator_context *context);
static bool evaluate_recursive_step(evaluator_context *context);
static bool evaluate_predicate(evaluator_context *context);

static bool apply_node_test(Node *each, void *argument, nodelist *target);
static bool apply_recursive_node_test(Node *each, void *argument, nodelist *target);
static bool recursive_test_sequence_iterator(Node *each, void *context);
static bool recursive_test_map_iterator(Node *key, Node *value, void *context);
static bool apply_greedy_wildcard_test(const Node *each, void *argument, nodelist *target);
static bool apply_recursive_wildcard_test(const Node *each, void *argument, nodelist *target);
static bool apply_type_test(const Node *each, void *argument, nodelist *target);
static bool apply_name_test(const Node *each, void *argument, nodelist *target);

static bool apply_predicate(Node *value, void *argument, nodelist *target);
static bool apply_wildcard_predicate(const Node *value, evaluator_context *context, nodelist *target);
static bool apply_subscript_predicate(const Sequence *value, evaluator_context *context, nodelist *target);
static bool apply_slice_predicate(const Sequence *value, evaluator_context *context, nodelist *target);
static bool apply_join_predicate(const Node *value, evaluator_context *context, nodelist *target);

static bool add_to_nodelist_sequence_iterator(Node *each, void *context);
static bool add_values_to_nodelist_map_iterator(Node *key, Node *value, void *context);
static void normalize_interval(const Sequence *value, predicate *slice, int64_t *from, int64_t *to, int64_t *step);

#define guard(EXPR) EXPR ? true : (context->code = ERR_EVALUATOR_OUT_OF_MEMORY, false)

static inline Step *current_step(evaluator_context *context)
{
    return (Step *)vector_get(context->path->steps, context->current_step);
}

evaluator_status_code evaluate_steps(const DocumentModel *model, const jsonpath *path, nodelist **list)
{
    evaluator_debug("beginning evaluation of %d steps", path_length(path));

    evaluator_context context;
    memset(&context, 0, sizeof(evaluator_context));

    *list = NULL;
    context.list = make_nodelist();
    if(NULL == context.list)
    {
        evaluator_debug("uh oh! out of memory, can't allocate the result nodelist");
        return ERR_EVALUATOR_OUT_OF_MEMORY;
    }

    context.model = model;
    context.path = path;

    nodelist_add(context.list, model_document(model, 0));

    if(!path_iterate(path, evaluate_step, &context))
    {
        evaluator_error("aborted, step: %d, code: %d (%s)", context.current_step, context.code, evaluator_status_message(context.code));
        return context.code;
    }

    evaluator_debug("done, found %d matching nodes", nodelist_length(context.list));
    *list = context.list;
    return context.code;
}

static bool evaluate_step(Step* each, void *argument)
{
    evaluator_context *context = (evaluator_context *)argument;
    evaluator_trace("step: %zu", context->current_step);

    bool result = false;
    switch(each->kind)
    {
        case ROOT:
            result = evaluate_root_step(context);
            break;
        case SINGLE:
            result = evaluate_single_step(context);
            break;
        case RECURSIVE:
            result = evaluate_recursive_step(context);
            break;
    }
    if(result && NULL != current_step(context)->predicate)
    {
        result = evaluate_predicate(context);
    }
    if(result)
    {
        context->current_step++;
    }
    return result;
}

static bool evaluate_root_step(evaluator_context *context)
{
    evaluator_trace("evaluating root step");
    Document *doc = nodelist_get(context->list, 0);
    Node *root = document_root(doc);
    evaluator_trace("root test: adding root node (%p) from document (%p)", root, doc);
    return nodelist_set(context->list, root, 0);
}

#define evaluate_nodelist(NAME, TEST, FUNCTION)                         \
    evaluator_trace("evaluating %s across %zu nodes", (NAME), nodelist_length(context->list)); \
    nodelist *result = nodelist_map(context->list, (FUNCTION), context); \
    evaluator_trace("%s: %s", (TEST), NULL == result ? "failed" : "completed"); \
    evaluator_trace("%s: added %zu nodes", (NAME), nodelist_length(result)); \
    return NULL == result ? false : (nodelist_free(context->list), context->list = result, true);

static bool evaluate_recursive_step(evaluator_context *context)
{
    evaluate_nodelist("recursive step",
                      test_kind_name(current_step(context)->test.kind),
                      apply_recursive_node_test);
}

static bool evaluate_single_step(evaluator_context *context)
{
    evaluate_nodelist("step",
                      test_kind_name(current_step(context)->test.kind),
                      apply_node_test);
}

static bool evaluate_predicate(evaluator_context *context)
{
    evaluate_nodelist("predicate",
                      predicate_kind_name(current_step(context)->predicate->kind),
                      apply_predicate);
}

static bool apply_recursive_node_test(Node *each, void *argument, nodelist *target)
{
    evaluator_context *context = (evaluator_context *)argument;
    bool result = true;
    if(!is_alias(each))
    {
        result = apply_node_test(each, argument, target);
    }
    if(result)
    {
        switch(node_kind(each))
        {
            case MAPPING:
                evaluator_trace("recursive step: processing %zu mapping values (%p)",
                                node_size(each), each);
                result = mapping_iterate(mapping(each), recursive_test_map_iterator,

&(meta_context){context, target});
                break;
            case SEQUENCE:
                evaluator_trace("recursive step: processing %zu sequence items (%p)",
                                node_size(each), each);
                result = sequence_iterate(sequence(each), recursive_test_sequence_iterator,
                                          &(meta_context){context, target});
                break;
            case SCALAR:
                evaluator_trace("recursive step: found scalar, recursion finished on this path (%p)", each);
                result = true;
                break;
            case DOCUMENT:
                evaluator_error("recursive step: uh-oh! found a document node somehow (%p), aborting...", each);
                context->code = ERR_UNEXPECTED_DOCUMENT_NODE;
                result = false;
                break;
            case ALIAS:
                evaluator_trace("recursive step: resolving alias (%p)", each);
                result = apply_recursive_node_test(alias_target(alias(each)), argument, target);
                break;
        }
    }

    return result;
}

static bool recursive_test_sequence_iterator(Node *each, void *context)
{
    meta_context *iterator_context = (meta_context *)context;
    return apply_recursive_node_test(each, iterator_context->context, iterator_context->target);
}

static bool recursive_test_map_iterator(Node *key, Node *value, void *context)
{
    meta_context *iterator_context = (meta_context *)context;
    return apply_recursive_node_test(value, iterator_context->context, iterator_context->target);
}

static bool apply_node_test(Node *each, void *argument, nodelist *target)
{
    bool result = false;
    evaluator_context *context = (evaluator_context *)argument;
    switch(current_step(context)->test.kind)
    {
        case WILDCARD_TEST:
            if(RECURSIVE == current_step(context)->kind)
            {
                result = apply_recursive_wildcard_test(each, argument, target);
            }
            else
            {
                result = apply_greedy_wildcard_test(each, argument, target);
            }
            break;
        case TYPE_TEST:
            result = apply_type_test(each, argument, target);
            break;
        case NAME_TEST:
            result = apply_name_test(each, argument, target);
            break;
    }
    return result;
}

static bool apply_greedy_wildcard_test(const Node *each, void *argument, nodelist *target)
{
    evaluator_context *context = (evaluator_context *)argument;
    bool result = false;
    switch(node_kind(each))
    {
        case MAPPING:
            evaluator_trace("wildcard test: adding %zu mapping values (%p)",
                            node_size(each), each);
            result = guard(mapping_iterate(mapping((Node *)each), add_values_to_nodelist_map_iterator, &(meta_context){context, target}));
            break;
        case SEQUENCE:
            evaluator_trace("wildcard test: adding %zu sequence items (%p)",
                            node_size(each), each);
            result = guard(sequence_iterate(sequence((Node *)each), add_to_nodelist_sequence_iterator, target));
            break;
        case SCALAR:
            trace_string("wildcard test: adding scalar: '%s' (%p)",
                         scalar_value(scalar((Node *)each)), node_size(each), each);
            result = guard(nodelist_add(target, each));
            break;
        case DOCUMENT:
            evaluator_error("wildcard test: uh-oh! found a document node somehow (%p), aborting...", each);
            context->code = ERR_UNEXPECTED_DOCUMENT_NODE;
            break;
        case ALIAS:
            evaluator_trace("wildcard test: resolving alias (%p)", each);
            result = apply_greedy_wildcard_test(alias_target(alias((Node *)each)), argument, target);
            break;
    }
    return result;
}

static bool apply_recursive_wildcard_test(const Node *each, void *argument, nodelist *target)
{
    evaluator_context *context = (evaluator_context *)argument;
    bool result = false;
    switch(node_kind(each))
    {
        case MAPPING:
            evaluator_trace("recurisve wildcard test: adding mapping node (%p)", each);
            result = guard(nodelist_add(target, each));
            break;
        case SEQUENCE:
            evaluator_trace("recurisve wildcard test: adding sequence node (%p)", each);
            result = guard(nodelist_add(target, each));
            break;
        case SCALAR:
            trace_string("recurisve wildcard test: adding scalar: '%s' (%p)",
                         scalar_value(scalar((Node *)each)), node_size(each), each);
            result = guard(nodelist_add(target, each));
            break;
        case DOCUMENT:
            evaluator_error("recurisve wildcard test: uh oh! found a document node somehow (%p), aborting...", each);
            context->code = ERR_UNEXPECTED_DOCUMENT_NODE;
            break;
        case ALIAS:
            evaluator_trace("recurisve wildcard test: resolving alias (%p)", each);
            result = apply_recursive_wildcard_test(alias_target(alias((Node *)each)), argument, target);
            break;
    }
    return result;
}

static bool apply_type_test(const Node *each, void *argument, nodelist *target)
{
    bool match = false;
    if(is_alias(each))
    {
        evaluator_trace("type test: resolved alias from: (%p) to: (%p)",
                        each, alias_target((alias((Node *)each))));
        return apply_type_test(alias_target(alias((Node *)each)), argument, target);
    }
    evaluator_context *context = (evaluator_context *)argument;
    switch(current_step(context)->test.type)
    {
        case OBJECT_TEST:
            evaluator_trace("type test: testing for an object");
            match = is_mapping(each);
            break;
        case ARRAY_TEST:
            evaluator_trace("type test: testing for an array");
            match = is_sequence(each);
            break;
        case STRING_TEST:
            evaluator_trace("type test: testing for a string");
            match = is_string((Node *)each);
            break;
        case NUMBER_TEST:
            evaluator_trace("type test: testing for a number");
            match = is_number((Node *)each);
            break;
        case BOOLEAN_TEST:
            evaluator_trace("type test: testing for a boolean");
            match = is_boolean((Node *)each);
            break;
        case NULL_TEST:
            evaluator_trace("type test: testing for a null");
            match = is_null((Node *)each);
            break;
    }
    if(match)
    {
        evaluator_trace("type test: match! adding node (%p)", each);
        return nodelist_add(target, each) ? true : (context->code = ERR_EVALUATOR_OUT_OF_MEMORY, false);
    }
    else
    {
        const char *name = is_scalar(each) ? scalar_kind_name(scalar((Node *)each)) : node_kind_name(each);
        evaluator_trace("type test: no match (actual: %d). dropping (%p)", name, each);
        return true;
    }
}

static bool apply_name_test(const Node *each, void *argument, nodelist *target)
{
    evaluator_context *context = (evaluator_context *)argument;
    Step *context_step = current_step(context);
    trace_string("name test: using key '%s'", name_test_step_name(context_step), name_test_step_length(context_step));

    if(!is_mapping(each))
    {
        evaluator_trace("name test: node is not a mapping type, cannot use a key on it (kind: %d), dropping (%p)", node_kind(each), each);
        return true;
    }
    Mapping *map = mapping((Node *)each);
    Node *value = mapping_get(map, name_test_step_name(context_step), name_test_step_length(context_step));
    if(NULL == value)
    {
        evaluator_trace("name test: key not found in mapping, dropping (%p)", each);
        return true;
    }
    evaluator_trace("name test: match! adding node (%p)", value);
    if(is_alias(value))
    {
        evaluator_trace("name test: resolved alias from: (%p) to: (%p)",
                        value, alias_target(alias((Node *)value)));
        value = alias_target(alias((Node *)value));
    }
    return nodelist_add(target, value) ? true : (context->code = ERR_EVALUATOR_OUT_OF_MEMORY, false);
}

static bool apply_predicate(Node *each, void *argument, nodelist *target)
{
    evaluator_context *context = (evaluator_context *)argument;
    bool result = false;
    switch(current_step(context)->predicate->kind)
    {
        case WILDCARD:
            evaluator_trace("evaluating wildcard predicate");
            result = apply_wildcard_predicate(each, context, target);
            break;
        case SUBSCRIPT:
            evaluator_trace("evaluating subscript predicate");
            if(!is_sequence(each))
            {
                evaluator_trace("subscript predicate: node is not a sequence type, cannot use an index on it (kind: %d), dropping (%p)", node_kind(each), each);
                result = true;
            }
            else
            {
                result = apply_subscript_predicate(sequence(each), context, target);
            }
            break;
        case SLICE:
            evaluator_trace("evaluating slice predicate");
            if(!is_sequence(each))
            {
                evaluator_trace("slice predicate: node is not a sequence type, cannot use a slice on it (kind: %d), dropping (%p)", node_kind(each), each);
                result = true;
            }
            else
            {
                result = apply_slice_predicate(sequence(each), context, target);
            }
            break;
        case JOIN:
            evaluator_trace("evaluating join predicate");
            result = apply_join_predicate(each, context, target);
            break;
    }
    return result;
}

static bool apply_wildcard_predicate(const Node *value, evaluator_context *context, nodelist *target)
{
    bool result = false;
    switch(node_kind(value))
    {
        case SCALAR:
            trace_string("wildcard predicate: adding scalar '%s' (%p)",
                         scalar_value(scalar((Node *)value)), node_size(value), value);
            result = guard(nodelist_add(target, value));
            break;
        case MAPPING:
            evaluator_trace("wildcard predicate: adding mapping (%p)", value);
            result = guard(nodelist_add(target, value));
            break;
        case SEQUENCE:
            evaluator_trace("wildcard predicate: adding %zu sequence (%p) items", node_size(value), value);
            result = guard(sequence_iterate(sequence((Node *)value), add_to_nodelist_sequence_iterator, target));
            break;
        case DOCUMENT:
            evaluator_error("wildcard predicate: uh-oh! found a document node (%p), aborting...", value);
            context->code = ERR_UNEXPECTED_DOCUMENT_NODE;
            break;
        case ALIAS:
            evaluator_trace("wildcard predicate: resolving alias (%p)", value);
            result = apply_wildcard_predicate(alias_target(alias((Node *)value)), context, target);
            break;
    }
    return result;
}

static bool apply_subscript_predicate(const Sequence *value, evaluator_context *context, nodelist *target)
{
    Predicate *subscript = current_step(context)->predicate;
    int64_t index = subscript_predicate_index(subscript);
    uint64_t abs = (uint64_t)index;
    if(abs > node_size(value))
    {
        evaluator_trace("subscript predicate: index %zu not valid for sequence (length: %zd), dropping (%p)", index, node_size(value), value);
        return true;
    }
    Node *selected = sequence_get(value, index);
    evaluator_trace("subscript predicate: adding index %zu (%p) from sequence (%p) of %zd items", index, selected, value, node_size(value));
    return nodelist_add(target, selected) ? true : (context->code = ERR_EVALUATOR_OUT_OF_MEMORY, false);
}

static bool apply_slice_predicate(const Sequence *value, evaluator_context *context, nodelist *target)
{
    Predicate *slice = current_step(context)->predicate;
    int64_t from = 0, to = 0, increment = 0;
    normalize_interval(value, slice, &from, &to, &increment);
    evaluator_trace("slice predicate: using normalized interval [%d:%d:%d]", from, to, increment);

    for(int64_t i = from; 0 > increment ? i >= to : i < to; i += increment)
    {
        Node *selected = sequence_get(value, i);
        if(NULL == selected || !nodelist_add(target, selected))
        {
            evaluator_error("slice predicate: uh oh! out of memory, aborting. index: %d, selected: %p",
                            i, selected);
            context->code = ERR_EVALUATOR_OUT_OF_MEMORY;
            return false;
        }
        evaluator_trace("slice predicate: adding index: %d (%p)", i, selected);
    }
    return true;
}

static bool apply_join_predicate(Node *value, evaluator_context *context, nodelist *target)
{
    evaluator_trace("join predicate: evaluating axes (_, _)");

    // xxx - implement me!
    evaluator_error("join predicate: uh-oh! not implemented yet, aborting...");
    context->code = ERR_UNSUPPORTED_PATH;
    return false;
}

/*
 * Utility Functions
 * =================
 */

static bool add_to_nodelist_sequence_iterator(Node *each, void *context)
{
    nodelist *list = (nodelist *)context;
    Node *value = each;
    if(is_alias(each))
    {
        value = alias_target(alias(each));
    }
    return nodelist_add(list, value);
}

static bool add_values_to_nodelist_map_iterator(Node *key, node *value, void *context)
{
    meta_context *iterator_context = (meta_context *)context;
    bool result = false;
    switch(node_kind(value))
    {
        case SCALAR:
            trace_string("wildcard test: adding scalar mapping value: '%s' (%p)",
                         scalar_value(scalar(value)), node_size(value), value);
            result = nodelist_add(iterator_context->target, value);
            break;
        case MAPPING:
            evaluator_trace("wildcard test: adding mapping mapping value (%p)", value);
            result = nodelist_add(iterator_context->target, value);
            break;
        case SEQUENCE:
            evaluator_trace("wildcard test: adding %zu sequence mapping values (%p) items", node_size(value), value);
            result = sequence_iterate(sequence(value), add_to_nodelist_sequence_iterator, iterator_context->target);
            break;
        case DOCUMENT:
            evaluator_error("wildcard test: uh-oh! found a document node (%p), aborting...", value);
            iterator_context->context->code = ERR_UNEXPECTED_DOCUMENT_NODE;
            result = false;
            break;
        case ALIAS:
            evaluator_trace("wildcard test: resolving alias (%p)", value);
            result = add_values_to_nodelist_map_iterator(key, alias_target(alias(value)), context);
            break;
    }
    return result;
}

static int normalize_extent(bool specified_p, int64_t given, int64_t fallback, int64_t limit)
{
    if(!specified_p)
    {
        evaluator_trace("slice predicate: (normalizer) no value specified, defaulting to %zd", fallback);
        return fallback;
    }
    int64_t result = 0 > given ? given + limit: given;
    if(0 > result)
    {
        evaluator_trace("slice predicate: (normalizer) negative value, clamping to zero");
        return 0;
    }
    if(limit < result)
    {
        evaluator_trace("slice predicate: (normalizer) value over limit, clamping to %zd", limit);
        return limit;
    }
    evaluator_trace("slice predicate: (normalizer) constrained to %zd", result);
    return result;
}

static int64_t normalize_from(const predicate *slice, const Sequence *value)
{
    evaluator_trace("slice predicate: normalizing from, specified: %s, value: %d", slice_predicate_has_from(slice) ? "yes" : "no", slice_predicate_from(slice));
    int64_t length = (int64_t)node_size(value);
    return normalize_extent(slice_predicate_has_from(slice), slice_predicate_from(slice), 0, length);
}

static int64_t normalize_to(const predicate *slice, const Sequence *value)
{
    evaluator_trace("slice predicate: normalizing to, specified: %s, value: %d", slice_predicate_has_to(slice) ? "yes" : "no", slice_predicate_to(slice));
    int64_t length = (int64_t)node_size(value);
    return normalize_extent(slice_predicate_has_to(slice), slice_predicate_to(slice), length, length);
}

#ifdef USE_LOGGING
static inline void trace_interval(const Sequence *value, predicate *slice)
{
    static const char * fmt = "slice predicate: evaluating interval [%s:%s:%s] on sequence (%p) of %zd items";
    static const char * extent_fmt = "%" PRIdFAST32;
    size_t len = (unsigned)lrint(floor(log10((float)ULLONG_MAX))) + 1;
    char from_repr[len + 1];
    if(slice_predicate_has_from(slice))
    {
        snprintf(from_repr, len, extent_fmt, slice_predicate_from(slice));
    }
    else
    {
        from_repr[0] = '_';
        from_repr[1] = '\0';
    }
    char to_repr[len + 1];
    if(slice_predicate_has_to(slice))
    {
        snprintf(to_repr, len, extent_fmt, slice_predicate_to(slice));
    }
    else
    {
        to_repr[0] = '_';
        to_repr[1] = '\0';
    }
    char step_repr[len + 1];
    if(slice_predicate_has_step(slice))
    {
        snprintf(step_repr, len, extent_fmt, slice_predicate_step(slice));
    }
    else
    {
        step_repr[0] = '_';
        step_repr[1] = '\0';
    }
    evaluator_trace(fmt, from_repr, to_repr, step_repr, value, node_size(value));
}
#else
#define trace_interval(...)
#endif

static void normalize_interval(const Sequence *value, predicate *slice, int64_t *from_val, int64_t *to_val, int64_t *step_val)
{
    trace_interval(value, slice);
    *step_val = slice_predicate_has_step(slice) ? slice_predicate_step(slice) : 1;
    *from_val = 0 > *step_val ? normalize_to(slice, value) - 1 : normalize_from(slice, value);
    *to_val   = 0 > *step_val ? normalize_from(slice, value) : normalize_to(slice, value);
}
