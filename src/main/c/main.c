/*
 * 金棒 (kanabō)
 * Copyright (c) 2012 Kevin Birch <kmb@pobox.com>
 * 
 * 金棒 is a tool to bludgeon YAML and JSON files from the shell: the strong
 * made stronger.
 *
 * For more information, consult the README file in the project root.
 *
 * Distributed under an [MIT-style][license] license.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * [license]: http://www.opensource.org/licenses/mit-license.php
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/errno.h>
#include <string.h>

#include "options.h"
#include "loader.h"
#include "shell.h"

static int dispatch(int command, struct settings *settings);
static FILE *open_input(struct settings *settings);
static int interactive_mode(struct settings *settings);
static int expression_mode(struct settings *settings);
static int load_model(struct settings *settings, document_model *model);

int main(const int argc, char * const *argv)
{
    if(NULL == argv || NULL == argv[0])
    {
        fprintf(stderr, "error: whoa! something is wrong, there are no program arguments.\n");
        return -1;
    }

    struct settings settings;
    memset(&settings, 0, sizeof(settings));
    cmd command = process_options(argc, argv, &settings);

    return dispatch(command, &settings);
}

static int dispatch(int command, struct settings *settings)
{
    int result = 0;
    
    switch(command)
    {
        case SHOW_HELP:
            fprintf(stdout, "help\n");
            break;
        case SHOW_VERSION:
            fprintf(stdout, "version information\n");
            break;
        case SHOW_WARRANTY:
            fprintf(stdout, "warranty information\n");
            break;
        case ENTER_INTERACTIVE:
            result = interactive_mode(settings);
            break;
        case EVAL_PATH:
            result = expression_mode(settings);
            break;
        default:
            fprintf(stderr, "panic: unknown command state! this should not happen.\n");
            result = -1;
            break;
    }

    return result;
}

static int interactive_mode(struct settings *settings)
{
    document_model model;

    int result = load_model(settings, &model);
    if(result)
    {
        return result;
    }
    
    fprintf(stdout, "interactive mode\n");
    return 0;
}

static int expression_mode(struct settings *settings)
{
    document_model model;

    int result = load_model(settings, &model);
    if(result)
    {
        return result;
    }
    
    fprintf(stdout, "evaluating expression: \"%s\"\n", settings->expression);
    return 0;
}

static int load_model(struct settings *settings, document_model *model)
{
    FILE *input = open_input(settings);
    if(NULL == input)
    {
        perror(settings->program_name);
        return errno;
    }
    
    int result = load_file(input, model);
    if(result)
    {
        return result;
    }

    return 0;
}

static FILE *open_input(struct settings *settings)
{
    if(NULL == settings->input_file_name)
    {
        return stdin;
    }
    else
    {
        return fopen(settings->input_file_name, "r");
    }
}

