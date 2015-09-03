MASK_32 = (1 << 32) - 1
MASK_62 = (1 << 62) - 1
dwords = 1000.times.collect { |n| n.hash & MASK_32 }
qwords = 1000.times.collect { |n| n.hash & MASK_62 }
bnums  = 1000.times.collect { |n| n.hash << 16 }

raise "No Bignums!" if qwords.any? { |q| q.is_a?(Bignum) }
raise "No Fixnums!" if bnums.any? { |b| b.is_a?(Fixnum)}

Benchmark.ips do |b|
  b.report "#shl32 on 32-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.shl32(16) }}
  end
  # b.report "#<< on 32-bit Fixnum, small shift (x1000)" do |n|
  #   n.times { dwords.each { |x| x << 16 }}
  # end
  b.report "#shl32 on 32-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.shl32(32) }}
  end
  # b.report "#<< on 32-bit Fixnum, large shift (x1000)" do |n|
  #   n.times { dwords.each { |x| x << 32 }}
  # end

  b.report "#shr32 on 32-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.shr32(16) }}
  end
  # b.report "#>> on 32-bit Fixnum, small shift (x1000)" do |n|
  #   n.times { dwords.each { |x| x >> 16 }}
  # end
  b.report "#shr32 on 32-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.shr32(32) }}
  end
  # b.report "#>> on 32-bit Fixnum, large shift (x1000)" do |n|
  #   n.times { dwords.each { |x| x >> 32 }}
  # end

  b.report "#sar32 on 32-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.sar32(16) }}
  end
  b.report "#sar32 on 32-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.sar32(32) }}
  end

  b.report "#shl64 on 64-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.shl64(32) }}
  end
  b.report "#shl64 on 64-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.shl64(64) }}
  end
  b.report "#shr64 on 64-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.shr64(32) }}
  end
  b.report "#shr64 on 64-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.shr64(64) }}
  end
  b.report "#sar64 on 64-bit Fixnum, small shift (x1000)" do |n|
    n.times { dwords.each { |x| x.sar64(32) }}
  end
  b.report "#sar64 on 64-bit Fixnum, large shift (x1000)" do |n|
    n.times { dwords.each { |x| x.sar64(64) }}
  end

  b.report "#shl64 on Bignum, small shift (x1000)" do |n|
    n.times { bnums.each { |x| x.shl64(32) }}
  end
  b.report "#shl64 on Bignum, large shift (x1000)" do |n|
    n.times { bnums.each { |x| x.shl64(64) }}
  end
  b.report "#shr64 on Bignum, small shift (x1000)" do |n|
    n.times { bnums.each { |x| x.shr64(32) }}
  end
  b.report "#shr64 on Bignum, large shift (x1000)" do |n|
    n.times { bnums.each { |x| x.shr64(64) }}
  end
  b.report "#sar64 on Bignum, small shift (x1000)" do |n|
    n.times { bnums.each { |x| x.sar64(32) }}
  end
  b.report "#sar64 on Bignum, large shift (x1000)" do |n|
    n.times { bnums.each { |x| x.sar64(64) }}
  end
end