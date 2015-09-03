describe "#rrot8" do
  it "rotates the low 8 bits to the right" do
    100.times do
      num  = rand(1 << 8)
      bnum = rand(1 << 90)
      1.upto(7) do |rdist|
        mask = ~((1 << rdist) - 1) & MASK_8
        expect(num.rrot8(rdist)).to eq (((num & mask) >> rdist) | ((num & ~mask) << (8 - rdist)))
        expect(bnum.rrot8(rdist)).to eq (((bnum & mask) >> rdist) | (((bnum & ~mask) & MASK_8) << (8 - rdist)) | (bnum & ~0xFF))
      end
    end
  end

  it "doesn't modify the higher bits" do
    100.times do
      num  = rand(1 << 32)
      bnum = rand(1 << 90)
      1.upto(30) do |rdist|
        expect(num.rrot8(rdist) & ~MASK_8).to eq (num & ~MASK_8)
        expect(bnum.rrot8(rdist) & ~MASK_8).to eq (bnum & ~MASK_8)
      end
    end
  end

  context "with a negative rotate distance" do
    it "rotates the low 8 bits to the left" do
      100.times do
        num  = rand(1 << 8)
        bnum = rand(1 << 90)
        1.upto(7) do |rdist|
          mask = (1 << (8 - rdist)) - 1
          expect(num.rrot8(-rdist)).to eq (((num & mask) << rdist) | ((num & ~mask) >> (8 - rdist)))
          expect(bnum.rrot8(-rdist)).to eq (((bnum & mask) << rdist) | (((bnum & ~mask) & MASK_8) >> (8 - rdist)) | (bnum & ~0xFF))
        end
      end
    end
  end

  context "with a rotate distance greater than 7" do
    it "does the same as (rotate distance) % 8" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 90)
        8.upto(100) do |n|
          expect(num.rrot8(n)).to eq num.rrot8(n % 8)
          expect(bnum.rrot8(n)).to eq bnum.rrot8(n % 8)
        end
      end
    end
  end

  context "with a Bignum as rotate distance" do
    it "still works the same" do
      100.times do
        num   = rand(1 << 32)
        bnum  = rand(1 << 90)
        dist = rand(1 << 90)
        0.upto(20) do |offset|
          rdist = dist + offset
          expect(num.rrot8(rdist)).to eq num.rrot8(rdist % 8)
          expect(bnum.rrot8(rdist)).to eq bnum.rrot8(rdist % 8)
          expect(num.rrot8(rdist) & ~MASK_8).to eq (num & ~MASK_8)
          expect(bnum.rrot8(rdist) & ~MASK_8).to eq (bnum & ~MASK_8)
        end
      end
    end
  end

  context "with a 0 as rotate distance" do
    it "returns the receiver" do
      expect(100.rrot8(0)).to eq 100
      expect((111 << 80).rrot8(0)).to eq (111 << 80)
    end
  end
end

describe "#rrot16" do
  it "rotates the low 16 bits to the right" do
    100.times do
      num  = rand(1 << 16)
      bnum = rand(1 << 90)
      1.upto(15) do |rdist|
        mask = ~((1 << rdist) - 1) & MASK_16
        expect(num.rrot16(rdist)).to eq (((num & mask) >> rdist) | ((num & ~mask) << (16 - rdist)))
        expect(bnum.rrot16(rdist)).to eq (((bnum & mask) >> rdist) | (((bnum & ~mask) & MASK_16) << (16 - rdist)) | (bnum & ~0xFFFF))
      end
    end
  end

  it "doesn't modify the higher bits" do
    100.times do
      num  = rand(1 << 32)
      bnum = rand(1 << 90)
      1.upto(30) do |rdist|
        expect(num.rrot16(rdist) & ~MASK_16).to eq (num & ~MASK_16)
        expect(bnum.rrot16(rdist) & ~MASK_16).to eq (bnum & ~MASK_16)
      end
    end
  end

  context "with a negative rotate distance" do
    it "rotates the low 16 bits to the left" do
      100.times do
        num  = rand(1 << 16)
        bnum = rand(1 << 90)
        1.upto(15) do |rdist|
          mask = (1 << (16 - rdist)) - 1
          expect(num.rrot16(-rdist)).to eq (((num & mask) << rdist) | ((num & ~mask) >> (16 - rdist)))
          expect(bnum.rrot16(-rdist)).to eq (((bnum & mask) << rdist) | (((bnum & ~mask) & MASK_16) >> (16 - rdist)) | (bnum & ~0xFFFF))
        end
      end
    end
  end

  context "with a rotate distance greater than 15" do
    it "does the same as (rotate distance) % 16" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 90)
        16.upto(100) do |n|
          expect(num.rrot16(n)).to eq num.rrot16(n % 16)
          expect(bnum.rrot16(n)).to eq bnum.rrot16(n % 16)
        end
      end
    end
  end

  context "with a Bignum as rotate distance" do
    it "still works the same" do
      100.times do
        num   = rand(1 << 32)
        bnum  = rand(1 << 90)
        dist = rand(1 << 90)
        0.upto(20) do |offset|
          rdist = dist + offset
          expect(num.rrot16(rdist)).to eq num.rrot16(rdist % 16)
          expect(bnum.rrot16(rdist)).to eq bnum.rrot16(rdist % 16)
          expect(num.rrot16(rdist) & ~MASK_16).to eq (num & ~MASK_16)
          expect(bnum.rrot16(rdist) & ~MASK_16).to eq (bnum & ~MASK_16)
        end
      end
    end
  end

  context "with a 0 as rotate distance" do
    it "returns the receiver" do
      expect(100.rrot16(0)).to eq 100
      expect((111 << 80).rrot16(0)).to eq (111 << 80)
    end
  end
end

describe "#rrot32" do
  it "rotates the low 32 bits to the right" do
    100.times do
      num  = rand(1 << 32)
      bnum = rand(1 << 90)
      1.upto(31) do |rdist|
        mask = ~((1 << rdist) - 1) & MASK_32
        expect(num.rrot32(rdist)).to eq (((num & mask) >> rdist) | ((num & ~mask) << (32 - rdist)))
        expect(bnum.rrot32(rdist)).to eq (((bnum & mask) >> rdist) | (((bnum & ~mask) & MASK_32) << (32 - rdist)) | (bnum & ~MASK_32))
      end
    end
  end

  it "doesn't modify the higher bits" do
    100.times do
      num  = rand(1 << 32)
      bnum = rand(1 << 90)
      1.upto(50) do |rdist|
        expect(num.rrot32(rdist) & ~MASK_32).to eq (num & ~MASK_32)
        expect(bnum.rrot32(rdist) & ~MASK_32).to eq (bnum & ~MASK_32)
      end
    end
  end

  context "with a negative rotate distance" do
    it "rotates the low 32 bits to the left" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 90)
        1.upto(31) do |rdist|
          mask = (1 << (32 - rdist)) - 1
          expect(num.rrot32(-rdist)).to eq (((num & mask) << rdist) | ((num & ~mask) >> (32 - rdist)))
          expect(bnum.rrot32(-rdist)).to eq (((bnum & mask) << rdist) | (((bnum & ~mask) & MASK_32) >> (32 - rdist)) | (bnum & ~MASK_32))
        end
      end
    end
  end

  context "with a rotate distance greater than 31" do
    it "does the same as (rotate distance) % 32" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 90)
        32.upto(100) do |n|
          expect(num.rrot32(n)).to eq num.rrot32(n % 32)
          expect(bnum.rrot32(n)).to eq bnum.rrot32(n % 32)
        end
      end
    end
  end

  context "with a Bignum as rotate distance" do
    it "still works the same" do
      100.times do
        num   = rand(1 << 32)
        bnum  = rand(1 << 90)
        dist = rand(1 << 90)
        0.upto(20) do |offset|
          rdist = dist + offset
          expect(num.rrot32(rdist)).to eq num.rrot32(rdist % 32)
          expect(bnum.rrot32(rdist)).to eq bnum.rrot32(rdist % 32)
          expect(num.rrot32(rdist) & ~MASK_32).to eq (num & ~MASK_32)
          expect(bnum.rrot32(rdist) & ~MASK_32).to eq (bnum & ~MASK_32)
        end
      end
    end
  end

  context "with a 0 as rotate distance" do
    it "returns the receiver" do
      expect(100.rrot32(0)).to eq 100
      expect((111 << 80).rrot32(0)).to eq (111 << 80)
    end
  end
end

describe "#rrot64" do
  it "rotates the low 64 bits to the right" do
    100.times do
      num  = rand(1 << 64)
      bnum = rand(1 << 90)
      1.upto(63) do |rdist|
        mask = ~((1 << rdist) - 1) & MASK_64
        expect(num.rrot64(rdist)).to eq (((num & mask) >> rdist) | ((num & ~mask) << (64 - rdist)))
        expect(bnum.rrot64(rdist)).to eq (((bnum & mask) >> rdist) | (((bnum & ~mask) & MASK_64) << (64 - rdist)) | (bnum & ~MASK_64))
      end
    end
  end

  it "doesn't modify the higher bits" do
    100.times do
      num  = rand(1 << 32)
      bnum = rand(1 << 90)
      1.upto(80) do |rdist|
        expect(num.rrot64(rdist) & ~MASK_64).to eq (num & ~MASK_64)
        expect(bnum.rrot64(rdist) & ~MASK_64).to eq (bnum & ~MASK_64)
      end
    end
  end

  context "with a negative rotate distance" do
    it "rotates the low 64 bits to the left" do
      100.times do
        num  = rand(1 << 64)
        bnum = rand(1 << 90)
        1.upto(63) do |rdist|
          mask = (1 << (64 - rdist)) - 1
          expect(num.rrot64(-rdist)).to eq (((num & mask) << rdist) | ((num & ~mask) >> (64 - rdist)))
          expect(bnum.rrot64(-rdist)).to eq (((bnum & mask) << rdist) | (((bnum & ~mask) & MASK_64) >> (64 - rdist)) | (bnum & ~MASK_64))
        end
      end
    end
  end

  context "with a rotate distance greater than 63" do
    it "does the same as (rotate distance) % 64" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 90)
        64.upto(100) do |n|
          expect(num.rrot64(n)).to eq num.rrot64(n % 64)
          expect(bnum.rrot64(n)).to eq bnum.rrot64(n % 64)
        end
      end
    end
  end

  context "with a Bignum as rotate distance" do
    it "still works the same" do
      100.times do
        num   = rand(1 << 32)
        bnum  = rand(1 << 90)
        dist = rand(1 << 90)
        0.upto(20) do |offset|
          rdist = dist + offset
          expect(num.rrot64(rdist)).to eq num.rrot64(rdist % 64)
          expect(bnum.rrot64(rdist)).to eq bnum.rrot64(rdist % 64)
          expect(num.rrot64(rdist) & ~MASK_64).to eq (num & ~MASK_64)
          expect(bnum.rrot64(rdist) & ~MASK_64).to eq (bnum & ~MASK_64)
        end
      end
    end
  end

  context "with a 0 as rotate distance" do
    it "returns the receiver" do
      expect(100.rrot64(0)).to eq 100
      expect((111 << 80).rrot64(0)).to eq (111 << 80)
    end
  end
end