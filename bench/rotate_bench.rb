require 'bit-twiddle/core_ext'

MASK_32 = (1 << 32) - 1
MASK_62 = (1 << 62) - 1
dwords = 1000.times.collect { |n| n.hash & MASK_32 }
qwords = 1000.times.collect { |n| n.hash & MASK_62 }
bnums  = 1000.times.collect { |n| n.hash << 16 }

raise "No Bignums!" if qwords.any? { |q| q.is_a?(Bignum) }
raise "No Fixnums!" if bnums.any? { |b| b.is_a?(Fixnum)}

Benchmark.ips do |b|
  b.report "#rrot8 on Fixnum (x1000)" do |n|
    n.times { dwords.each { |x| x.rrot8(16) }}
  end
  b.report "#rrot16 on Fixnum (x1000)" do |n|
    n.times { dwords.each { |x| x.rrot16(16) }}
  end
  b.report "#rrot32 on Fixnum (x1000)" do |n|
    n.times { dwords.each { |x| x.rrot32(16) }}
  end
  b.report "#rrot64 on Fixnum (x1000)" do |n|
    n.times { dwords.each { |x| x.rrot64(16) }}
  end

  b.report "#lrot8 on Fixnum (x1000)" do |n|
    n.times { dwords.each { |x| x.lrot8(16) }}
  end
  b.report "#lrot16 on Fixnum (x1000)" do |n|
    n.times { dwords.each { |x| x.lrot16(16) }}
  end
  b.report "#lrot32 on Fixnum (x1000)" do |n|
    n.times { dwords.each { |x| x.lrot32(16) }}
  end
  b.report "#lrot64 on Fixnum (x1000)" do |n|
    n.times { dwords.each { |x| x.lrot64(16) }}
  end
end