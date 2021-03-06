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

  it "raises a RangeError for negative numbers" do
    0.upto(100) do |n|
      num = -2 ** n
      expect { num.bitreverse8 }.to raise_error(RangeError)
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

  it "raises a RangeError for negative numbers" do
    0.upto(100) do |n|
      num = -2 ** n
      expect { num.bitreverse16 }.to raise_error(RangeError)
    end
  end
end

describe "#bitreverse32" do
  [[0, 0],
   [1, (1 << 31)],
   [2, (1 << 30)],
   [3, (3 << 30)],
   [4, (1 << 29)],
   [255, 0xFF000000],
   [0xFFFFFFFF, 0xFFFFFFFF]].each do |a,b|
    context "on #{a}" do
      it "returns #{b}" do
        expect(a.bitreverse32).to eq b
      end
    end

    context "on (1 << 40) + #{a}" do
      it "returns (1 << 40) + #{b}" do
        expect(((1 << 40) + a).bitreverse32).to eq ((1 << 40) + b)
      end
    end

    context "on (1 << 80) + #{a}" do
      it "returns (1 << 80) + #{b}" do
        expect(((1 << 80) + a).bitreverse32).to eq ((1 << 80) + b)
      end
    end
  end

  100.times do
    bits = 32.times.collect { %w[0 1].sample }.join
    context "on #{bits.to_i(2)}" do
      it "returns #{bits.reverse.to_i(2)}" do
        expect(bits.to_i(2).bitreverse32).to eq bits.reverse.to_i(2)
      end
    end
  end

  it "raises a RangeError for negative numbers" do
    0.upto(100) do |n|
      num = -2 ** n
      expect { num.bitreverse32 }.to raise_error(RangeError)
    end
  end
end

describe "#bitreverse64" do
  [[0, 0],
   [1, (1 << 63)],
   [2, (1 << 62)],
   [3, (3 << 62)],
   [4, (1 << 61)],
   [255, 0xFF00000000000000],
   [0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF]].each do |a,b|
    context "on #{a}" do
      it "returns #{b}" do
        expect(a.bitreverse64).to eq b
      end
    end

    context "on (1 << 80) + #{a}" do
      it "returns (1 << 80) + #{b}" do
        expect(((1 << 80) + a).bitreverse64).to eq ((1 << 80) + b)
      end
    end
  end

  100.times do
    bits = 64.times.collect { %w[0 1].sample }.join
    context "on #{bits.to_i(2)}" do
      it "returns #{bits.reverse.to_i(2)}" do
        expect(bits.to_i(2).bitreverse64).to eq bits.reverse.to_i(2)
      end
    end
  end

  it "raises a RangeError for negative numbers" do
    0.upto(100) do |n|
      num = -2 ** n
      expect { num.bitreverse64 }.to raise_error(RangeError)
    end
  end
end
