Gem::Specification.new do |s|
  s.name        = 'bit-twiddle'
  s.version     = '0.0.2'
  s.date        = Time.now.strftime('%Y-%m-%d')
  s.summary     = 'Fast bitwise operations for Ruby'
  s.description = 'Fast (native) bitwise operations for Ruby, in addition to the ones provided by the standard library'
  s.authors     = ['Alex Dowad']
  s.email       = 'alexinbeijing@gmail.com'
  s.files       = Dir["lib/*", "ext/*", "LICENSE", "*.md"]
  s.extensions  = ['ext/bit_twiddle/extconf.rb']
  s.homepage    = 'http://github.com/alexdowad/bit-twiddle'
  s.license     = 'None (Public Domain)'

  s.add_development_dependency "rspec", "~> 3.3"
  s.add_development_dependency "rake",  "~> 10.4"
  s.add_development_dependency "rake-compiler", "~> 0.9"
  s.add_development_dependency "benchmark-ips", "~> 2.3"
  s.add_development_dependency "yard", "~> 0.8"
end
