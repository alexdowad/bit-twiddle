[[:rrot8, 8], [:rrot16, 16], [:rrot32, 32], [:rrot64, 64]].each do |method, bits|
  describe "##{method}" do
    bitmask = (1 << bits) - 1

    it "rotates the low #{bits} bits to the right" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 90)
        1.upto(bits) do |rdist|
          mask = ~((1 << rdist) - 1) & bitmask
          expect(num.send(method, rdist)).to  eq (((num  & mask) >> rdist) | (((num  & ~mask) & bitmask) << (bits - rdist)) | (num  & ~bitmask))
          expect(bnum.send(method, rdist)).to eq (((bnum & mask) >> rdist) | (((bnum & ~mask) & bitmask) << (bits - rdist)) | (bnum & ~bitmask))
        end
      end
    end

    it "doesn't modify the higher bits" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 90)
        -(bits+10).upto(bits+10) do |rdist|
          expect(num.send(method, rdist)  & ~bitmask).to eq (num  & ~bitmask)
          expect(bnum.send(method, rdist) & ~bitmask).to eq (bnum & ~bitmask)
        end
      end
    end

    context "with a negative rotate distance" do
      it "rotates the low #{bits} bits to the left" do
        100.times do
          num  = rand(1 << 32)
          bnum = rand(1 << 90)
          1.upto(bits) do |rdist|
            mask = (1 << (bits - rdist)) - 1
            expect(num.send(method, -rdist)).to  eq (((num  & mask) << rdist) | (((num  & ~mask) & bitmask) >> (bits - rdist)) | (num  & ~bitmask))
            expect(bnum.send(method, -rdist)).to eq (((bnum & mask) << rdist) | (((bnum & ~mask) & bitmask) >> (bits - rdist)) | (bnum & ~bitmask))
          end
        end
      end
    end

    context "with a rotate distance greater than #{bits-1}" do
      it "does the same as (rotate distance) % #{bits}" do
        100.times do
          num  = rand(1 << 32)
          bnum = rand(1 << 90)
          8.upto(100) do |n|
            expect(num.send(method, n)).to  eq num.send(method, n % bits)
            expect(bnum.send(method, n)).to eq bnum.send(method, n % bits)
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
            expect(num.send(method, rdist)).to  eq num.send(method, rdist % bits)
            expect(bnum.send(method, rdist)).to eq bnum.send(method, rdist % bits)
            expect(num.send(method, rdist)  & ~bitmask).to eq (num  & ~bitmask)
            expect(bnum.send(method, rdist) & ~bitmask).to eq (bnum & ~bitmask)
          end
        end
      end
    end

    context "with a 0 as rotate distance" do
      it "returns the receiver" do
        expect(100.send(method, 0)).to eq 100
        expect((111 << 80).send(method, 0)).to eq (111 << 80)
      end
    end
  end
end