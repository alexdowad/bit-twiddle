describe "#shr32" do
  it "shifts bits in a 32-bit number to the left (but cuts off high bits)" do
    100.times do
      num = rand(1 << 32)
      1.upto(40) do |sdist|
        expect(num.shl32(sdist)).to eq ((num << sdist) & 0xFFFFFFFF)
      end
    end
  end

  it "does a right shift if shift distance is negative" do
    100.times do
      num = rand(1 << 32)
      1.upto(40) do |sdist|
        expect(num.shl32(-sdist)).to eq (num >> sdist)
      end
    end
  end

  it "returns the receiver if shift distance is zero" do
    100.times do
      num = rand(1 << 32)
      expect(num.shl32(0)).to eq num
    end
  end

  it "fills in the low end with zeros" do
    100.times do
      num = rand(1 << 32)
      1.upto(40) do |sdist|
        mask = sdist <= 32 ? (1 << sdist) - 1 : 0xFFFFFFFF
        expect(num.shl32(sdist) & mask).to eq 0
      end
    end
  end

  it "doesn't modify bits above the 32nd" do
    100.times do
      num = rand(1 << 64)
      0.upto(64) do |sdist|
        expect(num.shl32(sdist) & 0xFFFFFFFF).to eq ((num << sdist) & 0xFFFFFFFF)
        expect(num.shl32(sdist) & ~0xFFFFFFFF).to eq (num & ~0xFFFFFFFF)
      end
    end
  end

  it "zeroes out low 32 bits when shift distance is a Bignum" do
    expect(100.shl32(1 << 100)).to eq 0
    expect((1 << 100).shl32(1 << 100)).to eq (1 << 100)
    (expect ((1 << 100)+1).shl32(1 << 80)).to eq (1 << 100)
  end
end