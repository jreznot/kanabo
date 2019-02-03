#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "str.h"


struct string_s
{
    size_t  length;
    uint8_t value[];
};

struct mutable_string_s
{
    size_t  capacity;
    String  base;
};


static inline String *string_alloc(size_t length)
{
    return calloc(1, sizeof(String) + length + 1);
}

static inline String *string_init(String *self, const char *value, size_t length)
{
    self->length = length;
    memcpy(self->value, value, length);
    self->value[self->length] = '\0';

    return self;
}

static inline String *string_init_with_bytestring(String *self, const uint8_t *value, size_t length)
{
    self->length = length;
    memcpy(self->value, value, length);
    self->value[self->length] = '\0';

    return self;
}

String *make_string(const char *value)
{
    size_t length = strlen(value);
    String *self = string_alloc(length);
    if(NULL == self)
    {
        return NULL;
    }

    return string_init(self, value, length);
}

String *make_string_with_bytestring(const uint8_t *value, size_t length)
{
    String *self = string_alloc(length);
    if(NULL == self)
    {
        return NULL;
    }

    return string_init_with_bytestring(self, value, length);
}

String *format(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    String *result = vformat(format, args);

    va_end(args);

    return result;
}

String *vformat(const char *format, va_list format_args)
{
    va_list count_args;
    va_copy(count_args, format_args);

    errno = 0;
    int count = vsnprintf(NULL, 0, format, count_args);
    va_end(count_args);

    if(0 > count)
    {
        return NULL;
    }

    size_t length = (size_t)count;
    String *self = string_alloc(length);
    if(NULL == self)
    {
        return NULL;
    }

    self->length = length;

    errno = 0;
    count = vsnprintf((char *)self->value, length + 1, format, format_args);  // N.B. - sizeof(self->value) == length + 1
    if(0 > count)
    {
        free(self);
        return NULL;
    }

    return self;
}

void dispose_string(String *self)
{
    if(NULL == self)
    {
        return;
    }

    free(self);
}

String *string_clone(const String *self)
{
    String *that = string_alloc(self->length);
    if(NULL == that)
    {
        return NULL;
    }

    return string_init(that, (const char *)self->value, self->length);
}

size_t string_length(const String *self)
{
    return self->length;
}

const uint8_t * string_data(const String *self)
{
    return self->value;
}

uint8_t string_get(const String *self, size_t index)
{
    return self->value[index];
}

bool string_equals(const String *self, const String *other)
{
    if(self->length != other->length)
    {
        return false;
    }

    return 0 == memcmp(self->value, other->value, self->length);
}

bool string_equals_c_string(const String *self, const char *other)
{
    if(self->length != strlen(other))
    {
        return false;
    }

    return 0 == memcmp(self->value, other, self->length);
}

bool string_equals_bytestring(const String *self, const uint8_t *other, size_t length)
{
    if(self->length != length)
    {
        return false;
    }

    return 0 == memcmp(self->value, other, self->length);
}

bool string_iterate(const String *self, string_iterator iterator, void *parameter)
{
    for(size_t i = 0; i < string_length(self); i++)
    {
        if(!iterator(i, self->value[i], parameter))
        {
            return false;
        }
    }

    return true;
}

bool string_startswith(const String *self, const String *value)
{
    if(value->length > self->length)
    {
        return false;
    }

    return 0 == memcmp(self->value, value->value, value->length);
}

bool string_startswith_c_string(const String *self, const char *value)
{
    size_t length = strlen(value);
    if(length > self->length)
    {
        return false;
    }

    return 0 == memcmp(self->value, value, length);
}

bool string_endswith(const String *self, const String *value)
{
    if(value->length > self->length)
    {
        return false;
    }

    size_t offset = self->length - value->length;

    return 0 == memcmp(self->value + offset, value->value, value->length);
}

bool string_endswith_c_string(const String *self, const char *value)
{
    size_t length = strlen(value);
    if(length > self->length)
    {
        return false;
    }

    size_t offset = self->length - length;

    return 0 == memcmp(self->value + offset, value, length);
}

bool string_contains(const String *self, uint8_t value)
{
    for(size_t i = 0; i < self->length; i++)
    {
        if(value == self->value[i])
        {
            return true;
        }
    }

    return false;
}

const char *string_as_c_str(const String *self)
{
    return (const char *)self->value;
}

static inline size_t calculate_allocation_size(size_t capacity)
{
    return sizeof(MutableString) + capacity + 1;
}

static inline MutableString *mstring_alloc(const size_t capacity)
{
    return calloc(1, calculate_allocation_size(capacity));
}

static inline bool mstring_realloc(MutableString **self, size_t capacity)
{
    MutableString *cache = *self;
    *self = realloc(*self, calculate_allocation_size(capacity));
    if(NULL == *self)
    {
        *self = cache;
        return false;
    }

    (*self)->capacity = capacity;

    return true;
}

static inline MutableString *mstring_init(MutableString *self, size_t capacity)
{
    self->base.length = 0;
    self->capacity = capacity;

    return self;
}

MutableString *make_mstring(size_t capacity)
{
    MutableString *self = mstring_alloc(capacity);
    if(NULL == self)
    {
        return NULL;
    }

    return mstring_init(self, capacity);
}

MutableString *make_mstring_with_char(const uint8_t value)
{
    MutableString *self = make_mstring(1);
    if(NULL == self)
    {
        return NULL;
    }

    mstring_append(&self, value);

    return self;
}

MutableString *make_mstring_with_c_str(const char *value)
{
    size_t length = strlen(value);
    MutableString *self = make_mstring(length);
    if(NULL == self)
    {
        return NULL;
    }

    mstring_append(&self, value);

    return self;
}

MutableString *make_mstring_with_string(const String *value)
{
    MutableString *self = make_mstring(value->length);
    if(NULL == self)
    {
        return NULL;
    }

    mstring_append(&self, value);

    return self;
}

void dispose_mstring(MutableString *self)
{
    if(NULL == self)
    {
        return;
    }

    free(self);
}

size_t mstring_length(const MutableString *self)
{
    return self->base.length;
}

const uint8_t * mstring_data(const MutableString *self)
{
    return self->base.value;
}

uint8_t mstring_get(const MutableString *self, size_t index)
{
    return self->base.value[index];
}

bool mstring_equals(const MutableString *self, const MutableString *other)
{
    if(self->base.length != other->base.length)
    {
        return false;
    }

    return 0 == memcmp(self->base.value, other->base.value, self->base.length);
}

bool mstring_equals_string(const MutableString *self, const String *other)
{
    if(self->base.length != other->length)
    {
        return false;
    }

    return 0 == memcmp(self->base.value, other->value, self->base.length);
}

bool mstring_equals_c_string(const MutableString *self, const char *other)
{
    if(self->base.length != strlen(other))
    {
        return false;
    }

    return 0 == memcmp(self->base.value, other, self->base.length);
}

bool mstring_equals_bytestring(const MutableString *self, const uint8_t *other, size_t length)
{
    if(self->base.length != length)
    {
        return false;
    }

    return 0 == memcmp(self->base.value, other, self->base.length);
}

bool mstring_iterate(const MutableString *self, string_iterator iterator, void *parameter)
{
    return string_iterate(&self->base, iterator, parameter);
}

bool mstring_startswith(const MutableString *self, const String *value)
{
    return string_startswith(&self->base, value);
}

bool mstring_startswith_c_string(const MutableString *self, const char *value)
{
    return string_startswith_c_string(&self->base, value);
}

bool mstring_endswith(const MutableString *self, const String *value)
{
    return string_endswith(&self->base, value);
}

bool mstring_endswith_c_string(const MutableString *self, const char *value)
{
    return string_endswith_c_string(&self->base, value);
}

bool mstring_contains(const MutableString *self, uint8_t value)
{
    return string_contains(&self->base, value);
}

size_t mstring_get_capacity(const MutableString *self)
{
    return self->capacity;
}

bool mstring_has_capacity(const MutableString *self, size_t length)
{
    return (self->capacity - self->base.length) >= length;
}

MutableString *mstring_clone(const MutableString *self)
{
    MutableString *that = mstring_alloc(self->capacity);
    if(NULL == that)
    {
        return NULL;
    }
    mstring_init(that, self->capacity);
    if(self->base.length)
    {
        memcpy(that->base.value, self->base.value, self->capacity + 1);
    }

    return that;
}

String *mstring_as_string(const MutableString *self)
{
    String *that = string_alloc(self->base.length);
    if(NULL == self)
    {
        return NULL;
    }

    return string_init(that, (const char *)self->base.value, self->base.length);
}

String *mstring_as_string_no_copy(MutableString *self)
{
    return &self->base;
}

const char *mstring_as_c_str(const MutableString *self)
{
    return (const char *)self->base.value;
}

char *mstring_copy(const MutableString *self)
{
    char *result = calloc(1, self->base.length + 1);
    if(NULL == result)
    {
        return NULL;
    }

    memcpy(result, self->base.value, self->base.length);
    result[self->base.length] = '\0';

    return result;
}

static inline size_t calculate_new_capacity(size_t capacity)
{
    return (capacity * 3) / 2 + 1;
}

static inline bool ensure_capacity(MutableString **self, size_t length)
{
    if(!mstring_has_capacity(*self, length))
    {
        size_t min_capacity = (*self)->base.length + length;
        size_t new_capacity = calculate_new_capacity(min_capacity);
        return mstring_realloc(self, new_capacity);
    }

    return true;
}

static inline void append(MutableString *self, const void *data, size_t length)
{
    memcpy(self->base.value + self->base.length, data, length);
    self->base.length += length;
    self->base.value[self->base.length] = '\0';
}

bool mstring_append_byte(MutableString **self, const uint8_t value)
{
    if(!ensure_capacity(self, 1))
    {
        return false;
    }

    append(*self, &value, 1);

    return true;
}

bool mstring_append_char(MutableString **self, const char value)
{
    if(!ensure_capacity(self, 1))
    {
        return false;
    }

    append(*self, &value, 1);

    return true;
}

bool mstring_append_c_str(MutableString **self, const char *value)
{
    size_t length = strlen(value);
    if(!ensure_capacity(self, length))
    {
        return false;
    }

    append(*self, value, length);

    return true;
}

bool mstring_append_string(MutableString **self, const String *string)
{
    if(!ensure_capacity(self, string->length))
    {
        return false;
    }

    append(*self, string->value, string->length);

    return true;
}

bool mstring_append_mstring(MutableString **self, const MutableString *string)
{
    if(!ensure_capacity(self, string->base.length))
    {
        return false;
    }

    append(*self, string->base.value, string->base.length);

    return true;
}

bool mstring_append_stream(MutableString **self, const uint8_t *value, size_t length)
{
    if(!ensure_capacity(self, length))
    {
        return false;
    }

    append(*self, value, length);

    return true;
}

bool mformat(MutableString **self, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    bool result = mvformat(self, format, args);

    va_end(args);

    return result;
}

bool mvformat(MutableString **self, const char *format, va_list format_args)
{
    va_list count_args;
    va_copy(count_args, format_args);

    errno = 0;
    int count = vsnprintf(NULL, 0, format, count_args);
    va_end(count_args);

    if(0 > count)
    {
        return false;
    }

    size_t length = (size_t)count;
    if(!ensure_capacity(self, length))
    {
        return false;
    }

    char *end = ((char *)(*self)->base.value) + (*self)->base.length;
    errno = 0;
    count = vsnprintf(end, length + 1, format, format_args);  // N.B. - sizeof(self->value) == length + 1
    if(0 > count)
    {
        return false;
    }

    (*self)->base.length += length;

    return true;
}

void mstring_set(MutableString *self, size_t position, uint8_t value)
{
    if(position > self->base.length)
    {
        return;
    }
    self->base.value[position] = value;
}

void mstring_set_range(MutableString *self, size_t position, size_t length, const uint8_t *value)
{
    if(position > self->base.length || position + length > self->base.length)
    {
        return;
    }
    memcpy(self->base.value, value, length);
}