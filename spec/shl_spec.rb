describe "#lshift8" do
  it "shifts bits in a 8-bit number to the left (but cuts off high bits)" do
    100.times do
      num = rand(1 << 8)
      1.upto(20) do |sdist|
        expect(num.lshift8(sdist)).to eq ((num << sdist) & MASK_8)
      end
    end
  end

  it "does a right shift if shift distance is negative" do
    100.times do
      num = rand(1 << 8)
      1.upto(20) do |sdist|
        expect(num.lshift8(-sdist)).to eq (num >> sdist)
      end
    end
  end

  it "returns the receiver if shift distance is zero" do
    100.times do
      num = rand(1 << 32)
      expect(num.lshift8(0)).to eq num
    end
  end

  it "fills in the low end with zeros" do
    100.times do
      num = rand(1 << 32)
      1.upto(20) do |sdist|
        mask = sdist <= 8 ? (1 << sdist) - 1 : MASK_8
        expect(num.lshift8(sdist) & mask).to eq 0
      end
    end
  end

  it "doesn't modify bits above the 32nd" do
    100.times do
      num = rand(1 << 64)
      -64.upto(64) do |sdist|
        expect(num.lshift8(sdist) & ~MASK_8).to eq (num & ~MASK_8)
      end
    end
  end

  it "zeroes out low 32 bits when shift distance is a Bignum" do
    expect(100.lshift8(1 << 100)).to eq 0
    expect((1 << 100).lshift8(1 << 100)).to eq (1 << 100)
  end
end


describe "#lshift32" do
  it "shifts bits in a 32-bit number to the left (but cuts off high bits)" do
    100.times do
      num = rand(1 << 32)
      1.upto(40) do |sdist|
        expect(num.lshift32(sdist)).to eq ((num << sdist) & MASK_32)
      end
    end
  end

  it "does a right shift if shift distance is negative" do
    100.times do
      num = rand(1 << 32)
      1.upto(40) do |sdist|
        expect(num.lshift32(-sdist)).to eq (num >> sdist)
      end
    end
  end

  it "returns the receiver if shift distance is zero" do
    100.times do
      num = rand(1 << 32)
      expect(num.lshift32(0)).to eq num
    end
  end

  it "fills in the low end with zeros" do
    100.times do
      num = rand(1 << 32)
      1.upto(40) do |sdist|
        mask = sdist <= 32 ? (1 << sdist) - 1 : MASK_32
        expect(num.lshift32(sdist) & mask).to eq 0
      end
    end
  end

  it "doesn't modify bits above the 32nd" do
    100.times do
      num = rand(1 << 64)
      0.upto(64) do |sdist|
        expect(num.lshift32(sdist) & MASK_32).to eq ((num << sdist) & MASK_32)
        expect(num.lshift32(sdist) & ~MASK_32).to eq (num & ~MASK_32)
      end
    end
  end

  it "zeroes out low 32 bits when shift distance is a Bignum" do
    expect(100.lshift32(1 << 100)).to eq 0
    expect((1 << 100).lshift32(1 << 100)).to eq (1 << 100)
    (expect ((1 << 100)+(1 << 31)+1).lshift32(1 << 80)).to eq (1 << 100)
  end
end

describe "#lshift64" do
  it "shifts bits in a 64-bit number to the left (but cuts off high bits)" do
    100.times do
      num = rand(1 << 64)
      1.upto(80) do |sdist|
        expect(num.lshift64(sdist)).to eq ((num << sdist) & MASK_64)
      end
    end
  end

  it "does a right shift if shift distance is negative" do
    100.times do
      num = rand(1 << 64)
      1.upto(80) do |sdist|
        expect(num.lshift64(-sdist)).to eq (num >> sdist)
      end
    end
  end

  it "returns the receiver if shift distance is zero" do
    100.times do
      num = rand(1 << 64)
      expect(num.lshift64(0)).to eq num
    end
  end

  it "fills in the low end with zeros" do
    100.times do
      num = rand(1 << 64)
      1.upto(80) do |sdist|
        mask = sdist <= 64 ? (1 << sdist) - 1 : MASK_64
        expect(num.lshift64(sdist) & mask).to eq 0
      end
    end
  end

  it "doesn't modify bits above the 64th" do
    100.times do
      num = rand(1 << 100)
      0.upto(80) do |sdist|
        expect(num.lshift64(sdist) & MASK_64).to eq ((num << sdist) & MASK_64)
        expect(num.lshift64(sdist) & ~MASK_64).to eq (num & ~MASK_64)
      end
    end
  end

  it "zeroes out low 64 bits when shift distance is a Bignum" do
    expect(100.lshift64(1 << 100)).to eq 0
    expect((1 << 100).lshift64(1 << 100)).to eq (1 << 100)
    (expect ((1 << 100)+(1 << 63)+1).lshift64(1 << 80)).to eq (1 << 100)
  end
end