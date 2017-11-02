Fast bitwise operations for Ruby
================================

Ruby has built-in implementations of the "workhorses" of bit manipulation: bitwise AND, OR, NOT, and XOR operations and bit shifts. This library adds more bitwise operations, which may be useful in implementing some algorithms. All the added operations are implemented in optimized C code (so this is MRI-only).

Install this goodness with:

```ruby
gem install bit-twiddle
```

If you want all operations to be namespaced under the `BitTwiddle` module, load it with:

```ruby
require "bit-twiddle"
```

Or for all operations to be defined as instance methods on `Fixnum` and `Bignum`:

```ruby
require "bit-twiddle/core_ext"
```

In many cases, `bit-twiddle` operations explicitly work on the low 8, 16, 32, or 64 bits of an integer. (For example, it defines `#bitreverse8`, `#bitreverse16`, `#bitreverse32`, and `#bitreverse64` methods.) If an integer's bit width is larger than the number of bits operated on, the higher-end bits are passed through unchanged.

## Examples

### Population count

"Popcount" or "population count" refers to the number of 1 bits in a binary number. For example, the popcount of decimal 11 (binary 1011) is 3.

Typically, Ruby programmers use goofy tricks like `num.to_s(2).count("1")` to compute this quantity. This is much faster, and doesn't needlessly allocate memory:

```ruby
7.popcount   # => 3
255.popcount # => 255
```

### Highest/lowest set bit

```ruby
8.hi_bit   # => 4
255.hi_bit # => 8
8.lo_bit   # => 4
255.lo_bit # => 1
```

### Rotating bits

```ruby
0b10010011.rrot8(1).to_s(2).rjust(8,'0') # => "11001001"
0b10010011.rrot8(2).to_s(2).rjust(8,'0') # => "11100100"
0b10010011.rrot8(3).to_s(2).rjust(8,'0') # => "01110010"
0b10010011.rrot8(4).to_s(2).rjust(8,'0') # => "00111001"
0b10010011.rrot8(5).to_s(2).rjust(8,'0') # => "10011100"
0b10010011.rrot8(6).to_s(2).rjust(8,'0') # => "01001110"

0b10010011.lrot8(1).to_s(2).rjust(8,'0') # => "00100111"
0b10010011.lrot8(2).to_s(2).rjust(8,'0') # => "01001110"
0b10010011.lrot8(3).to_s(2).rjust(8,'0') # => "10011100"
0b10010011.lrot8(4).to_s(2).rjust(8,'0') # => "00111001"
0b10010011.lrot8(5).to_s(2).rjust(8,'0') # => "01110010"
0b10010011.lrot8(6).to_s(2).rjust(8,'0') # => "11100100"
```

8/16/32/64 bit variants are available.

### Reversing bytes

```ruby
0x11223344.bswap16.to_s(16) # => "11224433"
0x11223344.bswap32.to_s(16) # => "44332211"
0x11223344.bswap64.to_s(16) # => "4433221100000000"
```

### Reversing bits

```ruby
0b10010011.bitreverse8.to_s(2) # => "11001001"
```

8/16/32/64 bit variants are available.

### "Arithmetic" right bitshift

An arithmetic right shift fills the vacated bit positions with copies of the most-significant (or "sign") bit. In contrast, Ruby's `Integer#>>` is a "logical" right shift -- it fills the vacated bit positions with zeroes.

```ruby
0b10001111.arith_rshift8(3).to_s(2).rjust(8,'0') # => "11110001"
0b00001111.arith_rshift8(3).to_s(2).rjust(8,'0') # => "00000001"
```

8/16/32/64 bit variants are available.

### Logical left and right bitshifts

Ruby already provides `Integer#<<` and `#>>`, which perform logical left and right bitshifts, so these are less useful than the other `BitTwiddle` methods. Probably the only reason to use them is if you want to explicitly operate on the low 8/16/32/64 bits:

```ruby
0b10001111.rshift8(3).to_s(2).rjust(8,'0') # => "01000111"
0b10001111.lshift8(2).to_s(2).rjust(8,'0') # => "00111100"
```

8/16/32/64 bit variants are available.

## Detailed documentation

Clone yourself up a copy of this repo, then generate some local HTML documentation (with examples for each and every method):

```
git clone https://github.com/alexdowad/bit-twiddle.git
cd bit-twiddle
bundle install
rake yard
```