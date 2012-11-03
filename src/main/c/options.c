/*
 * 金棒 (kanabō)
 * Copyright (c) 2012 Kevin Birch <kmb@pobox.com>
 * 
 * 金棒 is a tool to bludgeon YAML and JSON files from the shell: the strong
 * made stronger.
 *
 * For more information, consult the README in the project root.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/param.h>

#include "options.h"

static inline emit_mode process_emit_mode(const char * restrict argument);
static inline const char *process_program_name(const char *argv0);

static struct option options[] = 
{
    // meta commands:
    {"version",     no_argument,       NULL, 'v'}, // print version and exit
    {"no-warranty", no_argument,       NULL, 'w'}, // print no-warranty and exit
    {"help",        no_argument,       NULL, 'h'}, // print help and exit
    // operating modes:
    {"interactive", no_argument,       NULL, 'I'}, // enter interactive mode (requres -f/--file), specify the shell to emit for
    {"query",       required_argument, NULL, 'Q'}, // evaluate given expression and exit
    // optional arguments:
    {"file",        required_argument, NULL, 'f'}, // read input from file instead of stdin (required for interactive mode)
    {"shell",       required_argument, NULL, 's'}, // emit expressions for the given shell (the default is Bash)
    {0, 0, 0, 0}
};

cmd process_options(const int argc, char * const *argv, struct settings * restrict settings)
{
    int opt;
    int command = -1;
    bool done = false;

    settings->program_name = process_program_name(argv[0]);
    settings->emit_mode = BASH;
    settings->expression = NULL;
    settings->input_file_name = NULL;

    while(!done && (opt = getopt_long(argc, argv, "vwhIQ:s:f:", options, NULL)) != -1)
    {
        switch(opt)
        {
            case 'v':
#define ENSURE_COMMAND_ORTHOGONALITY(test) \
                if((test))                 \
                {                          \
                    command = SHOW_HELP;   \
                    done = true;           \
                    break;                 \
                }
                ENSURE_COMMAND_ORTHOGONALITY(1 < argc);
                command = SHOW_VERSION;
                done = true;
                break;
            case 'h':
                ENSURE_COMMAND_ORTHOGONALITY(1 < argc);
                command = SHOW_HELP;
                done = true;
                break;
            case 'w':
                ENSURE_COMMAND_ORTHOGONALITY(1 < argc);
                command = SHOW_WARRANTY;
                done = true;
                break;
            case 'I':
                ENSURE_COMMAND_ORTHOGONALITY(-1 != command);
                command = ENTER_INTERACTIVE;
                break;
            case 'Q':
                ENSURE_COMMAND_ORTHOGONALITY(-1 != command);
                command = EVAL_PATH;
                settings->expression = optarg;
                break;
            case 's':
                settings->emit_mode = process_emit_mode(optarg);
                if(-1 == settings->emit_mode)
                {
                    fprintf(stderr, "%s: unsupported shell mode `%s'\n", settings->program_name, optarg);
                    command = SHOW_HELP;
                    done = true;
                }
                break;
            case 'f':
                settings->input_file_name = optarg;
                break;
            case ':':
            case '?':
            default:
                command = SHOW_HELP;
                done = true;
                break;
        }
    }

    if(-1 == command)
    {
        fprintf(stderr, "%s: either `--query <expression>' or `--interactive' must be specified.\n", settings->program_name);
        command = SHOW_HELP;
    }
    else if(ENTER_INTERACTIVE == command && NULL == settings->input_file_name)
    {
        command = SHOW_HELP;
    }
    
    return command;
}

static inline emit_mode process_emit_mode(const char * restrict argument)
{
    if(strncmp("bash", argument, 4) == 0)
    {
        return BASH;
    }
    else if(strncmp("zsh", argument, 3) == 0)
    {
        return ZSH;
    }
    else
    {
        return -1;
    }
}

static inline const char *process_program_name(const char *argv0)
{
    char *slash = strrchr(argv0, '/');
    return NULL == slash ? argv0 : slash + 1;
}
