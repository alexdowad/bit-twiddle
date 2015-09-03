describe "#bswap16" do
  it "returns 0 for 0" do
    expect(0.bswap16).to eq 0
  end

  it "reverses the bottom 2 bytes for 16-bit numbers" do
    bytes = [0, 1, 2, 3, 10, 100, 254, 255]
    bytes.combination(2) do |(a, b)|
      expect(((a << 8) + b).bswap16).to eq ((b << 8) + a)
      expect(((b << 8) + a).bswap16).to eq ((a << 8) + b)
    end
  end

  it "doesn't touch the higher bytes for larger numbers" do
    100.times do
      num = rand(1 << 32)
      expect(num.bswap16 & 0xFFFF0000).to eq (num & 0xFFFF0000)
    end
  end

  it "works on huge numbers" do
    100.times do
      huge    = rand(1 << 100)
      swapped = huge.bswap16
      expect(huge & ~MASK_16).to eq (swapped & ~MASK_16)
      expect(huge & 0xFF).to eq ((swapped & 0xFF00) >> 8)
      expect((huge & 0xFF00) >> 8).to eq (swapped & 0xFF)
    end
  end
end

describe "#bswap32" do
  it "returns 0 for 0" do
    expect(0.bswap32).to eq 0
  end

  it "reverses the bottom 4 bytes for 32-bit numbers" do
    bytes = [0, 1, 2, 3, 10, 100, 254, 255]
    bytes.repeated_combination(4) do |(a, b, c, d)|
      num1 = (a << 24) + (b << 16) + (c << 8) + d
      num2 = a + (b << 8) + (c << 16) + (d << 24)
      expect(num1.bswap32).to eq num2
    end
  end

  it "doesn't touch the higher bytes for larger numbers" do
    100.times do
      num = rand(1 << 48)
      expect(num.bswap32 & 0xFFFFFFFF00000000).to eq (num & 0xFFFFFFFF00000000)
    end
  end

  it "works on huge numbers" do
    100.times do
      huge    = rand(1 << 100)
      swapped = huge.bswap32
      expect(huge & ~MASK_32).to eq (swapped & ~MASK_32)
      expect(huge & 0xFF).to eq ((swapped & 0xFF000000) >> 24)
      expect((huge & 0xFF00) >> 8).to eq ((swapped & 0xFF0000) >> 16)
      expect((huge & 0xFF0000) >> 16).to eq ((swapped & 0xFF00) >> 8)
      expect((huge & 0xFF000000) >> 24).to eq (swapped & 0xFF)
    end
  end
end

describe "#bswap64" do
  it "returns 0 for 0" do
    expect(0.bswap64).to eq 0
  end

  it "reverses the bottom 4 bytes for 32-bit numbers" do
    bytes = [0, 1, 2, 3, 10, 12, 100, 101, 254, 255]
    bytes.combination(8) do |(a, b, c, d, e, f, g, h)|
      num1 = (a << 56) + (b << 48) + (c << 40) + (d << 32) + (e << 24) + (f << 16) + (g << 8) + h
      num2 = a + (b << 8) + (c << 16) + (d << 24) + (e << 32) + (f << 40) + (g << 48) + (h << 56)
      expect(num1.bswap64).to eq num2
    end
  end

  it "doesn't touch the higher bytes for larger numbers" do
    100.times do
      num = rand(1 << 80)
      expect(num.bswap64 & ~MASK_64).to eq (num & ~MASK_64)
    end
  end

  it "works on huge numbers" do
    100.times do
      huge    = rand(1 << 100)
      swapped = huge.bswap64
      expect(huge & ~MASK_64).to eq (swapped & ~MASK_64)
      expect(huge & 0xFF).to eq ((swapped & 0xFF00000000000000) >> 56)
      expect((huge & 0xFF00) >> 8).to eq ((swapped & 0xFF000000000000) >> 48)
      expect((huge & 0xFF0000) >> 16).to eq ((swapped & 0xFF0000000000) >> 40)
      expect((huge & 0xFF000000) >> 24).to eq ((swapped & 0xFF00000000) >> 32)
      expect((huge & 0xFF00000000) >> 32).to eq ((swapped & 0xFF000000) >> 24)
      expect((huge & 0xFF0000000000) >> 40).to eq ((swapped & 0xFF0000) >> 16)
      expect((huge & 0xFF000000000000) >> 48).to eq ((swapped & 0xFF00) >> 8)
      expect((huge & 0xFF00000000000000) >> 56).to eq (swapped & 0xFF)
    end
  end
end