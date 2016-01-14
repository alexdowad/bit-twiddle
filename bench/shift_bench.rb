MASK_30 = (1 << 30) - 1
MASK_62 = (1 << 62) - 1
dwords = 1000.times.collect { |n| n.hash & MASK_30 }
qwords = 1000.times.collect { |n| n.hash & MASK_62 }
bnums  = 1000.times.collect { |n| n.hash << 16 }

raise "No Bignums!" if qwords.any? { |q| q.is_a?(Bignum) }
raise "No Fixnums!" if bnums.any? { |b| b.is_a?(Fixnum)}

Benchmark.ips do |b|
  b.report "#lshift32 on 32-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.lshift32(16) }}
  end
  # b.report "#<< on 32-bit Fixnum, small shift (x1000)" do |n|
  #   n.times { dwords.each { |x| x << 16 }}
  # end
  b.report "#lshift32 on 32-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.lshift32(32) }}
  end
  # b.report "#<< on 32-bit Fixnum, large shift (x1000)" do |n|
  #   n.times { dwords.each { |x| x << 32 }}
  # end

  b.report "#rshift32 on 32-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.rshift32(16) }}
  end
  # b.report "#>> on 32-bit Fixnum, small shift (x1000)" do |n|
  #   n.times { dwords.each { |x| x >> 16 }}
  # end
  b.report "#rshift32 on 32-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.rshift32(32) }}
  end
  # b.report "#>> on 32-bit Fixnum, large shift (x1000)" do |n|
  #   n.times { dwords.each { |x| x >> 32 }}
  # end

  b.report "#arith_rshift32 on 32-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.arith_rshift32(16) }}
  end
  b.report "#arith_rshift32 on 32-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.arith_rshift32(32) }}
  end

  b.report "#lshift64 on 64-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.lshift64(32) }}
  end
  b.report "#lshift64 on 64-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.lshift64(64) }}
  end
  b.report "#rshift64 on 64-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.rshift64(32) }}
  end
  b.report "#rshift64 on 64-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.rshift64(64) }}
  end
  b.report "#arith_rshift64 on 64-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.arith_rshift64(32) }}
  end
  b.report "#arith_rshift64 on 64-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.arith_rshift64(64) }}
  end

  b.report "#lshift64 on Bignum, small shift (x1000)" do |n|
    n.times { bnums.each { |x| x.lshift64(32) }}
  end
  b.report "#lshift64 on Bignum, large shift (x1000)" do |n|
    n.times { bnums.each { |x| x.lshift64(64) }}
  end
  b.report "#rshift64 on Bignum, small shift (x1000)" do |n|
    n.times { bnums.each { |x| x.rshift64(32) }}
  end
  b.report "#rshift64 on Bignum, large shift (x1000)" do |n|
    n.times { bnums.each { |x| x.rshift64(64) }}
  end
  b.report "#arith_rshift64 on Bignum, small shift (x1000)" do |n|
    n.times { bnums.each { |x| x.arith_rshift64(32) }}
  end
  b.report "#arith_rshift64 on Bignum, large shift (x1000)" do |n|
    n.times { bnums.each { |x| x.arith_rshift64(64) }}
  end
end