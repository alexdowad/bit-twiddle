#Fast bitwise operations for Ruby

Ruby has built-in implementations of the "workhorses" of bit manipulation: bitwise AND, OR, NOT, and XOR operations and bit shifts. This library adds more bitwise operations, which may be useful in implementing some algorithms. All the added operations are implemented in optimized C code. All operations on integers are implemented on both `Fixnum` and `Bignum`. Install this goodness with:

```ruby
gem install bit-twiddle
```

Here is what it offers:

### Population count

"Popcount" or "population count" refers to the number of 1 bits in a binary number. For example, the popcount of 11 (binary 1011) is 3. Typically, Ruby programmers use goofy tricks like `num.to_s(2).count("1")` to compute this quantity. Don't bother with that nonsense. Just use the fastest native code which GCC can generate for your CPU architecture. (On x86 CPUs with SSE4, that is a single POPCNT instruction.) Like so:

```
7.popcount   # => 3
255.popcount # => 255
```
