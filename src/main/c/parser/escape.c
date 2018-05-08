#include <inttypes.h>
#include <stdio.h>

#include "str.h"

#include "parser/parse.h"

#define SURROGATE_START    0xD800
#define SURROGATE_END      0xDFFF
#define NONCHARACTER_START 0xFFFE
#define NONCHARACTER_END   0xFFFF

static inline int8_t ucs4_to_utf8(uint32_t ucs4, uint8_t utf8[4])
{
    if(ucs4 >= SURROGATE_START && ucs4 <= SURROGATE_END)
    {
        return -1;
    }
    if(ucs4 >= NONCHARACTER_START && ucs4 <= NONCHARACTER_END)
    {
        return -1;
    }

    if(ucs4 < 0x80)
    {
        utf8[0] = (uint8_t)ucs4;
        return 1;
    }
    if(ucs4 < 0x800)
    {
        utf8[0] = (uint8_t)(ucs4 >> 6) | 0xC0;
        utf8[1] = (uint8_t)(ucs4 & 0x3F) | 0x80;
        return 2;
    }
    if(ucs4 < 0x10000)
    {
        utf8[0] = (uint8_t)(ucs4 >> 12) | 0xE0;
        utf8[1] = (uint8_t)((ucs4 >> 6) & 0x3F) | 0x80;
        utf8[2] = (uint8_t)(ucs4 & 0x3F) | 0x80;
        return 3;
    }
    if(ucs4 < 0x110000)
    {
        utf8[0] = (uint8_t)(ucs4 >> 18) | 0xF0;
        utf8[1] = (uint8_t)((ucs4 >> 12) & 0x3F) | 0x80;
        utf8[2] = (uint8_t)((ucs4 >> 6) & 0x3F) | 0x80;
        utf8[3] = (uint8_t)((ucs4 & 0x3F)) | 0x80;
        return 4;
    }

    return -1;
}

static inline bool unescape_unicode(Parser *self, uint32_t ucs4, MutableString **string)
{
    uint8_t utf8[4];

    int8_t length = ucs4_to_utf8(ucs4, utf8);

    if(!(length > 0))
    {
        add_error(self, position(self), UNSUPPORTED_UNICODE_SEQUENCE);
        return false;
    }

    mstring_append_stream(string, utf8, (size_t)length);

    return true;
}

char *unescape(Parser *self, const char *lexeme)
{
    char *result = NULL;

    size_t length = strlen(lexeme);
    MutableString *cooked = make_mstring(length);

    for(size_t i = 0; i < length; i++)
    {
        if(lexeme[i] != '\\')
        {
            mstring_append(&cooked, lexeme[i]);
            continue;
        }
        i++;
        switch(lexeme[i])
        {
            case '"':
            case '\'':
            case '/':
            case ' ':
            case '\\':
                // N.B. - unescape as the literal value
                mstring_append(&cooked, lexeme[i]);
                break;
            case 'b':
                mstring_append(&cooked, (char)'\b');
                break;
            case 'n':
                mstring_append(&cooked, (char)'\n');
                break;
            case 'r':
                mstring_append(&cooked, (char)'\r');
                break;
            case 't':
                mstring_append(&cooked, (char)'\t');
                break;
            case '_':
                mstring_append(&cooked, "\xC2\xA0");
                break;
            case '0':
                mstring_append(&cooked, (char)'\0');
                break;
            case 'a':
                mstring_append(&cooked, (char)'\a');
                break;
            case 'e':
                mstring_append(&cooked, (char)'\x1b');
                break;
            case 'v':
                mstring_append(&cooked, (char)'\v');
                break;
            case 'L':
                mstring_append(&cooked, "\xe2\x80\xa8");
                break;
            case 'N':
                mstring_append(&cooked, "\xc2\x85");
                break;
            case 'P':
                mstring_append(&cooked, "\xe2\x80\xa9");
                break;
            case 'x':
                i++;
                uint8_t c;
                sscanf(lexeme + i, "%2"SCNx8, &c);
                mstring_append(&cooked, c);
                i++;
                break;
            case 'u':
            {
                i++;
                uint32_t ucs4;
                sscanf(lexeme + i, "%4"SCNx32, &ucs4);
                if(!unescape_unicode(self, ucs4, &cooked))
                {
                    goto cleanup;
                }
                i += 3;
                break;
            }
            case 'U':
            {
                i++;
                uint32_t ucs4;
                sscanf(lexeme + i, "%8"SCNx32, &ucs4);
                if(!unescape_unicode(self, ucs4, &cooked))
                {
                    goto cleanup;
                }
                i += 7;
                break;
            }
            default:
                add_error(self, position(self), UNSUPPORTED_ESCAPE_SEQUENCE);
                goto cleanup;
                break;
        }
    }


    result = mstring_copy(cooked);

  cleanup:
    mstring_free(cooked);

    return result;
}