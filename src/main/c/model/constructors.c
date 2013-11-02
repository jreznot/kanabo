/*
 * 金棒 (kanabō)
 * Copyright (c) 2012 Kevin Birch <kmb@pobox.com>.  All rights reserved.
 *
 * 金棒 is a tool to bludgeon YAML and JSON files from the shell: the strong
 * made stronger.
 *
 * For more information, consult the README file in the project root.
 *
 * Distributed under an [MIT-style][license] license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal with
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimers.
 * - Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimers in the documentation and/or
 *   other materials provided with the distribution.
 * - Neither the names of the copyright holders, nor the names of the authors, nor
 *   the names of other contributors may be used to endorse or promote products
 *   derived from this Software without specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.
 *
 * [license]: http://www.opensource.org/licenses/ncsa
 */

#include <string.h>
#include <errno.h>

#include "model.h"
#include "model/private.h"
#include "conditions.h"

static bool model_init(document_model * restrict model, size_t capacity);

static inline node *make_node(enum node_kind kind);

static inline void sequence_free(node *sequence);
static inline void mapping_free(node *mapping);

static hashcode scalar_node_hash(void *key);
static bool freedom_iterator(void *key, void *value, void *context);

node *make_document_node(node *root)
{
    PRECOND_NONNULL_ELSE_NULL(root);

    node *result = make_node(DOCUMENT);
    if(NULL != result)
    {
        result->content.size = 1;
        result->content.document.root = root;
    }

    return result;
}

node *make_sequence_node(size_t capacity)
{
    PRECOND_ELSE_NULL(0 < capacity);

    node *result = make_node(SEQUENCE);

    if(NULL != result)
    {
        result->content.size = 0;
        result->content.sequence.capacity = capacity;
        result->content.sequence.value = (node **)calloc(1, sizeof(node *) * capacity);
        if(NULL == result->content.sequence.value)
        {
            free(result);
            result = NULL;
            return NULL;
        }
    }

    return result;
}    

node *make_mapping_node(size_t capacity)
{
    PRECOND_ELSE_NULL(0 < capacity);

    node *result = make_node(MAPPING);
    if(NULL != result)
    {
        result->content.mapping = make_hashtable_with_function(node_comparitor, scalar_node_hash);
        if(NULL == result->content.mapping)
        {
            free(result);
            result = NULL;
            return NULL;
        }        
        result->content.size = 0;
    }

    return result;
}    

node *make_scalar_node(const uint8_t *value, size_t length, enum scalar_kind kind)
{
    if(NULL == value && 0 != length)
    {
        errno = EINVAL;
        return NULL;
    }

    node *result = make_node(SCALAR);
    if(NULL != result)
    {
        result->content.size = length;
        result->content.scalar.kind = kind;
        if(NULL != value)
        {
            result->content.scalar.value = (uint8_t *)calloc(1, length);
            if(NULL == result->content.scalar.value)
            {
                free(result);
                result = NULL;
                return NULL;
            }
            memcpy(result->content.scalar.value, value, length);
        }
    }

    return result;
}

static inline node *make_node(enum node_kind kind)
{
    node *result = (node *)calloc(1, sizeof(struct node));
    if(NULL != result)
    {
        result->tag.kind = kind;
        result->tag.name = NULL;
        result->anchor = NULL;
    }

    return result;
}

void model_free(document_model *model)
{
    if(NULL == model)
    {
        return;
    }
    for(size_t i = 0; i < model->size; i++)
    {
        node_free(model->documents[i]);
        model->documents[i] = NULL;
    }
    free(model->documents);
    model->size = 0;
    model->documents = NULL;
    
    free(model);
}

void node_free(node *value)
{
    if(NULL == value)
    {
        return;
    }
    switch(node_kind(value))
    {
        case DOCUMENT:
            node_free(value->content.document.root);
            value->content.document.root = NULL;
            break;
        case SCALAR:
            fflush(stdout);
            free(value->content.scalar.value);
            value->content.scalar.value = NULL;
            break;
        case SEQUENCE:
            sequence_free(value);
            break;
        case MAPPING:
            mapping_free(value);
            break;
    }
    free(value->tag.name);
    free(value->anchor);
    free(value);
}

static inline void sequence_free(node *sequence)
{
    if(NULL == sequence_get_all(sequence))
    {
        return;
    }
    for(size_t i = 0; i < node_size(sequence); i++)
    {
        node_free(sequence->content.sequence.value[i]);
        sequence->content.sequence.value[i] = NULL;
    }
    free(sequence->content.sequence.value);
    sequence->content.sequence.value = NULL;
}

static bool freedom_iterator(void *key, void *value, void *context __attribute__((unused)))
{
    node_free((node *)key);
    node_free((node *)value);
    
    return true;
}

static inline void mapping_free(node *mapping)
{
    if(NULL == mapping->content.mapping)
    {
        return;
    }
    
    hashtable_iterate(mapping->content.mapping, freedom_iterator, NULL);
    hashtable_free(mapping->content.mapping);
    mapping->content.mapping = NULL;
}

document_model *make_model(size_t capacity)
{
    document_model *result = (document_model *)calloc(1, sizeof(document_model));
    bool initialized = false;
    if(NULL != result)
    {
        initialized = model_init(result, capacity);
    }

    return NULL != result && initialized ? result : NULL;
}

static bool model_init(document_model * restrict model, size_t capacity)
{
    PRECOND_NONNULL_ELSE_FALSE(model);
    PRECOND_ELSE_FALSE(0 < capacity);

    bool result = true;
    model->size = 0;
    model->documents = (node **)calloc(1, sizeof(node *) * capacity);
    if(NULL == model->documents)
    {
        model->capacity = 0;
        result = false;
    }
    else
    {
        model->capacity = capacity;
    }

    return result;
}

static hashcode scalar_node_hash(void *key)
{
    node *scalar = (node *)key;
    return shift_add_xor_string_buffer_hash(scalar_value(scalar), node_size(scalar));
}

