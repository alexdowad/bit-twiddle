MASK_64 = (1 << 64) - 1
MASK_32 = (1 << 32) - 1
MASK_16 = (1 << 16) - 1
MASK_8  = (1 << 8)  - 1

describe "#lrot8" do
  it "rotates the low 8 bits to the left" do
    100.times do
      num  = rand(1 << 8)
      bnum = rand(1 << 90)
      1.upto(7) do |rdist|
        mask = (1 << (8 - rdist)) - 1
        expect(num.lrot8(rdist)).to eq (((num & mask) << rdist) | ((num & ~mask) >> (8 - rdist)))
        expect(bnum.lrot8(rdist)).to eq (((bnum & mask) << rdist) | (((bnum & ~mask) & MASK_8) >> (8 - rdist)) | (bnum & ~0xFF))
      end
    end
  end

  it "doesn't modify the higher bits" do
    100.times do
      num  = rand(1 << 32)
      bnum = rand(1 << 90)
      1.upto(30) do |rdist|
        expect(num.lrot8(rdist) & ~MASK_8).to eq (num & ~MASK_8)
        expect(bnum.lrot8(rdist) & ~MASK_8).to eq (bnum & ~MASK_8)
      end
    end
  end

  context "with a negative rotate distance" do
    it "rotates the low 8 bits to the right" do
      100.times do
        num  = rand(1 << 8)
        bnum = rand(1 << 90)
        1.upto(7) do |rdist|
          mask = ~((1 << rdist) - 1) & MASK_8
          expect(num.lrot8(-rdist)).to eq (((num & mask) >> rdist) | ((num & ~mask) << (8 - rdist)))
          expect(bnum.lrot8(-rdist)).to eq (((bnum & mask) >> rdist) | (((bnum & ~mask) & MASK_8) << (8 - rdist)) | (bnum & ~0xFF))
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
          expect(num.lrot8(n)).to eq num.lrot8(n % 8)
          expect(bnum.lrot8(n)).to eq bnum.lrot8(n % 8)
        end
      end
    end
  end

  context "with a Bignum as rotate distance" do
    it "still works the same" do
      100.times do
        num = rand(1 << 32)
        expect(num.lrot8(14143509919777)).to eq num.lrot8(14143509919777 % 8)
      end
    end
  end

  context "with a 0 as rotate distance" do
    it "returns the receiver" do
      expect(100.lrot8(0)).to eq 100
      expect((111 << 80).lrot8(0)).to eq (111 << 80)
    end
  end
end

describe "#lrot16" do
  it "rotates the low 16 bits to the left" do
    100.times do
      num  = rand(1 << 16)
      bnum = rand(1 << 90)
      1.upto(15) do |rdist|
        mask = (1 << (16 - rdist)) - 1
        expect(num.lrot16(rdist)).to eq (((num & mask) << rdist) | ((num & ~mask) >> (16 - rdist)))
        expect(bnum.lrot16(rdist)).to eq (((bnum & mask) << rdist) | (((bnum & ~mask) & MASK_16) >> (16 - rdist)) | (bnum & ~0xFFFF))
      end
    end
  end

  it "doesn't modify the higher bits" do
    100.times do
      num  = rand(1 << 32)
      bnum = rand(1 << 90)
      1.upto(30) do |rdist|
        expect(num.lrot16(rdist) & ~MASK_16).to eq (num & ~MASK_16)
        expect(bnum.lrot16(rdist) & ~MASK_16).to eq (bnum & ~MASK_16)
      end
    end
  end

  context "with a negative rotate distance" do
    it "rotates the low 16 bits to the right" do
      100.times do
        num  = rand(1 << 16)
        bnum = rand(1 << 90)
        1.upto(15) do |rdist|
          mask = ~((1 << rdist) - 1) & MASK_16
          expect(num.lrot16(-rdist)).to eq (((num & mask) >> rdist) | ((num & ~mask) << (16 - rdist)))
          expect(bnum.lrot16(-rdist)).to eq (((bnum & mask) >> rdist) | (((bnum & ~mask) & MASK_16) << (16 - rdist)) | (bnum & ~0xFFFF))
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
          expect(num.lrot16(n)).to eq num.lrot16(n % 16)
          expect(bnum.lrot16(n)).to eq bnum.lrot16(n % 16)
        end
      end
    end
  end

  context "with a Bignum as rotate distance" do
    it "still works the same" do
      100.times do
        num = rand(1 << 32)
        expect(num.lrot16(14143509919777)).to eq num.lrot16(14143509919777 % 16)
      end
    end
  end

  context "with a 0 as rotate distance" do
    it "returns the receiver" do
      expect(100.lrot16(0)).to eq 100
      expect((111 << 80).lrot16(0)).to eq (111 << 80)
    end
  end
end