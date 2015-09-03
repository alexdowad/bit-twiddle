describe "#sar32" do
  it "shifts bits in a 32-bit number to the right" do
    100.times do
      num = rand(1 << 32)
      1.upto(40) do |sdist|
        mask = sdist <= 32 ? ~((1 << (32 - sdist)) - 1) : MASK_32
        if num & 0x80000000 == 0
          expect(num.sar32(sdist)).to eq (num >> sdist)
        else
          expect(num.sar32(sdist)).to eq ((num >> sdist) | mask) & MASK_32
        end
      end
    end
  end

  it "does a left shift if shift distance is negative (but cuts off high bits)" do
    100.times do
      num = rand(1 << 32)
      1.upto(40) do |sdist|
        expect(num.sar32(-sdist)).to eq ((num << sdist) & MASK_32)
      end
    end
  end

  it "returns the receiver if shift distance is zero" do
    100.times do
      num = rand(1 << 32)
      expect(num.sar32(0)).to eq num
    end
  end

  it "fills in the high end with zeroes if the high bit was 0" do
    100.times do
      num = rand(1 << 32)
      num &= ~0x80000000 # turn high bit off
      1.upto(40) do |sdist|
        mask = sdist <= 32 ? ~((1 << (32 - sdist)) - 1) : MASK_32
        expect(num.sar32(sdist) & mask).to eq 0
      end
    end
  end

  it "fills in the high end with ones if the high bit was 1" do
    100.times do
      num = rand(1 << 32)
      num |= 0x80000000 # turn high bit on
      1.upto(40) do |sdist|
        mask = sdist <= 32 ? ~((1 << (32 - sdist)) - 1) : MASK_32
        expect(num.sar32(sdist) & mask).to eq (mask & MASK_32)
      end
    end
  end

  it "doesn't modify bits above the 32nd if high bit is 0" do
    100.times do
      num = rand(1 << 64)
      num &= ~0x80000000
      0.upto(64) do |sdist|
        expect(num.sar32(sdist) & MASK_32).to eq ((num & MASK_32) >> sdist)
        expect(num.sar32(sdist) & ~MASK_32).to eq (num & ~MASK_32)
      end
    end
  end

  it "zeroes out low 32 bits when shift distance is a Bignum and high bit is 0" do
    expect(100.sar32(1 << 100)).to eq 0
    expect((1 << 100).sar32(1 << 100)).to eq (1 << 100)
    (expect ((1 << 100)+1).sar32(1 << 80)).to eq (1 << 100)
  end
end

describe "#sar64" do
  it "shifts bits in a 64-bit number to the right" do
    100.times do
      num = rand(1 << 64)
      bnum = rand(1 << 100)
      1.upto(80) do |sdist|
        mask = sdist <= 64 ? ~((1 << (64 - sdist)) - 1) : MASK_64
        if 0x8000000000000000 & num == 0
          expect(num.sar64(sdist)).to eq (num >> sdist)
        else
          expect(num.sar64(sdist)).to eq ((num >> sdist) | mask) & MASK_64
        end
        if 0x8000000000000000 & bnum == 0
          expect(bnum.sar64(sdist)).to eq ((bnum & MASK_64) >> sdist) | (bnum & ~MASK_64)
        else
          expect(bnum.sar64(sdist)).to eq ((((bnum & MASK_64) >> sdist) | mask) & MASK_64)  | (bnum & ~MASK_64)
        end
      end
    end
  end

  it "does a left shift if shift distance is negative (but cuts off high bits)" do
    100.times do
      num = rand(1 << 64)
      1.upto(80) do |sdist|
        expect(num.sar64(-sdist)).to eq ((num << sdist) & MASK_64)
      end
    end
  end

  it "returns the receiver if shift distance is zero" do
    100.times do
      num = rand(1 << 64)
      expect(num.sar64(0)).to eq num
    end
  end

  it "fills in the high end with zeros if high bit is 0" do
    100.times do
      num = rand(1 << 64)
      num &= ~0x8000000000000000 # turn high bit off
      1.upto(80) do |sdist|
        mask = sdist <= 64 ? ~((1 << (64 - sdist)) - 1) : MASK_64
        expect(num.sar64(sdist) & mask).to eq 0
      end
    end
  end

  it "fills in the high end with ones if the high bit was 1" do
    100.times do
      num = rand(1 << 64)
      num |= 0x8000000000000000 # turn high bit on
      1.upto(80) do |sdist|
        mask = sdist <= 64 ? ~((1 << (64 - sdist)) - 1) : MASK_64
        expect(num.sar64(sdist) & mask).to eq (mask & MASK_64)
      end
    end
  end

  it "doesn't modify bits above the 64th if high bit is 0" do
    100.times do
      num = rand(1 << 100)
      num &= ~0x8000000000000000 # turn high bit off
      0.upto(100) do |sdist|
        expect(num.sar64(sdist) & MASK_64).to eq ((num & MASK_64) >> sdist)
        expect(num.sar64(sdist) & ~MASK_64).to eq (num & ~MASK_64)
      end
    end
  end

  it "zeroes out low 64 bits when shift distance is a Bignum" do
    expect(100.sar64(1 << 100)).to eq 0
    expect((1 << 100).sar64(1 << 100)).to eq (1 << 100)
    (expect ((1 << 100)+1).sar64(1 << 80)).to eq (1 << 100)
  end
end