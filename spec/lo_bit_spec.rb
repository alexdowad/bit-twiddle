describe "#lo_bit" do
  it "returns N+1 for 1 << N" do
    0.upto(200) do |n|
      expect((1 << n).lo_bit).to eq (n+1)
    end
  end

  it "returns 1 for (1 << N)-1" do
    1.upto(200) do |n|
      expect(((1 << n) - 1).lo_bit).to eq 1
    end
  end

  it "returns N+1 for 3 << N" do
    0.upto(200) do |n|
      expect((3 << n).lo_bit).to eq (n+1)
    end
  end

  it "returns 1 for any number with the LSB set" do
    100.times do
      expect((rand(100000) | 1).lo_bit).to eq 1
    end
  end

  it "returns 0 for 0" do
    expect(0.lo_bit).to eq 0
  end

  it "returns the same for negative numbers as for their absolute values" do
    0.upto(100) do |n|
      num = -2 ** n
      expect(num.lo_bit).to eq num.abs.lo_bit
    end
  end
end