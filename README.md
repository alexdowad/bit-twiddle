#Fast popcount for Ruby

"Popcount" or "population count" refers to the number of 1 bits in a binary number. For example, the popcount of 11 (binary 1011) is 3. Typically, Ruby programmers use goofy tricks like `num.to_s(2).count("1")` to compute this quantity.

Don't bother with that nonsense. Just use the fastest native code which GCC can generate for your CPU architecture. (On x86 CPUs with SSE4, that is a single POPCNT instruction.) Like so:

```ruby
require 'popcount'
puts 11.popcount # => 3
```