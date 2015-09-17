describe "#bitreverse8" do
  [[0, 0],
   [1, 128],
   [2, 64],
   [3, 192],
   [4, 32],
   [11, 208],
   [15, 240],
   ["10101010".to_i(2), "01010101".to_i(2)],
   [254, 127],
   [255, 255]].each do |a, b|
    context "on #{a}" do
      it "returns #{b}" do
        expect(a.bitreverse8).to eq b
      end
    end

    context "on (1 << 15) + #{a}" do
      it "returns (1 << 15) + #{b}" do
        expect(((1 << 15) + a).bitreverse8).to eq ((1 << 15) + b)
      end
    end

    context "on (1 << 32) + #{a}" do
      it "returns (1 << 32) + #{b}" do
        expect(((1 << 32) + a).bitreverse8).to eq ((1 << 32) + b)
      end
    end

    context "on (1 << 80) + #{a}" do
      it "returns (1 << 80) + #{b}" do
        expect(((1 << 80) + a).bitreverse8).to eq ((1 << 80) + b)
      end
    end
  end
end

describe "#bitreverse16" do
  [[0, 0],
   [1, (1 << 15)],
   [2, (1 << 14)],
   [3, (3 << 14)],
   [4, (1 << 13)],
   [255, 0xFF00],
   [0xFFFF, 0xFFFF]].each do |a,b|
    context "on #{a}" do
      it "returns #{b}" do
        expect(a.bitreverse16).to eq b
      end
    end

    context "on (1 << 32) + #{a}" do
      it "returns (1 << 32) + #{b}" do
        expect(((1 << 32) + a).bitreverse16).to eq ((1 << 32) + b)
      end
    end

    context "on (1 << 80) + #{a}" do
      it "returns (1 << 80) + #{b}" do
        expect(((1 << 80) + a).bitreverse16).to eq ((1 << 80) + b)
      end
    end
  end

  100.times do
    bits = 16.times.collect { %w[0 1].sample }.join
    context "on #{bits.to_i(2)}" do
      it "returns #{bits.reverse.to_i(2)}" do
        expect(bits.to_i(2).bitreverse16).to eq bits.reverse.to_i(2)
      end
    end
  end
end