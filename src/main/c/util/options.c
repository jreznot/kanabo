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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/param.h>
#include <getopt.h>

#include "options.h"

typedef struct option argument;

static const char * const EMIT_MODES [] =
{
    "bash",
    "zsh",
    "json",
    "yaml"
};

static argument arguments[] =
{
    // meta commands:
    {"version",     no_argument,       NULL, 'v'}, // print version and exit
    {"no-warranty", no_argument,       NULL, 'w'}, // print no-warranty and exit
    {"help",        no_argument,       NULL, 'h'}, // print help and exit
    // operating modes:
    {"query",       required_argument, NULL, 'q'}, // evaluate given expression and exit
    // optional arguments:
    {"output",      required_argument, NULL, 'o'}, // emit expressions for the given shell
    {"duplicate",   required_argument, NULL, 'd'}, // how to respond to duplicate mapping keys
    {0, 0, 0, 0}
};

#define ENSURE_COMMAND_ORTHOGONALITY(test) \
    if((test))                             \
    {                                      \
        command = SHOW_HELP;     \
        done = true;                       \
        break;                             \
    }

inline int32_t parse_emit_mode(const char *value)
{
    if(strncmp("bash", value, 4) == 0)
    {
        return BASH;
    }
    else if(strncmp("zsh", value, 3) == 0)
    {
        return ZSH;
    }
    else if(strncmp("json", value, 4) == 0)
    {
        return JSON;
    }
    else if(strncmp("yaml", value, 4) == 0)
    {
        return YAML;
    }
    else
    {
        return -1;
    }
}

inline const char * emit_mode_name(enum emit_mode value)
{
    return EMIT_MODES[value];
}

enum command process_options(const int argc, char * const *argv, struct options *options)
{
    int opt;
    bool done = false;
    enum command command = INTERACTIVE_MODE;

    options->emit_mode = BASH;
    options->duplicate_strategy = DUPE_CLOBBER;
    options->input_file_name = NULL;
    options->mode = INTERACTIVE_MODE;

    while(!done && (opt = getopt_long(argc, argv, "vwhq:o:d:", arguments, NULL)) != -1)
    {
        switch(opt)
        {
            case 'v':
                ENSURE_COMMAND_ORTHOGONALITY(2 < argc);
                command = SHOW_VERSION;
                done = true;
                break;
            case 'h':
                ENSURE_COMMAND_ORTHOGONALITY(2 < argc);
                command = SHOW_HELP;
                done = true;
                break;
            case 'w':
                ENSURE_COMMAND_ORTHOGONALITY(2 < argc);
                command = SHOW_WARRANTY;
                done = true;
                break;
            case 'q':
                command = EXPRESSION_MODE;
                options->expression = optarg;
                options->mode = EXPRESSION_MODE;
                break;
            case 'o':
            {
                int32_t mode = parse_emit_mode(optarg);
                if(-1 == mode)
                {
                    fprintf(stderr, "error: %s: unsupported output format `%s'\n", argv[0], optarg);
                    command = SHOW_HELP;
                    done = true;
                    break;
                }
                options->emit_mode = (enum emit_mode)mode;
                break;
            }
            case 'd':
            {
                int32_t strategy = parse_duplicate_strategy(optarg);
                if(-1 == strategy)
                {
                    fprintf(stderr, "error: %s: unsupported duplicate strategy `%s'\n", argv[0], optarg);
                    command = SHOW_HELP;
                    done = true;
                    break;
                }
                options->duplicate_strategy = (enum loader_duplicate_key_strategy)strategy;
                break;
            }
            case ':':
            case '?':
            default:
                command = SHOW_HELP;
                done = true;
                break;
        }
    }
    if(optind > argc)
    {
        fputs("uh oh! something went wrong handing arguments!\n", stderr);
        return SHOW_HELP;
    }
    if(argc - optind)
    {
        options->input_file_name = argv[optind];
    }
    if(INTERACTIVE_MODE == options->mode &&
       options->input_file_name &&
       0 == memcmp("-", options->input_file_name, 1))
    {
        fputs("error: the standard in shortcut `-' can't be used with interactive evaluation\n", stderr);
        command = SHOW_HELP;
    }
    return command;
}
