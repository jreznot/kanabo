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


#include "jsonpath/combinators/wrapped.h"


struct wrapped_parser_s
{
    Parser  base;
    Parser *child;
};

typedef struct wrapped_parser_s WrappedParser;


static void wrapped_free(Parser *value)
{
    WrappedParser *self = (WrappedParser *)value;
    parser_free(self->child);
}

static void wrapped_log(Parser *value)
{
    WrappedParser *self = (WrappedParser *)value;
    parser_debug("processing %s combinator, containing: %s",
                 parser_name(value), parser_name(self->child));
}

static const struct vtable_s WRAPPED_VTABLE =
{
    wrapped_free,
    wrapped_log
};

Parser *make_wrapped_parser(enum parser_kind kind, parser_function func, Parser *child)
{
    WrappedParser *result = (WrappedParser *)calloc(1, sizeof(WrappedParser));
    if(NULL == result)
    {
        return NULL;
    }
    
    parser_init((Parser *)result, kind, func, &WRAPPED_VTABLE);
    result->child = child;

    return (Parser *)result;
}
