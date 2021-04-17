#if SIZEOF_INT*2 <= SIZEOF_LONG_LONG
#  define BDIGIT unsigned int
#  define SIZEOF_BDIGIT SIZEOF_INT
#elif SIZEOF_INT*2 <= SIZEOF_LONG
#  define BDIGIT unsigned int
#  define SIZEOF_BDIGIT SIZEOF_INT
#elif SIZEOF_SHORT*2 <= SIZEOF_LONG
#  define BDIGIT unsigned short
#  define SIZEOF_BDIGIT SIZEOF_SHORT
#else
#  define BDIGIT unsigned short
#  define SIZEOF_BDIGIT (SIZEOF_LONG/2)
#  define SIZEOF_ACTUAL_BDIGIT SIZEOF_LONG
#endif
#ifndef SIZEOF_ACTUAL_BDIGIT
# define SIZEOF_ACTUAL_BDIGIT SIZEOF_BDIGIT
#endif

#define RBIGNUM(obj) ((struct RBignum *)(obj))
#define BIGNUM_SIGN_BIT FL_USER1
#define BIGNUM_EMBED_FLAG ((VALUE)FL_USER2)
#define BIGNUM_EMBED_LEN_NUMBITS 3
#define BIGNUM_EMBED_LEN_MASK \
    (~(~(VALUE)0U << BIGNUM_EMBED_LEN_NUMBITS) << BIGNUM_EMBED_LEN_SHIFT)
#define BIGNUM_EMBED_LEN_SHIFT \
    (FL_USHIFT+3) /* bit offset of BIGNUM_EMBED_LEN_MASK */
#ifndef BIGNUM_EMBED_LEN_MAX
# if (SIZEOF_VALUE*RVALUE_EMBED_LEN_MAX/SIZEOF_ACTUAL_BDIGIT) < (1 << BIGNUM_EMBED_LEN_NUMBITS)-1
#  define BIGNUM_EMBED_LEN_MAX (SIZEOF_VALUE*RVALUE_EMBED_LEN_MAX/SIZEOF_ACTUAL_BDIGIT)
# else
#  define BIGNUM_EMBED_LEN_MAX ((1 << BIGNUM_EMBED_LEN_NUMBITS)-1)
# endif
#endif

struct RBignum {
    struct RBasic basic;
    union {
        struct {
            size_t len;
            BDIGIT *digits;
        } heap;
        BDIGIT ary[BIGNUM_EMBED_LEN_MAX];
    } as;
};

static inline size_t BIGNUM_LEN(VALUE b);
static inline BDIGIT *BIGNUM_DIGITS(VALUE b);
static inline bool BIGNUM_EMBED_P(VALUE b);

static inline size_t
BIGNUM_LEN(VALUE b)
{
    if (! BIGNUM_EMBED_P(b)) {
        return RBIGNUM(b)->as.heap.len;
    }
    else {
        size_t ret = RBASIC(b)->flags;
        ret &= BIGNUM_EMBED_LEN_MASK;
        ret >>= BIGNUM_EMBED_LEN_SHIFT;
        return ret;
    }
}

/* LSB:BIGNUM_DIGITS(b)[0], MSB:BIGNUM_DIGITS(b)[BIGNUM_LEN(b)-1] */
static inline BDIGIT *
BIGNUM_DIGITS(VALUE b)
{
    if (BIGNUM_EMBED_P(b)) {
        return RBIGNUM(b)->as.ary;
    }
    else {
        return RBIGNUM(b)->as.heap.digits;
    }
}

static inline bool
BIGNUM_EMBED_P(VALUE b)
{
    return FL_TEST_RAW(b, BIGNUM_EMBED_FLAG);
}

#define RBIGNUM_DIGITS BIGNUM_DIGITS
#define RBIGNUM_LEN BIGNUM_LEN
