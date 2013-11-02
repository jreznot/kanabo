/*
 * Copyright (c) 2013 Kevin Birch <kmb@pobox.com>.  All rights reserved.
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

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef size_t hashcode;

typedef hashcode (*hash_function)(void *key);
typedef bool (*compare_function)(void *key1, void *key2);

hashcode identity_hash(void *key);
hashcode identity_xor_hash(void *key);

hashcode shift_add_xor_string_hash(void *key);
hashcode shift_add_xor_string_buffer_hash(uint8_t *key, size_t length);

hashcode sdbm_string_hash(void *key);
hashcode sdbm_string_buffer_hash(uint8_t *key, size_t length);

hashcode fnv1_string_hash(void *key);
hashcode fnv1_string_buffer_hash(uint8_t *key, size_t length);

hashcode fnv1a_string_hash(void *key);
hashcode fnv1a_string_buffer_hash(uint8_t *key, size_t length);

hashcode djb_string_hash(void *key);
hashcode djb_string_buffer_hash(uint8_t *key, size_t length);

