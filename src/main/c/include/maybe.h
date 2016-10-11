#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

enum maybe_tag_e
{
    NOTHING,
    JUST
};

typedef enum maybe_tag_e MaybeTag;
    
#define Maybe(TYPE) maybe_ ## TYPE ## _s

#define make_maybe(TYPE) \
    typedef struct       \
    {                                           \
        MaybeTag tag;                           \
        union                                   \
        {                                       \
            uint_fast16_t  code;                \
            TYPE value;                         \
        };                                      \
    } Maybe(TYPE)


// Mabye Constructors

#define just(a, x) (Maybe(a)){.tag=JUST, .value=(x)}
#define nothing(a) (Maybe(a)){.tag=NOTHING, .code=0}
#define fail(v) (Maybe(a)){.tag=NOTHING, .code=(v)}


// Maybe functions

#define is_just(x) JUST == (x).tag
#define is_nothing(x) NOTHING == (x).tag

#define from_just(x) (x).value
#define from_nothing(x) (x).code
#define unwrap(x, fallback) is_just(x) ? from_just(x) : fallback


// Maybe Monand functions

// the >>= function
typedef uint_fast16_t (*bind_fn)(void *a, void **result);
Maybe bind(Maybe a, bind_fn fn);

#define bind(a, x, f) is_nothing(x) ? x : 
    
// the >> function
typedef uint_fast16_t (*then_fn)(void **result);
Maybe then(Maybe a, then_fn fn);

// todo - liftM2 applys f of contained types and returns maybe
// ap?


// Maybe MonadPlus functions

#define mplus(a, x, y) is_just(x) ? x : is_just(y) ? y : nothing(a)
#define mzero(a) nothing(a)

#ifdef __cplusplus
}
#endif
