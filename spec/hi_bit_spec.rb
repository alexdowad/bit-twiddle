describe "#hi_bit" do
  it "returns N+1 for 1 << N" do
    0.upto(200) do |n|
      expect((1 << n).hi_bit).to eq (n+1)
    end
  end

  it "returns N for (1 << N)-1" do
    0.upto(200) do |n|
      expect(((1 << n) - 1).hi_bit).to eq n
    end
  end

  it "returns N+2 for 3 << N" do
    0.upto(200) do |n|
      expect((3 << n).hi_bit).to eq (n+2)
    end
  end

  it "returns 0 for 0" do
    expect(0.hi_bit).to eq 0
  end

  it "returns the same for negative numbers as for their absolute values" do
    0.upto(100) do |n|
      num = -2 ** n
      expect(num.hi_bit).to eq num.abs.hi_bit
    end
  end
end