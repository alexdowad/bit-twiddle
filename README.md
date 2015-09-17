#Fast bitwise operations for Ruby

Ruby has built-in implementations of the "workhorses" of bit manipulation: bitwise AND, OR, NOT, and XOR operations and bit shifts. This library adds more bitwise operations, which may be useful in implementing some algorithms. All the added operations are implemented in optimized C code (so this is MRI-only). All operations on integers are implemented on both `Fixnum` and `Bignum`. Install this goodness with:

```ruby
gem install bit-twiddle
```

And load it with:

```ruby
require "bit-twiddle"
```

Ruby does not use a fixed bit width for integers. Rather, the number of bits used for a `Fixnum` is the size of a `long` in the underlying C implementation. The bit width of a `Bignum` varies with its magnitude (typically it's a multiple of 32 bits). This raises the question of what operations which are dependent on bit width, like reversing bits in an integer, should do. Should they return results which vary depending on the bit width used by the underlying, platform-dependent integer representation? That would cause all kinds of headaches.

Instead, `bit-twiddle` defines operations which explicitly work on the low 8, low 16, low 32, or low 64 bits of an integer. For example, it defines `#bitreverse8`, `#bitreverse16`, `#bitreverse32`, and `#bitreverse64` methods. If an integer's bit width is larger than the number of bits operated on, the higher-end bits are passed through unchanged. All these methods automatically convert between `Fixnum` and `Bignum` as is appropriate to represent their result.

Here is what it offers:

### Population count

"Popcount" or "population count" refers to the number of 1 bits in a binary number. For example, the popcount of 11 (binary 1011) is 3. Typically, Ruby programmers use goofy tricks like `num.to_s(2).count("1")` to compute this quantity. This is much faster, and doesn't needlessly allocate memory:

```
7.popcount   # => 3
255.popcount # => 255
```
