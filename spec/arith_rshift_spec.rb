[[:arith_rshift8, 8], [:arith_rshift16, 16], [:arith_rshift32, 32], [:arith_rshift64, 64]].each do |method, bits|
  describe "##{method}" do
    bitmask = (1 << bits) - 1

    it "shifts bits in a #{bits}-bit number to the right" do
      100.times do
        num  = rand(1 << bits)
        bnum = rand(1 << 100)

        1.upto(bits+10) do |sdist|
          mask = sdist <= bits ? ~((1 << (bits - sdist)) - 1) : bitmask
          if num & (1 << (bits-1)) == 0
            expect(num.send(method, sdist)).to  eq (num >> sdist)
          else
            expect(num.send(method, sdist)).to  eq (((num >> sdist) | mask) & bitmask)
          end
          if bnum & (1 << (bits-1)) == 0
            expect(bnum.send(method, sdist)).to eq ((bnum & bitmask) >> sdist) | (bnum & ~bitmask)
          else
            expect(bnum.send(method, sdist)).to eq (((bnum >> sdist) | mask) & bitmask) | (bnum & ~bitmask)
          end
        end
      end
    end

    it "does a left shift if shift distance is negative (but cuts off high bits)" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 100)
        1.upto(bits+10) do |sdist|
          expect(num.send(method, -sdist)).to  eq ((num << sdist)  & bitmask) | (num  & ~bitmask)
          expect(bnum.send(method, -sdist)).to eq ((bnum << sdist) & bitmask) | (bnum & ~bitmask)
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

    it "fills in the high end with zeroes if the high bit was 0" do
      100.times do
        num  = rand(1 << 32)  & ~(1 << (bits-1)) # turn high bit off
        bnum = rand(1 << 100) & ~(1 << (bits-1)) # turn high bit off
        1.upto(bits+10) do |sdist|
          mask = sdist <= bits ? ~((1 << (bits - sdist)) - 1) & bitmask : bitmask
          expect(num.send(method, sdist)  & mask).to eq 0
          expect(bnum.send(method, sdist) & mask).to eq 0
        end
      end
    end

    it "fills in the high end with ones if the high bit was 1" do
      100.times do
        num  = rand(1 << 32)  | (1 << (bits-1)) # turn high bit on
        bnum = rand(1 << 100) | (1 << (bits-1)) # turn high bit on
        1.upto(bits+10) do |sdist|
          mask = sdist <= bits ? ~((1 << (bits - sdist)) - 1) & bitmask : bitmask
          expect(num.send(method, sdist) & mask).to  eq (mask & bitmask)
          expect(bnum.send(method, sdist) & mask).to eq (mask & bitmask)
        end
      end
    end

    it "doesn't modify bits above number #{bits}" do
      100.times do
        num  = rand(1 << 32)
        bnum = rand(1 << 100)
        -64.upto(64) do |sdist|
          expect(num.send(method, sdist)  & ~bitmask).to eq (num  & ~bitmask)
          expect(bnum.send(method, sdist) & ~bitmask).to eq (bnum & ~bitmask)
        end
      end
    end

    it "zeroes out low #{bits} bits when shift distance is a Bignum and high bit is 0" do
      expect(100.send(method, 1 << 100)).to eq 0
      expect((1 << 100).send(method, 1 << 100)).to eq (1 << 100)
      expect(((1 << 100)+1).send(method, 1 << 80)).to eq (1 << 100)
    end
  end
end