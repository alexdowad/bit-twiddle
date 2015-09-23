#include <ruby.h>
#include "bt_bignum.h"

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
#error "Sorry, Bignum#bswap32 and Bignum#arith_rshift32 will not work if sizeof(BDIGIT) < 4. Please report this error."
#elif SIZEOF_BDIGIT > 8
#error "Sorry, several methods will not work if sizeof(BDIGIT) > 8. Please report this error."
#elif SIZEOF_LONG > 8
#error "Sorry, Fixnum#arith_rshift64 will not work if sizeof(long) > 8. Please report this error."
#endif

#if HAVE_BSWAP16 == 0
/* stupid bug in GCC 4.7 */
static inline uint16_t __builtin_bswap16(uint16_t value)
{
  return (value >> 8) | (value << 8);
}
#endif

static int
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

static long
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
static ulong
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

static void
store_64_into_bnum(VALUE bnum, uint64_t int64)
{
  BDIGIT *dest = RBIGNUM_DIGITS(bnum);
  size_t  len  = RBIGNUM_LEN(bnum);

  if (SIZEOF_BDIGIT == 8) {
    *dest = int64;
  } else {
    if (len > 1) {
      *dest     = int64;
      *(dest+1) = int64 >> 32;
    } else if ((int64 & (0xFFFFFFFFULL << 32)) == 0) {
      /* the high 4 bytes are zero anyways */
      *dest = int64;
    } else {
      rb_big_resize(bnum, 2);
      dest      = RBIGNUM_DIGITS(bnum); /* may have moved */
      *dest     = int64;
      *(dest+1) = int64 >> 32;
    }
  }
}

static uint64_t
load_64_from_bignum(VALUE bnum)
{
  BDIGIT *src = RBIGNUM_DIGITS(bnum);
  size_t  len = RBIGNUM_LEN(bnum);
  uint64_t result = *src;

  if (SIZEOF_BDIGIT == 4 && len > 1)
    result += ((uint64_t)*(src+1)) << 32;

  return result;
}

static VALUE
modify_lo8_in_bignum(VALUE bnum, uint8_t lo8)
{
  VALUE result;

  if (lo8 == (uint8_t)*RBIGNUM_DIGITS(bnum))
    return bnum;

  result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = (*RBIGNUM_DIGITS(bnum) & ~0xFFL) | lo8;
  return result;
}

static VALUE
modify_lo16_in_bignum(VALUE bnum, uint16_t lo16)
{
  VALUE result;

  if (lo16 == (uint16_t)*RBIGNUM_DIGITS(bnum))
    return bnum;

  result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = (*RBIGNUM_DIGITS(bnum) & ~0xFFFFL) | lo16;
  return result;
}

static VALUE
modify_lo32_in_bignum(VALUE bnum, uint32_t lo32)
{
  BDIGIT value;
  VALUE  result;

  if (lo32 == (uint32_t)*RBIGNUM_DIGITS(bnum))
    return bnum;

  if (SIZEOF_BDIGIT == 4)
    value = lo32;
  else
    value = (*RBIGNUM_DIGITS(bnum) & ~0xFFFFFFFFL) | lo32;

#if SIZEOF_LONG == 4
  /* if a 'long' is only 4 bytes, a 32-bit number could be promoted to Bignum
   * then modifying the low 32 bits could make it fixable again */
  if (RBIGNUM_LEN(bnum) == 1 && FIXABLE(value))
    return LONG2FIX(value);
#endif

  result = rb_big_clone(bnum);
  *RBIGNUM_DIGITS(result) = value;
  return result;
}

static VALUE
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

/* Document-method: Fixnum#popcount
 * Document-method: Bignum#popcount
 * Return the number of 1 bits in this integer.
 * @example
 *   7.popcount   # => 3
 *   255.popcount # => 8
 * @return [Fixnum]
 */
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

/* Return the number of 1 bits in all the bytes of this `String`.
 * @example
 *   "abc".popcount # => 10
 * @return [Fixnum]
 */
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

/* Document-method: Fixnum#lo_bit
 * Document-method: Bignum#lo_bit
 * Return the index of the lowest 1 bit, where the least-significant bit is index 1.
 * If this integer is 0, return 0.
 * @example
 *   1.lo_bit   # => 1
 *   128.lo_bit # => 8
 * @return [Fixnum]
 */
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

/* Document-method: Fixnum#hi_bit
 * Document-method: Bignum#hi_bit
 * Return the index of the highest 1 bit, where the least-significant bit is index 1.
 * If this integer is 0, return 0.
 * @example
 *   1.hi_bit   # => 1
 *   255.hi_bit # => 8
 * @return [Fixnum]
 */
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

/* Document-method: Fixnum#bswap16
 * Document-method: Bignum#bswap16
 * Reverse the least-significant and second least-significant bytes of this integer.
 * @example
 *   0xFF00.bswap16 # => 255
 *   0x00FF.bswap16 # => 65280
 * @return [Integer]
 */
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

/* Document-method: Fixnum#bswap32
 * Document-method: Bignum#bswap32
 * Reverse the least-significant 4 bytes of this integer.
 *
 * Does not reverse bits within each byte. This can be used to swap endianness
 * of a 32-bit integer.
 *
 * @example
 *   0xaabbccdd.bswap32.to_s(16) # => "ddccbbaa"
 *
 * @return [Integer]
 */
static VALUE
fnum_bswap32(VALUE fnum)
{
  long value;

  if (SIZEOF_LONG == 4) {
    /* the size of a Fixnum is always the same as 'long'
     * and the C standard guarantees 'long' is at least 32 bits
     * but a couple bits are used for tagging, so the usable precision could
     * be less than 32 bits...
     * That is why we have to use a '2NUM' function, not '2FIX' */
    return ULONG2NUM(__builtin_bswap32(FIX2LONG(fnum)));
  } else {
    value = FIX2LONG(fnum);
    return LONG2FIX((value & ~0xFFFFFFFFL) | __builtin_bswap32(value));
  }
}

static VALUE
bnum_bswap32(VALUE bnum)
{
  return modify_lo32_in_bignum(bnum, __builtin_bswap32(*RBIGNUM_DIGITS(bnum)));
}

/* Document-method: Fixnum#bswap64
 * Document-method: Bignum#bswap64
 * Reverse the least-significant 8 bytes of this integer.
 *
 * Does not reverse bits within each byte. This can be used to swap endianness
 * of a 64-bit integer.
 *
 * @example
 *   0xaabbccdd.bswap64.to_s(16) # => "ddccbbaa00000000"
 *
 * @return [Integer]
 */
static VALUE
fnum_bswap64(VALUE fnum)
{
  return ULL2NUM(__builtin_bswap64(FIX2LONG(fnum)));
}

static VALUE
bnum_bswap64(VALUE bnum)
{
  return modify_lo64_in_bignum(bnum, __builtin_bswap64(load_64_from_bignum(bnum)));
}

#define def_rot_helpers(bits) \
  static inline uint##bits##_t rrot##bits(uint##bits##_t value, VALUE rotdist) { \
    ulong rotd = value_to_rotdist(rotdist, bits, bits-1); \
    return (value >> rotd) | (value << (-rotd & (bits-1))); \
  } \
  static inline uint##bits##_t lrot##bits(uint##bits##_t value, VALUE rotdist) { \
    ulong rotd = value_to_rotdist(rotdist, bits, bits-1); \
    return (value << rotd) | (value >> (-rotd & (bits-1))); \
  }

def_rot_helpers(8);
def_rot_helpers(16);
def_rot_helpers(32);
def_rot_helpers(64);

/* Document-method: Fixnum#rrot8
 * Document-method: Bignum#rrot8
 * Right-rotation ("circular shift") of the low 8 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the left
 * instead.
 *
 * @example
 *   0b01110001.rrot8(1).to_s(2) # => "10111000"
 *   0b01110001.rrot8(3).to_s(2) # => "101110"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_rrot8(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFL) | rrot8(value, rotdist));
}

static VALUE
bnum_rrot8(VALUE bnum, VALUE rotdist)
{
  return modify_lo8_in_bignum(bnum, rrot8(*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Fixnum#rrot16
 * Document-method: Bignum#rrot16
 * Right-rotation ("circular shift") of the low 16 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the left
 * instead.
 *
 * @example
 *   0b0111000101110001.rrot16(1).to_s(2) # => "1011100010111000"
 *   0b0111000101110001.rrot16(3).to_s(2) # => "10111000101110"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_rrot16(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFFL) | rrot16(value, rotdist));
}

static VALUE
bnum_rrot16(VALUE bnum, VALUE rotdist)
{
  return modify_lo16_in_bignum(bnum, rrot16(*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Fixnum#rrot32
 * Document-method: Bignum#rrot32
 * Right-rotation ("circular shift") of the low 32 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the left
 * instead.
 *
 * @example
 *   0xaabbccdd.rrot32(4).to_s(16) # => "daabbccd"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_rrot32(VALUE fnum, VALUE rotdist)
{
  long     value  = FIX2LONG(fnum);
  if (SIZEOF_LONG == 8)
    return LONG2FIX((value & ~0xFFFFFFFFL) | rrot32(value, rotdist));
  else
    return ULONG2NUM(rrot32(value, rotdist));
}

static VALUE
bnum_rrot32(VALUE bnum, VALUE rotdist)
{
  return modify_lo32_in_bignum(bnum, rrot32(*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Fixnum#rrot64
 * Document-method: Bignum#rrot64
 * Right-rotation ("circular shift") of the low 64 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the left
 * instead.
 *
 * @example
 *   0x11223344aabbccdd.rrot64(4).to_s(16) # => "d11223344aabbccd"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_rrot64(VALUE fnum, VALUE rotdist)
{
  return ULL2NUM(rrot64(FIX2ULONG(fnum), rotdist));
}

static VALUE
bnum_rrot64(VALUE bnum, VALUE rotdist)
{
  return modify_lo64_in_bignum(bnum, rrot64(load_64_from_bignum(bnum), rotdist));
}

/* Document-method: Fixnum#lrot8
 * Document-method: Bignum#lrot8
 * Left-rotation ("circular shift") of the low 8 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the right
 * instead.
 *
 * @example
 *   0b01110001.lrot8(1).to_s(2) # => "11100010"
 *   0b01110001.lrot8(3).to_s(2) # => "10001011"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_lrot8(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFL) | lrot8(value, rotdist));
}

static VALUE
bnum_lrot8(VALUE bnum, VALUE rotdist)
{
  return modify_lo8_in_bignum(bnum, lrot8(*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Fixnum#lrot16
 * Document-method: Bignum#lrot16
 * Left-rotation ("circular shift") of the low 16 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the right
 * instead.
 *
 * @example
 *   0b0111000101110001.lrot16(1).to_s(2) # => "1110001011100010"
 *   0b0111000101110001.lrot16(3).to_s(2) # => "1000101110001011"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_lrot16(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFFL) | lrot16(value, rotdist));
}

static VALUE
bnum_lrot16(VALUE bnum, VALUE rotdist)
{
  return modify_lo16_in_bignum(bnum, lrot16(*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Fixnum#lrot32
 * Document-method: Bignum#lrot32
 * Left-rotation ("circular shift") of the low 32 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the right
 * instead.
 *
 * @example
 *   0xaabbccdd.lrot32(4).to_s(16) # => "abbccdda"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_lrot32(VALUE fnum, VALUE rotdist)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFFFFFFL) | lrot32(value, rotdist));
}

static VALUE
bnum_lrot32(VALUE bnum, VALUE rotdist)
{
  return modify_lo32_in_bignum(bnum, lrot32(*RBIGNUM_DIGITS(bnum), rotdist));
}

/* Document-method: Fixnum#lrot64
 * Document-method: Bignum#lrot64
 * Left-rotation ("circular shift") of the low 64 bits in this integer.
 *
 * If the rotate distance is negative, the bit rotation will be to the right
 * instead.
 *
 * @example
 *   0x11223344aabbccdd.lrot64(4).to_s(16) # => "1223344aabbccdd1"
 *
 * @param rotdist [Integer] Number of bit positions to rotate by
 * @return [Integer]
 */
static VALUE
fnum_lrot64(VALUE fnum, VALUE rotdist)
{
  return ULL2NUM(lrot64(FIX2ULONG(fnum), rotdist));
}

static VALUE
bnum_lrot64(VALUE bnum, VALUE rotdist)
{
  return modify_lo64_in_bignum(bnum, lrot64(load_64_from_bignum(bnum), rotdist));
}

#define def_shift_helpers(bits) \
  static uint##bits##_t lshift##bits(uint##bits##_t value, VALUE shiftdist) { \
    long sdist = value_to_shiftdist(shiftdist, bits); \
    if (sdist >= bits || sdist <= -bits) return 0; \
    else if (sdist < 0) return value >> ((uint)-sdist); \
    else return value << (uint)sdist; \
  } \
  static uint##bits##_t rshift##bits(uint##bits##_t value, VALUE shiftdist) { \
    long sdist = value_to_shiftdist(shiftdist, bits); \
    if (sdist >= bits || sdist <= -bits) return 0; \
    else if (sdist < 0) return value << ((uint)-sdist); \
    else return value >> (uint)sdist; \
  } \
  static uint##bits##_t arith_rshift##bits(uint##bits##_t value, VALUE shiftdist) { \
    long sdist = value_to_shiftdist(shiftdist, bits); \
    if (sdist >= bits) { \
      if ((((uint##bits##_t)1 << (bits - 1)) & value) != 0) \
        return ~0; \
      else \
        return 0; \
    } else if (sdist <= -bits) { \
      return 0; \
    } else if (sdist < 0) { \
      return value << ((uint)-sdist); \
    } else if (RSHIFT_IS_ARITH) { \
      return (int##bits##_t)value >> (int)sdist; \
    } else if ((((uint##bits##_t)1 << (bits - 1)) & value) != 0) { \
      return (value >> sdist) | ~(((uint##bits##_t)~0) >> sdist); \
    } else { \
      return value >> sdist; \
    } \
  }

def_shift_helpers(8);
def_shift_helpers(16);
def_shift_helpers(32);
def_shift_helpers(64);

/* Document-method: Fixnum#lshift8
 * Document-method: Bignum#lshift8
 * Left-shift of the low 8 bits in this integer.
 *
 * If the shift distance is negative, a right shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 7 or less than -7, the low 8 bits will all be zeroed.
 *
 * @example
 *   0x11223344.lshift8(1).to_s(16) # => "11223388"
 *   0x11223344.lshift8(2).to_s(16) # => "11223310"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_lshift8(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX((value & ~0xFFL) | lshift8(value, shiftdist));
}

static VALUE
bnum_lshift8(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo8_in_bignum(bnum, lshift8(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Fixnum#lshift16
 * Document-method: Bignum#lshift16
 * Left-shift of the low 16 bits in this integer.
 *
 * If the shift distance is negative, a right shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 15 or less than -15, the low 16 bits will all be zeroed.
 *
 * @example
 *   0x11223344.lshift16(1).to_s(16) # => "11226688"
 *   0x11223344.lshift16(2).to_s(16) # => "1122cd10"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_lshift16(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX((value & ~0xFFFFL) | lshift16(value, shiftdist));
}

static VALUE
bnum_lshift16(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo16_in_bignum(bnum, lshift16(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Fixnum#lshift32
 * Document-method: Bignum#lshift32
 * Left-shift of the low 32 bits in this integer.
 *
 * If the shift distance is negative, a right shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 31 or less than -31, the low 32 bits will all be zeroed.
 *
 * @example
 *   0x11223344.lshift32(1).to_s(16) # => "22446688"
 *   0x11223344.lshift32(2).to_s(16) # => "4488cd10"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_lshift32(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX((value & ~0xFFFFFFFFL) | lshift32(value, shiftdist));
}

static VALUE
bnum_lshift32(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo32_in_bignum(bnum, lshift32(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Fixnum#lshift64
 * Document-method: Bignum#lshift64
 * Left-shift of the low 64 bits in this integer.
 *
 * If the shift distance is negative, a right shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 63 or less than -63, the low 64 bits will all be zeroed.
 *
 * @example
 *   0x1122334411223344.lshift64(1).to_s(16) # => "2244668822446688"
 *   0x1122334411223344.lshift64(2).to_s(16) # => "4488cd104488cd10"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_lshift64(VALUE fnum, VALUE shiftdist)
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
bnum_lshift64(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo64_in_bignum(bnum, lshift64(load_64_from_bignum(bnum), shiftdist));
}

/* Document-method: Fixnum#rshift8
 * Document-method: Bignum#rshift8
 * Right-shift of the low 8 bits in this integer.
 *
 * If the shift distance is negative, a left shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 7 or less than -7, the low 8 bits will all be zeroed.
 *
 * @example
 *   0x11223344.rshift8(1).to_s(16) # => "11223322"
 *   0x11223344.rshift8(2).to_s(16) # => "11223311"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_rshift8(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX((value & ~0xFFL) | rshift8(value, shiftdist));
}

static VALUE
bnum_rshift8(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo8_in_bignum(bnum, rshift8(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Fixnum#rshift16
 * Document-method: Bignum#rshift16
 * Right-shift of the low 16 bits in this integer.
 *
 * If the shift distance is negative, a left shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 15 or less than -15, the low 16 bits will all be zeroed.
 *
 * @example
 *   0x11223344.rshift16(1).to_s(16) # => "112219a2"
 *   0x11223344.rshift16(2).to_s(16) # => "11220cd1"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_rshift16(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX((value & ~0xFFFFL) | rshift16(value, shiftdist));
}

static VALUE
bnum_rshift16(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo16_in_bignum(bnum, rshift16(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Fixnum#rshift32
 * Document-method: Bignum#rshift32
 * Right-shift of the low 32 bits in this integer.
 *
 * If the shift distance is negative, a left shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 31 or less than -31, the low 32 bits will all be zeroed.
 *
 * @example
 *   0x11223344.rshift32(1).to_s(16) # => "89119a2"
 *   0x11223344.rshift32(2).to_s(16) # => "4488cd1"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_rshift32(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX((value & ~0xFFFFFFFFL) | rshift32(value, shiftdist));
}

static VALUE
bnum_rshift32(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo32_in_bignum(bnum, rshift32(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Fixnum#rshift64
 * Document-method: Bignum#rshift64
 * Right-shift of the low 64 bits in this integer.
 *
 * If the shift distance is negative, a left shift will be performed instead.
 * The vacated bit positions will be filled with 0 bits. If shift distance is
 * more than 63 or less than -63, the low 64 bits will all be zeroed.
 *
 * @example
 *   0x1122334411223344.rshift64(1).to_s(16) # => "89119a2089119a2"
 *   0x1122334411223344.rshift64(2).to_s(16) # => "4488cd104488cd1"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_rshift64(VALUE fnum, VALUE shiftdist)
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
bnum_rshift64(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo64_in_bignum(bnum, rshift64(load_64_from_bignum(bnum), shiftdist));
}

/* Document-method: Fixnum#arith_rshift8
 * Document-method: Bignum#arith_rshift8
 * Arithmetic right-shift of the low 8 bits in this integer.
 *
 * If bit 8 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
 * they will be filled with 0s. Or, if the shift distance is negative, a left shift
 * will be performed instead, and the vacated bit positions will be filled with 0s.
 *
 * @example
 *   0xaabbccdd.arith_rshift8(1).to_s(16) # => "aabbccee"
 *   0xaabbccdd.arith_rshift8(2).to_s(16) # => "aabbccf7"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_arith_rshift8(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX((value & ~0xFFUL) | arith_rshift8(value, shiftdist));
}

static VALUE
bnum_arith_rshift8(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo8_in_bignum(bnum, arith_rshift8(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Fixnum#arith_rshift16
 * Document-method: Bignum#arith_rshift16
 * Arithmetic right-shift of the low 16 bits in this integer.
 *
 * If bit 16 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
 * they will be filled with 0s. Or, if the shift distance is negative, a left shift
 * will be performed instead, and the vacated bit positions will be filled with 0s.
 *
 * @example
 *   0xaabbccdd.arith_rshift16(1).to_s(16) # => "aabbe66e"
 *   0xaabbccdd.arith_rshift16(2).to_s(16) # => "aabbf337"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_arith_rshift16(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX((value & ~0xFFFFUL) | arith_rshift16(value, shiftdist));
}

static VALUE
bnum_arith_rshift16(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo16_in_bignum(bnum, arith_rshift16(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Fixnum#arith_rshift32
 * Document-method: Bignum#arith_rshift32
 * Arithmetic right-shift of the low 32 bits in this integer.
 *
 * If bit 32 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
 * they will be filled with 0s. Or, if the shift distance is negative, a left shift
 * will be performed instead, and the vacated bit positions will be filled with 0s.
 *
 * @example
 *   0xaabbccddaabbccdd.arith_rshift32(1).to_s(16) # => "d55de66e"
 *   0xaabbccddaabbccdd.arith_rshift32(2).to_s(16) # => "eaaef337"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_arith_rshift32(VALUE fnum, VALUE shiftdist)
{
  long value = FIX2LONG(fnum);
  if (shiftdist == fix_zero)
    return fnum;
  else
    return LONG2FIX((value & ~0xFFFFFFFFUL) | arith_rshift32(value, shiftdist));
}

static VALUE
bnum_arith_rshift32(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo32_in_bignum(bnum, arith_rshift32(*RBIGNUM_DIGITS(bnum), shiftdist));
}

/* Document-method: Fixnum#arith_rshift64
 * Document-method: Bignum#arith_rshift64
 * Arithmetic right-shift of the low 64 bits in this integer.
 *
 * If bit 64 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
 * they will be filled with 0s. Or, if the shift distance is negative, a left shift
 * will be performed instead, and the vacated bit positions will be filled with 0s.
 *
 * @example
 *   0xaabbccddaabbccdd.arith_rshift64(1).to_s(16) # => "d55de66ed55de66e"
 *   0xaabbccddaabbccdd.arith_rshift64(2).to_s(16) # => "eaaef3376aaef337"
 *
 * @param shiftdist [Integer] Number of bit positions to shift by
 * @return [Integer]
 */
static VALUE
fnum_arith_rshift64(VALUE fnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return fnum;
  else
    return ULONG2NUM(arith_rshift64(FIX2LONG(fnum), shiftdist));
}

static VALUE
bnum_arith_rshift64(VALUE bnum, VALUE shiftdist)
{
  if (shiftdist == fix_zero)
    return bnum;
  else
    return modify_lo64_in_bignum(bnum, arith_rshift64(load_64_from_bignum(bnum), shiftdist));
}

static const uint8_t bitreverse_table[] =
{
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

static inline uint8_t reverse8(uint8_t value)
{
  if (SIZEOF_LONG == 8)
    /* 64-bit CPU
     * Thanks to the Bit Twiddling Hacks page:
     * http://graphics.stanford.edu/~seander/bithacks.html */
    return (value * 0x0202020202UL & 0x010884422010UL) % 1023;
  else
    /* 32-bit CPU */
    return bitreverse_table[value];
}

static inline uint16_t reverse16(uint16_t value)
{
  return (bitreverse_table[value & 0xFF] << 8) | bitreverse_table[value >> 8];
}

static inline uint32_t reverse32(uint32_t value)
{
  return ((uint32_t)reverse16(value) << 16) | reverse16(value >> 16);
}

static inline uint64_t reverse64(uint64_t value)
{
  return ((uint64_t)reverse32(value) << 32) | reverse32(value >> 32);
}

/* Document-method: Fixnum#bitreverse8
 * Document-method: Bignum#bitreverse8
 * Reverse the low 8 bits in this integer.
 *
 * @example
 *   0b01101011.bitreverse8.to_s(2) # => "11010110"
 *
 * @return [Integer]
 */
static VALUE
fnum_bitreverse8(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFL) | reverse8(value));
}

static VALUE
bnum_bitreverse8(VALUE bnum)
{
  return modify_lo8_in_bignum(bnum, reverse8(*RBIGNUM_DIGITS(bnum)));
}

/* Document-method: Fixnum#bitreverse16
 * Document-method: Bignum#bitreverse16
 * Reverse the low 16 bits in this integer.
 *
 * @example
 *   0b0110101100001011.bitreverse16.to_s(2) # => "1101000011010110"
 *
 * @return [Integer]
 */
static VALUE
fnum_bitreverse16(VALUE fnum)
{
  long value = FIX2LONG(fnum);
  return LONG2FIX((value & ~0xFFFFL) | reverse16(value));
}

static VALUE
bnum_bitreverse16(VALUE bnum)
{
  return modify_lo16_in_bignum(bnum, reverse16(*RBIGNUM_DIGITS(bnum)));
}

/* Document-method: Fixnum#bitreverse32
 * Document-method: Bignum#bitreverse32
 * Reverse the low 32 bits in this integer.
 *
 * @example
 *   0x12341234.bitreverse32.to_s(16) # => "2c482c48"
 *
 * @return [Integer]
 */
static VALUE
fnum_bitreverse32(VALUE fnum)
{
  long     value = FIX2LONG(fnum);
  uint32_t lo32  = value;
  if (SIZEOF_LONG == 4)
    return ULONG2NUM(reverse32(lo32));
  else
    return LONG2FIX((value & ~0xFFFFFFFFL) | reverse32(lo32));
}

static VALUE
bnum_bitreverse32(VALUE bnum)
{
  return modify_lo32_in_bignum(bnum, reverse32(*RBIGNUM_DIGITS(bnum)));
}

/* Document-method: Fixnum#bitreverse64
 * Document-method: Bignum#bitreverse64
 * Reverse the low 64 bits in this integer.
 *
 * @example
 *   0xabcd1234abcd1234.bitreverse64.to_s(16) # => "2c48b3d52c48b3d5"
 *
 * @return [Integer]
 */
static VALUE
fnum_bitreverse64(VALUE fnum)
{
  /* on a 32-bit system, do we want sign extension of a negative 32-bit value into 64 bits??? */
  return ULL2NUM(reverse64(FIX2ULONG(fnum)));
}

static VALUE
bnum_bitreverse64(VALUE bnum)
{
  return modify_lo64_in_bignum(bnum, reverse64(load_64_from_bignum(bnum)));
}

/* Add all `bit-twiddle` methods directly to `Fixnum` and `Bignum`. */
static VALUE
bt_add_core_extensions(VALUE self)
{
  rb_define_method(rb_cFixnum, "popcount", fnum_popcount, 0);
  rb_define_method(rb_cBignum, "popcount", bnum_popcount, 0);
  rb_define_method(rb_cString, "popcount", str_popcount,  0);

  rb_define_method(rb_cFixnum, "lo_bit",   fnum_lo_bit, 0);
  rb_define_method(rb_cBignum, "lo_bit",   bnum_lo_bit, 0);
  rb_define_method(rb_cFixnum, "hi_bit",   fnum_hi_bit, 0);
  rb_define_method(rb_cBignum, "hi_bit",   bnum_hi_bit, 0);

  rb_define_method(rb_cFixnum, "bswap16",  fnum_bswap16, 0);
  rb_define_method(rb_cBignum, "bswap16",  bnum_bswap16, 0);
  rb_define_method(rb_cFixnum, "bswap32",  fnum_bswap32, 0);
  rb_define_method(rb_cBignum, "bswap32",  bnum_bswap32, 0);
  rb_define_method(rb_cFixnum, "bswap64",  fnum_bswap64, 0);
  rb_define_method(rb_cBignum, "bswap64",  bnum_bswap64, 0);

  rb_define_method(rb_cFixnum, "rrot8",    fnum_rrot8,  1);
  rb_define_method(rb_cBignum, "rrot8",    bnum_rrot8,  1);
  rb_define_method(rb_cFixnum, "rrot16",   fnum_rrot16, 1);
  rb_define_method(rb_cBignum, "rrot16",   bnum_rrot16, 1);
  rb_define_method(rb_cFixnum, "rrot32",   fnum_rrot32, 1);
  rb_define_method(rb_cBignum, "rrot32",   bnum_rrot32, 1);
  rb_define_method(rb_cFixnum, "rrot64",   fnum_rrot64, 1);
  rb_define_method(rb_cBignum, "rrot64",   bnum_rrot64, 1);

  rb_define_method(rb_cFixnum, "lrot8",    fnum_lrot8,  1);
  rb_define_method(rb_cBignum, "lrot8",    bnum_lrot8,  1);
  rb_define_method(rb_cFixnum, "lrot16",   fnum_lrot16, 1);
  rb_define_method(rb_cBignum, "lrot16",   bnum_lrot16, 1);
  rb_define_method(rb_cFixnum, "lrot32",   fnum_lrot32, 1);
  rb_define_method(rb_cBignum, "lrot32",   bnum_lrot32, 1);
  rb_define_method(rb_cFixnum, "lrot64",   fnum_lrot64, 1);
  rb_define_method(rb_cBignum, "lrot64",   bnum_lrot64, 1);

  rb_define_method(rb_cFixnum, "lshift8",   fnum_lshift8,  1);
  rb_define_method(rb_cBignum, "lshift8",   bnum_lshift8,  1);
  rb_define_method(rb_cFixnum, "lshift16",  fnum_lshift16, 1);
  rb_define_method(rb_cBignum, "lshift16",  bnum_lshift16, 1);
  rb_define_method(rb_cFixnum, "lshift32",  fnum_lshift32, 1);
  rb_define_method(rb_cBignum, "lshift32",  bnum_lshift32, 1);
  rb_define_method(rb_cFixnum, "lshift64",  fnum_lshift64, 1);
  rb_define_method(rb_cBignum, "lshift64",  bnum_lshift64, 1);

  rb_define_method(rb_cFixnum, "rshift8",   fnum_rshift8,  1);
  rb_define_method(rb_cBignum, "rshift8",   bnum_rshift8,  1);
  rb_define_method(rb_cFixnum, "rshift16",  fnum_rshift16, 1);
  rb_define_method(rb_cBignum, "rshift16",  bnum_rshift16, 1);
  rb_define_method(rb_cFixnum, "rshift32",  fnum_rshift32, 1);
  rb_define_method(rb_cBignum, "rshift32",  bnum_rshift32, 1);
  rb_define_method(rb_cFixnum, "rshift64",  fnum_rshift64, 1);
  rb_define_method(rb_cBignum, "rshift64",  bnum_rshift64, 1);

  rb_define_method(rb_cFixnum, "arith_rshift8",  fnum_arith_rshift8,  1);
  rb_define_method(rb_cBignum, "arith_rshift8",  bnum_arith_rshift8,  1);
  rb_define_method(rb_cFixnum, "arith_rshift16", fnum_arith_rshift16, 1);
  rb_define_method(rb_cBignum, "arith_rshift16", bnum_arith_rshift16, 1);
  rb_define_method(rb_cFixnum, "arith_rshift32", fnum_arith_rshift32, 1);
  rb_define_method(rb_cBignum, "arith_rshift32", bnum_arith_rshift32, 1);
  rb_define_method(rb_cFixnum, "arith_rshift64", fnum_arith_rshift64, 1);
  rb_define_method(rb_cBignum, "arith_rshift64", bnum_arith_rshift64, 1);

  rb_define_method(rb_cFixnum, "bitreverse8",  fnum_bitreverse8,  0);
  rb_define_method(rb_cBignum, "bitreverse8",  bnum_bitreverse8,  0);
  rb_define_method(rb_cFixnum, "bitreverse16", fnum_bitreverse16, 0);
  rb_define_method(rb_cBignum, "bitreverse16", bnum_bitreverse16, 0);
  rb_define_method(rb_cFixnum, "bitreverse32", fnum_bitreverse32, 0);
  rb_define_method(rb_cBignum, "bitreverse32", bnum_bitreverse32, 0);
  rb_define_method(rb_cFixnum, "bitreverse64", fnum_bitreverse64, 0);
  rb_define_method(rb_cBignum, "bitreverse64", bnum_bitreverse64, 0);

  return Qnil;
}

/* Wrapper functions are used for methods on BitTwiddle module */
#define def_wrapper(name) \
  static VALUE bt_ ## name(VALUE self, VALUE num) \
  { \
    retry: \
    switch (TYPE(num)) { \
    case T_FIXNUM: return fnum_ ## name(num); \
    case T_BIGNUM: return bnum_ ## name(num); \
    default: num = rb_to_int(num); goto retry; \
    } \
  }
#define def_wrapper_with_arg(name) \
  static VALUE bt_ ## name(VALUE self, VALUE num, VALUE arg) \
  { \
    retry: \
    switch (TYPE(num)) { \
    case T_FIXNUM: return fnum_ ## name(num, arg); \
    case T_BIGNUM: return bnum_ ## name(num, arg); \
    default: num = rb_to_int(num); goto retry; \
    } \
  }

def_wrapper(popcount);
def_wrapper(lo_bit);
def_wrapper(hi_bit);
def_wrapper(bswap16);
def_wrapper(bswap32);
def_wrapper(bswap64);
def_wrapper_with_arg(lrot8);
def_wrapper_with_arg(lrot16);
def_wrapper_with_arg(lrot32);
def_wrapper_with_arg(lrot64);
def_wrapper_with_arg(rrot8);
def_wrapper_with_arg(rrot16);
def_wrapper_with_arg(rrot32);
def_wrapper_with_arg(rrot64);
def_wrapper_with_arg(lshift8);
def_wrapper_with_arg(lshift16);
def_wrapper_with_arg(lshift32);
def_wrapper_with_arg(lshift64);
def_wrapper_with_arg(rshift8);
def_wrapper_with_arg(rshift16);
def_wrapper_with_arg(rshift32);
def_wrapper_with_arg(rshift64);
def_wrapper_with_arg(arith_rshift8);
def_wrapper_with_arg(arith_rshift16);
def_wrapper_with_arg(arith_rshift32);
def_wrapper_with_arg(arith_rshift64);
def_wrapper(bitreverse8);
def_wrapper(bitreverse16);
def_wrapper(bitreverse32);
def_wrapper(bitreverse64);

/* Document-class: Fixnum
 * Ruby's good old Fixnum.
 *
 * `require "bit-twiddle/core_ext"` before trying to use any of the below methods.
 */
/* Document-class: Bignum
 * Ruby's good old Bignum.
 *
 * `require "bit-twiddle/core_ext"` before trying to use any of the below methods.
 */
/* Document-class: String
 * Ruby's good old String.
 *
 * `require "bit-twiddle/core_ext"` before trying to use any of the below methods.
 */
void Init_bit_twiddle(void)
{
  VALUE rb_mBitTwiddle = rb_define_module("BitTwiddle");

  rb_define_singleton_method(rb_mBitTwiddle, "add_core_extensions", bt_add_core_extensions, 0);

  /* Return the number of 1 bits in `int`.
   * @example
   *   BitTwiddle.popcount(7)   # => 3
   *   BitTwiddle.popcount(255) # => 8
   * @param int [Integer] The integer to operate on
   * @return [Fixnum]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "popcount", bt_popcount, 1);
  /* Return the index of the lowest 1 bit, where the least-significant bit is index 1.
   * If this integer is 0, return 0.
   * @example
   *   BitTwiddle.lo_bit(1)   # => 1
   *   BitTwiddle.lo_bit(128) # => 8
   * @param int [Integer] The integer to operate on
   * @return [Fixnum]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lo_bit",   bt_lo_bit,   1);
  /* Return the index of the highest 1 bit, where the least-significant bit is index 1.
   * If `int` is 0, return 0.
   * @example
   *   BitTwiddle.hi_bit(1)   # => 1
   *   BitTwiddle.hi_bit(255) # => 8
   * @param int [Integer] The integer to operate on
   * @return [Fixnum]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "hi_bit",   bt_hi_bit,   1);
  /* Reverse the least-significant and second least-significant bytes of `int`.
   * @example
   *   BitTwiddle.bswap16(0xFF00) # => 255
   *   BitTwiddle.bswap16(0x00FF) # => 65280
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bswap16",  bt_bswap16,  1);
  /* Reverse the least-significant 4 bytes of `int`.
   *
   * Does not reverse bits within each byte. This can be used to swap endianness
   * of a 32-bit integer.
   *
   * @example
   *   BitTwiddle.bswap32(0xaabbccdd).to_s(16) # => "ddccbbaa"
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bswap32",  bt_bswap32,  1);
  /* Reverse the least-significant 8 bytes of `int`.
   *
   * Does not reverse bits within each byte. This can be used to swap endianness
   * of a 64-bit integer.
   *
   * @example
   *   BitTwiddle.bswap64(0xaabbccdd).to_s(16) # => "ddccbbaa00000000"
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bswap64",  bt_bswap64,  1);
  /* Left-rotation ("circular shift") of the low 8 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the right
   * instead.
   *
   * @example
   *   BitTwiddle.lrot8(0b01110001, 1).to_s(2) # => "11100010"
   *   BitTwiddle.lrot8(0b01110001, 3).to_s(2) # => "10001011"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lrot8",    bt_lrot8,    2);
  /* Left-rotation ("circular shift") of the low 16 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the right
   * instead.
   *
   * @example
   *   BitTwiddle.lrot16(0b0111000101110001, 1).to_s(2) # => "1110001011100010"
   *   BitTwiddle.lrot16(0b0111000101110001, 3).to_s(2) # => "1000101110001011"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lrot16",   bt_lrot16,   2);
  /* Left-rotation ("circular shift") of the low 32 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the right
   * instead.
   *
   * @example
   *   BitTwiddle.lrot32(0xaabbccdd, 4).to_s(16) # => "abbccdda"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lrot32",   bt_lrot32,   2);
  /* Left-rotation ("circular shift") of the low 64 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the right
   * instead.
   *
   * @example
   *   BitTwiddle.lrot64(0x11223344aabbccdd, 4).to_s(16) # => "1223344aabbccdd1"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lrot64",   bt_lrot64,   2);
  /* Right-rotation ("circular shift") of the low 8 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the left
   * instead.
   *
   * @example
   *   BitTwiddle.rrot8(0b01110001, 1).to_s(2) # => "10111000"
   *   BitTwiddle.rrot8(0b01110001, 3).to_s(2) # => "101110"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rrot8",    bt_rrot8,    2);
  /* Right-rotation ("circular shift") of the low 16 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the left
   * instead.
   *
   * @example
   *   BitTwiddle.rrot16(0b0111000101110001, 1).to_s(2) # => "1011100010111000"
   *   BitTwiddle.rrot16(0b0111000101110001, 3).to_s(2) # => "10111000101110"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rrot16",   bt_rrot16,   2);
  /* Right-rotation ("circular shift") of the low 32 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the left
   * instead.
   *
   * @example
   *   BitTwiddle.rrot32(0xaabbccdd, 4).to_s(16) # => "daabbccd"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rrot32",   bt_rrot32,   2);
  /* Right-rotation ("circular shift") of the low 64 bits in `int`.
   *
   * If the rotate distance is negative, the bit rotation will be to the left
   * instead.
   *
   * @example
   *   BitTwiddle.rrot64(0x11223344aabbccdd, 4).to_s(16) # => "d11223344aabbccd"
   *
   * @param int [Integer] The integer to operate on
   * @param rotdist [Integer] Number of bit positions to rotate by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rrot64",   bt_rrot64,   2);
  /* Left-shift of the low 8 bits in `int`.
   *
   * If the shift distance is negative, a right shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 7 or less than -7, the low 8 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.lshift8(0x11223344, 1).to_s(16) # => "11223388"
   *   BitTwiddle.lshift8(0x11223344, 2).to_s(16) # => "11223310"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lshift8",  bt_lshift8,  2);
  /* Left-shift of the low 16 bits in `int`.
   *
   * If the shift distance is negative, a right shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 15 or less than -15, the low 16 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.lshift16(0x11223344, 1).to_s(16) # => "11226688"
   *   BitTwiddle.lshift16(0x11223344, 2).to_s(16) # => "1122cd10"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lshift16", bt_lshift16, 2);
  /* Left-shift of the low 32 bits in `int`.
   *
   * If the shift distance is negative, a right shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 31 or less than -31, the low 32 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.lshift32(0x11223344, 1).to_s(16) # => "22446688"
   *   BitTwiddle.lshift32(0x11223344, 2).to_s(16) # => "4488cd10"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lshift32", bt_lshift32, 2);
  /* Left-shift of the low 64 bits in `int`.
   *
   * If the shift distance is negative, a right shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 63 or less than -63, the low 64 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.lshift64(0x1122334411223344, 1).to_s(16) # => "2244668822446688"
   *   BitTwiddle.lshift64(0x1122334411223344, 2).to_s(16) # => "4488cd104488cd10"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "lshift64", bt_lshift64, 2);
  /* Right-shift of the low 8 bits in `int`.
   *
   * If the shift distance is negative, a left shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 7 or less than -7, the low 8 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.rshift8(0x11223344, 1).to_s(16) # => "11223322"
   *   BitTwiddle.rshift8(0x11223344, 2).to_s(16) # => "11223311"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rshift8",  bt_rshift8,  2);
  /* Right-shift of the low 16 bits in `int`.
   *
   * If the shift distance is negative, a left shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 15 or less than -15, the low 16 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.rshift16(0x11223344, 1).to_s(16) # => "112219a2"
   *   BitTwiddle.rshift16(0x11223344, 2).to_s(16) # => "11220cd1"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rshift16", bt_rshift16, 2);
  /* Right-shift of the low 32 bits in `int`.
   *
   * If the shift distance is negative, a left shift will be performed instead.
   * The vacated bit positions will be filled with 0 bits. If shift distance is
   * more than 31 or less than -31, the low 32 bits will all be zeroed.
   *
   * @example
   *   BitTwiddle.rshift32(0x11223344, 1).to_s(16) # => "89119a2"
   *   BitTwiddle.rshift32(0x11223344, 2).to_s(16) # => "4488cd1"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rshift32", bt_rshift32, 2);
  /* Arithmetic right-shift of the low 64 bits in `int`.
   *
   * If bit 64 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift64(0xaabbccddaabbccdd, 1).to_s(16) # => "d55de66ed55de66e"
   *   BitTwiddle.arith_rshift64(0xaabbccddaabbccdd, 2).to_s(16) # => "eaaef3376aaef337"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "rshift64", bt_rshift64, 2);
  /* Arithmetic right-shift of the low 8 bits in `int`.
   *
   * If bit 8 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift8(0xaabbccdd, 1).to_s(16) # => "aabbccee"
   *   BitTwiddle.arith_rshift8(0xaabbccdd, 2).to_s(16) # => "aabbccf7"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "arith_rshift8",  bt_arith_rshift8,  2);
  /* Arithmetic right-shift of the low 16 bits in `int`.
   *
   * If bit 16 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift16(0xaabbccdd, 1).to_s(16) # => "aabbe66e"
   *   BitTwiddle.arith_rshift16(0xaabbccdd, 2).to_s(16) # => "aabbf337"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "arith_rshift16", bt_arith_rshift16, 2);
  /* Arithmetic right-shift of the low 32 bits in `int`.
   *
   * If bit 32 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift32(0xaabbccddaabbccdd, 1).to_s(16) # => "d55de66e"
   *   BitTwiddle.arith_rshift32(0xaabbccddaabbccdd, 2).to_s(16) # => "eaaef337"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "arith_rshift32", bt_arith_rshift32, 2);
  /* Arithmetic right-shift of the low 64 bits in `int`.
   *
   * If bit 64 is a 1, the vacated bit positions will be filled with 1s. Otherwise,
   * they will be filled with 0s. Or, if the shift distance is negative, a left shift
   * will be performed instead, and the vacated bit positions will be filled with 0s.
   *
   * @example
   *   BitTwiddle.arith_rshift64(0xaabbccddaabbccdd, 1).to_s(16) # => "d55de66ed55de66e"
   *   BitTwiddle.arith_rshift64(0xaabbccddaabbccdd, 2).to_s(16) # => "eaaef3376aaef337"
   *
   * @param int [Integer] The integer to operate on
   * @param shiftdist [Integer] Number of bit positions to shift by
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "arith_rshift64", bt_arith_rshift64, 2);
  /* Reverse the low 8 bits in `int`.
   *
   * @example
   *   BitTwiddle.bitreverse8(0b01101011).to_s(2) # => "11010110"
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bitreverse8",  bt_bitreverse8,  1);
  /* Reverse the low 16 bits in `int`.
   *
   * @example
   *   BitTwiddle.bitreverse16(0b0110101100001011).to_s(2) # => "1101000011010110"
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bitreverse16", bt_bitreverse16, 1);
  /* Reverse the low 32 bits in `int`.
   *
   * @example
   *   BitTwiddle.bitreverse32(0x12341234).to_s(16) # => "2c482c48"
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bitreverse32", bt_bitreverse32, 1);
  /* Reverse the low 64 bits in `int`.
   *
   * @example
   *   BitTwiddle.bitreverse64(0xabcd1234abcd1234).to_s(16) # => "2c48b3d52c48b3d5"
   *
   * @param int [Integer] The integer to operate on
   * @return [Integer]
   */
  rb_define_singleton_method(rb_mBitTwiddle, "bitreverse64", bt_bitreverse64, 1);

#if 0
  /* The following definitions are executed only on BitTwiddle.add_core_extensions
   * This is a hack for Yardoc -- so Yardoc can find them: */

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

   rb_define_method(rb_cFixnum, "lshift8",   fnum_lshift8,  1);
   rb_define_method(rb_cBignum, "lshift8",   bnum_lshift8,  1);
   rb_define_method(rb_cFixnum, "lshift16",  fnum_lshift16, 1);
   rb_define_method(rb_cBignum, "lshift16",  bnum_lshift16, 1);
   rb_define_method(rb_cFixnum, "lshift32",  fnum_lshift32, 1);
   rb_define_method(rb_cBignum, "lshift32",  bnum_lshift32, 1);
   rb_define_method(rb_cFixnum, "lshift64",  fnum_lshift64, 1);
   rb_define_method(rb_cBignum, "lshift64",  bnum_lshift64, 1);

   rb_define_method(rb_cFixnum, "rshift8",   fnum_rshift8,  1);
   rb_define_method(rb_cBignum, "rshift8",   bnum_rshift8,  1);
   rb_define_method(rb_cFixnum, "rshift16",  fnum_rshift16, 1);
   rb_define_method(rb_cBignum, "rshift16",  bnum_rshift16, 1);
   rb_define_method(rb_cFixnum, "rshift32",  fnum_rshift32, 1);
   rb_define_method(rb_cBignum, "rshift32",  bnum_rshift32, 1);
   rb_define_method(rb_cFixnum, "rshift64",  fnum_rshift64, 1);
   rb_define_method(rb_cBignum, "rshift64",  bnum_rshift64, 1);

   rb_define_method(rb_cFixnum, "arith_rshift8",  fnum_arith_rshift8,  1);
   rb_define_method(rb_cBignum, "arith_rshift8",  bnum_arith_rshift8,  1);
   rb_define_method(rb_cFixnum, "arith_rshift16", fnum_arith_rshift16, 1);
   rb_define_method(rb_cBignum, "arith_rshift16", bnum_arith_rshift16, 1);
   rb_define_method(rb_cFixnum, "arith_rshift32", fnum_arith_rshift32, 1);
   rb_define_method(rb_cBignum, "arith_rshift32", bnum_arith_rshift32, 1);
   rb_define_method(rb_cFixnum, "arith_rshift64", fnum_arith_rshift64, 1);
   rb_define_method(rb_cBignum, "arith_rshift64", bnum_arith_rshift64, 1);

   rb_define_method(rb_cFixnum, "bitreverse8",  fnum_bitreverse8,  0);
   rb_define_method(rb_cBignum, "bitreverse8",  bnum_bitreverse8,  0);
   rb_define_method(rb_cFixnum, "bitreverse16", fnum_bitreverse16, 0);
   rb_define_method(rb_cBignum, "bitreverse16", bnum_bitreverse16, 0);
   rb_define_method(rb_cFixnum, "bitreverse32", fnum_bitreverse32, 0);
   rb_define_method(rb_cBignum, "bitreverse32", bnum_bitreverse32, 0);
   rb_define_method(rb_cFixnum, "bitreverse64", fnum_bitreverse64, 0);
   rb_define_method(rb_cBignum, "bitreverse64", bnum_bitreverse64, 0);
#endif
}
