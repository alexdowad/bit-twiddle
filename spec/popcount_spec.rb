describe "#popcount" do
  it "returns 0 for 0" do
    expect(0.popcount).to be 0
  end

  it "returns 1 for numbers with only a single 1 bit" do
    0.upto(200) do |n|
      expect((1 << n).popcount).to eq 1
    end
  end

  it "returns 2 for numbers with 2 1 bits" do
    0.upto(200) do |n|
      expect((3 << n).popcount).to eq 2
      expect((5 << n).popcount).to eq 2
      expect((9 << n).popcount).to eq 2
      unless n == 0
        expect(((1 << n) + 1).popcount).to eq 2
      end
    end
  end

  it "returns N for (1 << N)-1" do
    0.upto(200) do |n|
      expect(((1 << n) - 1).popcount).to eq n
    end
  end

  it "raises a RangeError for negative numbers" do
    0.upto(100) do |n|
      num = -2 ** n
      expect { num.popcount }.to raise_error(RangeError)
    end
  end
end