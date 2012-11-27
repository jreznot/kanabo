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

#include <stdio.h>

#include <check.h>

#include "jsonpath.h"
#include "test.h"

START_TEST (null_expression)
{
    jsonpath path;
    parser_result *result = parse_jsonpath(NULL, 50, &path);
    
    ck_assert_not_null(result);
    ck_assert_int_eq(ERR_NULL_EXPRESSION, result->code);
    ck_assert_not_null(result->message);

    fprintf(stdout, "received expected failure message: '%s'\n", result->message);

    ck_assert_int_eq(0, result->position);

    free_parser_result(result);
}
END_TEST

START_TEST (zero_length)
{
    jsonpath path;
    parser_result *result = parse_jsonpath((uint8_t *)"", 0, &path);
    
    ck_assert_not_null(result);
    ck_assert_int_eq(ERR_ZERO_LENGTH, result->code);
    ck_assert_not_null(result->message);

    fprintf(stdout, "received expected failure message: '%s'\n", result->message);

    ck_assert_int_eq(0, result->position);

    free_parser_result(result);
}
END_TEST

START_TEST (null_path)
{
    parser_result *result = parse_jsonpath((uint8_t *)"", 5, NULL);
    
    ck_assert_not_null(result);
    ck_assert_int_eq(ERR_NULL_OUTPUT_PATH, result->code);
    ck_assert_not_null(result->message);

    fprintf(stdout, "received expected failure message: '%s'\n", result->message);

    ck_assert_int_eq(0, result->position);

    free_parser_result(result);
}
END_TEST

START_TEST (dollar_only)
{
    jsonpath path;
    parser_result *result = parse_jsonpath((uint8_t *)"$", 1, &path);
    
    ck_assert_not_null(result);
    ck_assert_int_eq(SUCCESS, result->code);
    ck_assert_not_null(result->message);
    ck_assert_int_ne(0, result->position);

    free_parser_result(result);
}
END_TEST

Suite *jsonpath_suite(void)
{
    TCase *bad_input = tcase_create("bad input");
    tcase_add_test(bad_input, null_expression);
    tcase_add_test(bad_input, zero_length);
    tcase_add_test(bad_input, null_path);
    
    TCase *basic = tcase_create("basic");
    tcase_add_test(basic, dollar_only);

    Suite *jsonpath = suite_create("JSONPath");
    suite_add_tcase(jsonpath, bad_input);
    suite_add_tcase(jsonpath, basic);
    
    return jsonpath;
}
