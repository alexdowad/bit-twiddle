describe BitTwiddle do
  it "implements all the bit-twiddle methods, accepting a Fixnum or Bignum as an argument" do
    expect(BitTwiddle.lshift8(192, 1)).to eq 128
    expect(BitTwiddle.rshift8(192, 1)).to eq 96
    # TODO: check all the others
  end
end