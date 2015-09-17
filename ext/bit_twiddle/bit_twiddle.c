#include <ruby.h>

#define fix_zero LONG2FIX(0L)
#define BIGNUM_P(x) RB_TYPE_P((x), T_BIGNUM)

#if SIZEOF_BDIGIT == SIZEOF_LONG

#define popcount_bdigit __builtin_popcountl
#define ffs_bdigit      __builtin_ffsl
#define clz_bdigit      __builtin_clzl

#elif SIZEOF_BDIGIT == SIZEOF_INT

#define popcount_bdigit __builtin_popcount
#define ffs_bdigit      __builtin_ffs
#define clz_bdigit      __builtin_clz

#else
#error "What is the size of a Ruby Bignum digit on this platform???"
#endif

#if SIZEOF_BDIGIT < 4
#error "Sorry, Bignum#bswap32 and Bignum#sar32 will not work if sizeof(BDIGIT) < 4. Please report this error."
#elif SIZEOF_BDIGIT > 8
#error "Sorry, several methods will not work if sizeof(BDIGIT) > 8. Please report this error."
#elif SIZEOF_LONG > 8
#error "Sorry, Fixnum#sar64 will not work if sizeof(long) > 8. Please report this error."
#endif

static inline VALUE
bigfixize(VALUE x)
{
  size_t n   = RBIGNUM_LEN(x);
  BDIGIT *ds = RBIGNUM_DIGITS(x);
  ulong u;

  while (n && ds[n-1] == 0)
    n--;

  if (n == 0) {
    return INT2FIX(0);
  } else if (n == 1) {
    u = ds[0];
  } else if (SIZEOF_BDIGIT == 4 && SIZEOF_LONG == 8 && n == 2) {
    u = ds[0] + ((ulong)ds[1] << 32);
  } else {
    goto return_big;
  }

  if (RBIGNUM_POSITIVE_P(x)) {
      if (POSFIXABLE(u))
        return LONG2FIX((long)u);
  } else if (u <= -FIXNUM_MIN) {
    return LONG2FIX(-(long)u);
  }

return_big:
  rb_big_resize(x, n);
  return x;
}

static inline int
bnum_greater(VALUE bnum, BDIGIT value)
{
  BDIGIT *digits = RBIGNUM_DIGITS(bnum);
  size_t  len    = RBIGNUM_LEN(bnum);
  if (*digits > value)
    return 1;
  while (--len)
    if (*++digits > 0)
      return 1;
  return 0;
}

static inline long
value_to_shiftdist(VALUE shiftdist, long bits)
{
  for (;;) {
    if (FIXNUM_P(shiftdist)) {
      return FIX2LONG(shiftdist);
    } else if (BIGNUM_P(shiftdist)) {
      long sdist;
      if (bnum_greater(shiftdist, bits-1))
        sdist = bits;
      else
        sdist = *RBIGNUM_DIGITS(shiftdist);
      if (RBIGNUM_NEGATIVE_P(shiftdist))
        sdist = -sdist;
      return sdist;
    } else {
      shiftdist = rb_to_int(shiftdist);
    }
  }
}

/* 'mask' is 0x7 for 8, 0xF for 16, 0x1F for 32, 0x3F for 64
 * return value is always positive! */
static inline ulong
value_to_rotdist(VALUE rotdist, long bits, long mask)
{
  for (;;) {
    long rdist;
    if (FIXNUM_P(rotdist)) {
      rdist = FIX2LONG(rotdist) % bits;
      if (rdist < 0)
        rdist += bits;
      return rdist;
    } else if (BIGNUM_P(rotdist)) {
      rdist = *RBIGNUM_DIGITS(rotdist) & mask;
      if (RBIGNUM_NEGATIVE_P(rotdist))
        rdist = bits - rdist;
      return rdist;
    } else {
      rotdist = rb_to_int(rotdist);
    }
  }
}

static inline void
store_64_into_bnum(VALUE bnum, uint64_t int64)
{
  BDIGIT *dest = RBIGNUM_DIGITS(bnum);
  size_t  len  = RBIGNUM_LEN(bnum);

#if SIZEOF_BDIGIT == 8
  *dest = int64;
#else
  if (len > 1) {
    *dest     = int64;
    *(dest+1) = int64 >> 32;
  } else if (int64 & (0xFFFFFFFFULL << 32) == 0) {
    /* the high 4 bytes are zero anyways */
    *dest = int64;
  } else {
    rb_big_resize(bnum, 2);
    dest      = RBIGNUM_DIGITS(bnum); /* may have moved */
    *dest     = int64;
    *(dest+1) = int64 >> 32;
  }
#endif
}

static inline uint64_t
load_64_from_bignum(VALUE bnum)
{
  BDIGIT *src = RBIGNUM_DIGITS(bnum);
  size_t  len = RBIGNUM_LEN(bnum);
  uint64_t result = *src;

#if SIZEOF_BDIGIT == 4
  if (len > 1)
    result += ((uint64_t)*(src+1)) << 32;
#endif

  return result;
}

static inline VALUE
modify_lo8_in_bignum(VALUE bnum, uint8_t lo8)
{
  VALUE result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = (*RBIGNUM_DIGITS(bnum) & ~0xFFL) | lo8;
  return result;
}

static inline VALUE
modify_lo16_in_bignum(VALUE bnum, uint16_t lo16)
{
  VALUE result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = (*RBIGNUM_DIGITS(bnum) & ~0xFFFFL) | lo16;
  return result;
}

static inline BDIGIT
modify_lo32_in_bdigit(BDIGIT digit, uint32_t lo32)
{
#if SIZEOF_BDIGIT == 4
  return lo32;
#else
  return (digit & ~0xFFFFFFFFL) | lo32;
#endif
}

static inline VALUE
modify_lo32_in_bignum(VALUE bnum, uint32_t lo32)
{
  BDIGIT *digit = RBIGNUM_DIGITS(bnum);
  BDIGIT  value = modify_lo32_in_bdigit(*digit, lo32);
  VALUE   result;

/* if a 'long' is only 4 bytes, a 32-bit number could be promoted to Bignum
 * then modifying the low 32 bits could make it fixable again */
#if SIZEOF_LONG == 4
  /* TODO: need to pay attention to sign!!! */
  if (FIXABLE(value))
    return LONG2FIX(value);
#endif

  result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = value;
  return result;
}

static inline VALUE
modify_lo64_in_bignum(VALUE bnum, uint64_t lo64)
{
  VALUE result;

  if (RBIGNUM_LEN(bnum) <= (8/SIZEOF_BDIGIT)) {
    if (RBIGNUM_POSITIVE_P(bnum)) {
        if (POSFIXABLE(lo64))
          return LONG2FIX((long)lo64);
    } else if (lo64 <= -FIXNUM_MIN) {
      return LONG2FIX(-(long)lo64);
    }
  }

  result = rb_big_clone(bnum);
  store_64_into_bnum(result, lo64);
  return result;
}

static VALUE
fnum_popcount(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  if (value < 0)
    value = -value;
  return LONG2FIX(__builtin_popcountl(value));
}

static VALUE
bnum_popcount(VALUE bnum)
{
  BDIGIT *digits = RBIGNUM_DIGITS(bnum);
  size_t  length = RBIGNUM_LEN(bnum);
  long    bits   = 0;

  while (length--) {
    bits += popcount_bdigit(*digits);
    digits++;
  }

  return LONG2FIX(bits);
}

static VALUE
str_popcount(VALUE str)
{
  char *p     = RSTRING_PTR(str);
  int  length = RSTRING_LEN(str);
  long bits   = 0;

  /* This could be made faster by processing 4/8 bytes at a time */

  while (length--)
    bits += __builtin_popcount(*p++);

  return LONG2FIX(bits);
}

static VALUE
fnum_lo_bit(VALUE fnum)
{
  return LONG2FIX(__builtin_ffsl(FIX2LONG(fnum)));
}

static VALUE
bnum_lo_bit(VALUE bnum)
{
  BDIGIT *digit = RBIGNUM_DIGITS(bnum);
  long    bits  = 0;

  while (!*digit) {
    digit++;
    bits += (sizeof(BDIGIT) * 8);
  }

  bits += ffs_bdigit(*digit);
  return LONG2FIX(bits);
}

static VALUE
fnum_hi_bit(VALUE fnum)
{
  long bits, value;
  if (fnum == fix_zero) return fix_zero;
  value = FIX2LONG(fnum);
  if (value < 0)
    value = -value;
  bits = __builtin_clzl(value);
  return LONG2FIX((sizeof(long) * 8) - bits);
}

static VALUE
bnum_hi_bit(VALUE bnum)
{
  BDIGIT *digit = RBIGNUM_DIGITS(bnum) + (RBIGNUM_LEN(bnum)-1);
  long    bits  = (sizeof(BDIGIT) * 8) * RBIGNUM_LEN(bnum);

  while (!*digit) {
    digit--;
    bits -= (sizeof(BDIGIT) * 8);
  }

  bits -= clz_bdigit(*digit);
  return LONG2FIX(bits);
}

static VALUE
fnum_bswap16(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFF) | __builtin_bswap16(value));
}

static VALUE
bnum_bswap16(VALUE bnum)
{
  VALUE  result = rb_big_clone(bnum);
  BDIGIT value  = *RBIGNUM_DIGITS(bnum);
  *RBIGNUM_DIGITS(result) = (value & ~0xFFFF) | __builtin_bswap16(value);
  return result;
}

static VALUE
fnum_bswap32(VALUE fnum)
{
#if SIZEOF_LONG == 4
  /* the size of a Fixnum is always the same as 'long'
   * and the C standard guarantees 'long' is at least 32 bits
   * but a couple bits are used for tagging, so the usable precision could
   * be less than 32 bits...
   * That is why we have to use a '2NUM' function, not '2FIX' */
  return ULONG2NUM(__builtin_bswap32(FIX2LONG(fnum)));
#elif SIZEOF_LONG >= 8
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFFFFFFL) | __builtin_bswap32(value));
#endif
}

static VALUE
bnum_bswap32(VALUE bnum)
{
  return modify_lo32_in_bignum(bnum, __builtin_bswap32(*RBIGNUM_DIGITS(bnum)));
}

static VALUE
fnum_bswap64(VALUE fnum)
{
  return ULL2NUM(__builtin_bswap64(FIX2LONG(fnum)));
}

static VALUE
bnum_bswap64(VALUE bnum)
{
  uint64_t value = __builtin_bswap64(load_64_from_bignum(bnum));
  return modify_lo64_in_bignum(bnum, value);
}

static VALUE
fnum_rrot8(VALUE fnum, VALUE rotdist)
{
  long    value  = FIX2LONG(fnum);
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  uint8_t lobyte = value;
  lobyte = ((lobyte >> rotd) | (lobyte << (-rotd & 7)));
  return LONG2FIX((value & ~0xFFL) | lobyte);
}

static VALUE
bnum_rrot8(VALUE bnum, VALUE rotdist)
{
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  uint8_t lobyte = *RBIGNUM_DIGITS(bnum);
  lobyte = ((lobyte >> rotd) | (lobyte << (-rotd & 7)));
  return modify_lo8_in_bignum(bnum, lobyte);
}

static VALUE
fnum_rrot16(VALUE fnum, VALUE rotdist)
{
  long     value  = FIX2LONG(fnum);
  ulong    rotd   = value_to_rotdist(rotdist, 16, 0xF);
  uint16_t loword = value;
  loword = (loword >> rotd) | (loword << (-rotd & 15));
  return LONG2FIX((value & ~0xFFFFL) | loword);
}

static VALUE
bnum_rrot16(VALUE bnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 16, 0xF);
  uint16_t loword = *RBIGNUM_DIGITS(bnum);
  loword = (loword >> rotd) | (loword << (-rotd & 15));
  return modify_lo16_in_bignum(bnum, loword);
}

static VALUE
fnum_rrot32(VALUE fnum, VALUE rotdist)
{
  long     value  = FIX2LONG(fnum);
  ulong    rotd   = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32   = value;
  lo32 = (lo32 >> rotd) | (lo32 << (-rotd & 31));
#if SIZEOF_LONG == 8
  return LONG2FIX((value & ~0xFFFFFFFFL) | lo32);
#else
  return ULONG2NUM(lo32);
#endif
}

static VALUE
bnum_rrot32(VALUE bnum, VALUE rotdist)
{
  ulong    rotd = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32 = *RBIGNUM_DIGITS(bnum);
  lo32 = (lo32 >> rotd) | (lo32 << (-rotd & 31));
  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_rrot64(VALUE fnum, VALUE rotdist)
{
  ulong    rotd  = value_to_rotdist(rotdist, 64, 0x3F);
  uint64_t value = FIX2ULONG(fnum);
  return ULL2NUM((value >> rotd) | (value << (-rotd & 63)));
}

static VALUE
bnum_rrot64(VALUE bnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 64, 0x3F);
  uint64_t value  = load_64_from_bignum(bnum);
  value = ((value >> rotd) | (value << (-rotd & 63)));
  return modify_lo64_in_bignum(bnum, value);
}

static VALUE
fnum_lrot8(VALUE fnum, VALUE rotdist)
{
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  long    value  = FIX2LONG(fnum);
  uint8_t lobyte = value;
  lobyte = ((lobyte << rotd) | (lobyte >> (-rotd & 7)));
  return LONG2FIX((value & ~0xFFL) | lobyte);
}

static VALUE
bnum_lrot8(VALUE bnum, VALUE rotdist)
{
  ulong   rotd   = value_to_rotdist(rotdist, 8, 0x7);
  uint8_t lobyte = *RBIGNUM_DIGITS(bnum);
  lobyte = ((lobyte << rotd) | (lobyte >> (-rotd & 7))) & 0xFF;
  return modify_lo8_in_bignum(bnum, lobyte);
}

static VALUE
fnum_lrot16(VALUE fnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 16, 0xF);
  long     value  = FIX2LONG(fnum);
  uint16_t loword = value;
  loword = (loword << rotd) | (loword >> (-rotd & 15));
  return LONG2FIX((value & ~0xFFFFL) | loword);
}

static VALUE
bnum_lrot16(VALUE bnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 16, 0xF);
  uint16_t loword = *RBIGNUM_DIGITS(bnum);
  loword = (loword << rotd) | (loword >> (-rotd & 15));
  return modify_lo16_in_bignum(bnum, loword);
}

static VALUE
fnum_lrot32(VALUE fnum, VALUE rotdist)
{
  long     value = FIX2LONG(fnum);
  ulong    rotd  = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32  = value;
  lo32 = (lo32 << rotd) | (lo32 >> (-rotd & 31));
  return LONG2FIX((value & ~0xFFFFFFFFL) | lo32);
}

static VALUE
bnum_lrot32(VALUE bnum, VALUE rotdist)
{
  ulong    rotd = value_to_rotdist(rotdist, 32, 0x1F);
  uint32_t lo32 = *RBIGNUM_DIGITS(bnum);
  lo32  = (lo32 << rotd) | (lo32 >> (-rotd & 31));
  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_lrot64(VALUE fnum, VALUE rotdist)
{
  ulong    rotd = value_to_rotdist(rotdist, 64, 0x3F);
  uint64_t val  = FIX2ULONG(fnum);
  return ULL2NUM((val << rotd) | (val >> (-rotd & 63)));
}

static VALUE
bnum_lrot64(VALUE bnum, VALUE rotdist)
{
  ulong    rotd   = value_to_rotdist(rotdist, 64, 0x3F);
  uint64_t value  = load_64_from_bignum(bnum);
  value = ((value << rotd) | (value >> (-rotd & 63)));
  return modify_lo64_in_bignum(bnum, value);
}

static VALUE
fnum_shl8(VALUE fnum, VALUE shiftdist)
{
  long    value = FIX2LONG(fnum);
  long    sdist = value_to_shiftdist(shiftdist, 8);
  uint8_t lo8   = value;

  if (sdist == 0)
    return fnum;
  else if (sdist >= 8 || sdist <= -8)
    return LONG2FIX(value & ~0xFFL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFL) | (lo8 >> ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFL) | (lo8 << ((ulong)sdist)));  
}

static VALUE
bnum_shl8(VALUE bnum, VALUE shiftdist)
{
  uint16_t lo8   = *RBIGNUM_DIGITS(bnum);
  long     sdist = value_to_shiftdist(shiftdist, 8);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 8 || sdist <= -8)
    lo8 = 0;
  else if (sdist < 0)
    lo8 = lo8 >> ((ulong)-sdist);
  else
    lo8 = lo8 << ((ulong)sdist);

  return modify_lo8_in_bignum(bnum, lo8);
}

static VALUE
fnum_shl16(VALUE fnum, VALUE shiftdist)
{
  long    value = FIX2LONG(fnum);
  long    sdist = value_to_shiftdist(shiftdist, 16);
  uint16_t lo16 = value;

  if (sdist == 0)
    return fnum;
  else if (sdist >= 16 || sdist <= -16)
    return LONG2FIX(value & ~0xFFFFL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFFFL) | (lo16 >> ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFFFL) | (lo16 << ((ulong)sdist)));  
}

static VALUE
bnum_shl16(VALUE bnum, VALUE shiftdist)
{
  uint16_t lo16  = *RBIGNUM_DIGITS(bnum);
  long     sdist = value_to_shiftdist(shiftdist, 16);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 16 || sdist <= -16)
    lo16 = 0;
  else if (sdist < 0)
    lo16 = lo16 >> ((ulong)-sdist);
  else
    lo16 = lo16 << ((ulong)sdist);

  return modify_lo16_in_bignum(bnum, lo16);
}

static VALUE
fnum_shl32(VALUE fnum, VALUE shiftdist)
{
  long     value = FIX2LONG(fnum);
  long     sdist = value_to_shiftdist(shiftdist, 32);
  uint32_t lo32  = value;

  if (sdist == 0)
    return fnum;
  else if (sdist >= 32 || sdist <= -32)
    return LONG2FIX(value & ~0xFFFFFFFFL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFFFFFFFL) | (lo32 >> ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFFFFFFFL) | (lo32 << ((ulong)sdist)));
}

static VALUE
bnum_shl32(VALUE bnum, VALUE shiftdist)
{
  uint32_t lo32  = *RBIGNUM_DIGITS(bnum);
  long     sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 32 || sdist <= -32)
    lo32 = 0;
  else if (sdist < 0)
    lo32 = lo32 >> ((ulong)-sdist);
  else
    lo32 = lo32 << ((ulong)sdist);

  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_shr32(VALUE fnum, VALUE shiftdist)
{
  long    value = FIX2LONG(fnum);
  uint32_t lo32 = value;
  long    sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist == 0)
    return fnum;
  else if (sdist >= 32 || sdist <= -32)
    return LONG2FIX(value & ~0xFFFFFFFFUL);
  else if (sdist < 0)
    return LONG2FIX((value & ~0xFFFFFFFFUL) | (lo32 << ((ulong)-sdist)));
  else
    return LONG2FIX((value & ~0xFFFFFFFFUL) | (lo32 >> ((ulong)sdist)));
}

static VALUE
bnum_shr32(VALUE bnum, VALUE shiftdist)
{
  uint32_t lo32;
  long     sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 32 || sdist <= -32)
    lo32 = 0;
  else if (sdist < 0)
    lo32 = *RBIGNUM_DIGITS(bnum) << ((ulong)-sdist);
  else
    lo32 = *RBIGNUM_DIGITS(bnum) >> ((ulong)sdist);

  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_sar32(VALUE fnum, VALUE shiftdist)
{
#if SIZEOF_LONG == 4
  /* Not enough precision for 32nd bit to be a 1 */
  return fnum_shr32(fnum, shiftdist);
#else
  long     value;
  uint32_t lo32;
  long     sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist == 0)
    return fnum;
  if (sdist < 0)
    return fnum_shl32(fnum, LONG2FIX(-sdist));
  
  value = FIX2LONG(fnum);
  lo32  = value;
  if ((0x80000000UL & lo32) != 0) {
    if (sdist < 32)
      lo32 = (lo32 >> sdist) | ~(0xFFFFFFFFUL >> sdist);
    else
      lo32 = 0xFFFFFFFFUL;
  } else {
    if (sdist < 32)
      lo32 = lo32 >> sdist;
    else
      lo32 = 0;
  }

  return LONG2FIX((value & ~0xFFFFFFFFUL) | lo32);
#endif
}

static VALUE
bnum_sar32(VALUE bnum, VALUE shiftdist)
{
  uint32_t lo32;
  long     sdist = value_to_shiftdist(shiftdist, 32);

  if (sdist == 0)
    return bnum;
  else if (sdist < 0)
    return bnum_shl32(bnum, LONG2FIX(-sdist));
  
  lo32 = *RBIGNUM_DIGITS(bnum);
  if ((0x80000000UL & lo32) != 0) {
    if (sdist < 32 && sdist > -32)
      lo32 = (lo32 >> sdist) | ~(~0UL >> sdist);
    else
      lo32 = 0xFFFFFFFFUL;
  } else {
    if (sdist < 32 && sdist > -32)
      lo32 = lo32 >> sdist;
    else
      lo32 = 0;
  }

  return modify_lo32_in_bignum(bnum, lo32);
}

static VALUE
fnum_shl64(VALUE fnum, VALUE shiftdist)
{
  long sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return fnum;
  else if (sdist >= 64 || sdist <= -64)
    return fix_zero;
  else if (sdist < 0)
    return LONG2FIX(FIX2ULONG(fnum) >> ((ulong)-sdist));
  else
    return ULL2NUM(FIX2ULONG(fnum) << ((ulong)sdist));
}

static VALUE
bnum_shl64(VALUE bnum, VALUE shiftdist)
{
  uint64_t val;
  long     sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 64 || sdist <= -64)
    val = 0ULL;
  else if (sdist < 0)
    val = load_64_from_bignum(bnum) >> ((ulong)-sdist);
  else
    val = load_64_from_bignum(bnum) << ((ulong)sdist);

  return modify_lo64_in_bignum(bnum, val);
}

static VALUE
fnum_shr64(VALUE fnum, VALUE shiftdist)
{
  long sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return fnum;
  else if (sdist >= 64 || sdist <= -64)
    return fix_zero;
  else if (sdist < 0)
    return ULL2NUM(FIX2ULONG(fnum) << ((ulong)-sdist));
  else
    return LONG2FIX(FIX2ULONG(fnum) >> ((ulong)sdist));
}

static VALUE
bnum_shr64(VALUE bnum, VALUE shiftdist)
{
  uint64_t val;
  long     sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return bnum;
  else if (sdist >= 64 || sdist <= -64)
    val = 0ULL;
  else if (sdist < 0)
    val = load_64_from_bignum(bnum) << ((ulong)-sdist);
  else
    val = load_64_from_bignum(bnum) >> ((ulong)sdist);

  return modify_lo64_in_bignum(bnum, val);
}

static VALUE
bnum_sar64(VALUE bnum, VALUE shiftdist)
{
  uint64_t val;
  long     sdist = value_to_shiftdist(shiftdist, 64);

  if (sdist == 0)
    return bnum;
  else if (sdist < 0)
    return bnum_shl64(bnum, LONG2FIX(-sdist));
  
  val = load_64_from_bignum(bnum);
  if ((0x8000000000000000ULL & val) != 0) {
    if (sdist < 64 && sdist > -64)
      val = (val >> sdist) | ~(~0ULL >> sdist);
    else
      val = ~0ULL;
  } else {
    if (sdist < 64 && sdist > -64)
      val = val >> sdist;
    else
      val = 0;
  }

  return modify_lo64_in_bignum(bnum, val);
}

void Init_bit_twiddle(void)
{
  rb_define_method(rb_cFixnum, "popcount", fnum_popcount, 0);
  rb_define_method(rb_cBignum, "popcount", bnum_popcount, 0);
  rb_define_method(rb_cString, "popcount", str_popcount,  0);

  rb_define_method(rb_cFixnum, "lo_bit",   fnum_lo_bit, 0);
  rb_define_method(rb_cBignum, "lo_bit",   bnum_lo_bit, 0);
  rb_define_method(rb_cFixnum, "hi_bit",   fnum_hi_bit, 0);
  rb_define_method(rb_cBignum, "hi_bit",   bnum_hi_bit, 0);

  rb_define_method(rb_cFixnum, "bswap16", fnum_bswap16, 0);
  rb_define_method(rb_cBignum, "bswap16", bnum_bswap16, 0);
  rb_define_method(rb_cFixnum, "bswap32", fnum_bswap32, 0);
  rb_define_method(rb_cBignum, "bswap32", bnum_bswap32, 0);
  rb_define_method(rb_cFixnum, "bswap64", fnum_bswap64, 0);
  rb_define_method(rb_cBignum, "bswap64", bnum_bswap64, 0);

  rb_define_method(rb_cFixnum, "rrot8",  fnum_rrot8,  1);
  rb_define_method(rb_cBignum, "rrot8",  bnum_rrot8,  1);
  rb_define_method(rb_cFixnum, "rrot16", fnum_rrot16, 1);
  rb_define_method(rb_cBignum, "rrot16", bnum_rrot16, 1);
  rb_define_method(rb_cFixnum, "rrot32", fnum_rrot32, 1);
  rb_define_method(rb_cBignum, "rrot32", bnum_rrot32, 1);
  rb_define_method(rb_cFixnum, "rrot64", fnum_rrot64, 1);
  rb_define_method(rb_cBignum, "rrot64", bnum_rrot64, 1);

  rb_define_method(rb_cFixnum, "lrot8",  fnum_lrot8,  1);
  rb_define_method(rb_cBignum, "lrot8",  bnum_lrot8,  1);
  rb_define_method(rb_cFixnum, "lrot16", fnum_lrot16, 1);
  rb_define_method(rb_cBignum, "lrot16", bnum_lrot16, 1);
  rb_define_method(rb_cFixnum, "lrot32", fnum_lrot32, 1);
  rb_define_method(rb_cBignum, "lrot32", bnum_lrot32, 1);
  rb_define_method(rb_cFixnum, "lrot64", fnum_lrot64, 1);
  rb_define_method(rb_cBignum, "lrot64", bnum_lrot64, 1);

  rb_define_method(rb_cFixnum, "shl8",   fnum_shl8,  1);
  rb_define_method(rb_cBignum, "shl8",   bnum_shl8,  1);
  rb_define_method(rb_cFixnum, "shl16",  fnum_shl16, 1);
  rb_define_method(rb_cBignum, "shl16",  bnum_shl16, 1);
  rb_define_method(rb_cFixnum, "shl32",  fnum_shl32, 1);
  rb_define_method(rb_cBignum, "shl32",  bnum_shl32, 1);
  rb_define_method(rb_cFixnum, "shl64",  fnum_shl64, 1);
  rb_define_method(rb_cBignum, "shl64",  bnum_shl64, 1);
  rb_define_method(rb_cFixnum, "shr32",  fnum_shr32, 1);
  rb_define_method(rb_cBignum, "shr32",  bnum_shr32, 1);
  rb_define_method(rb_cFixnum, "shr64",  fnum_shr64, 1);
  rb_define_method(rb_cBignum, "shr64",  bnum_shr64, 1);
  rb_define_method(rb_cFixnum, "sar32",  fnum_sar32, 1);
  rb_define_method(rb_cBignum, "sar32",  bnum_sar32, 1);

  /* Fixnum doesn't have enough precision that the 64th bit could be a 1
   * (Or if it does, our preprocessor directives above will make sure this won't compile) */
  rb_define_method(rb_cFixnum, "sar64",  fnum_shr64, 1);
  rb_define_method(rb_cBignum, "sar64",  bnum_sar64, 1);
}
