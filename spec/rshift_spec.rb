[[:rshift8, 8], [:rshift16, 16], [:rshift32, 32], [:rshift64, 64]].each do |method, bits|
  describe "##{method}" do
    bitmask = (1 << bits) - 1

    it "shifts bits in a #{bits}-bit number to the right (zeroing out the high bits)" do
      100.times do
        num  = rand(1 << bits)
        bnum = rand(1 << 100)
        1.upto(bits+10) do |sdist|
          expect(num.send(method, sdist)).to  eq (num >> sdist)
          expect(bnum.send(method, sdist)).to eq ((bnum & bitmask) >> sdist) | (bnum & ~bitmask)
        end
      end
    end

    it "does a left shift if shift distance is negative (but cuts off high bits)" do
      100.times do
        num  = rand(1 << bits)
        bnum = rand(1 << 100)
        1.upto(bits+10) do |sdist|
          expect(num.send(method, -sdist)).to eq ((num << sdist) & bitmask)
          expect(bnum.send(method, -sdist)).to eq (((bnum & bitmask) << sdist) & bitmask) | (bnum & ~bitmask)
        end
      end
    end

    it "returns the receiver if shift distance is zero" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 100)
        expect(num.send(method, 0)).to  eq num
        expect(bnum.send(method, 0)).to eq bnum
      end
    end

    it "fills in the high end with zeroes" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 90)
        1.upto(bits+10) do |sdist|
          mask = sdist <= bits ? ~((1 << (bits - sdist)) - 1) & bitmask : bitmask
          expect(num.send(method, sdist)  & mask).to eq 0
          expect(bnum.send(method, sdist) & mask).to eq 0
        end
      end
    end

    it "doesn't modify bits above number #{bits}" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 90)
        -64.upto(64) do |sdist|
          expect(num.send(method, sdist)  & ~bitmask).to eq (num  & ~bitmask)
          expect(bnum.send(method, sdist) & ~bitmask).to eq (bnum & ~bitmask)
        end
      end
    end

    it "zeroes out the low 8 bits when the shift distance is a Bignum" do
      expect(100.send(method, 1 << 100)).to eq 0
      expect((1 << 100).send(method, 1 << 100)).to eq (1 << 100)
    end
  end
end