Gem::Specification.new do |s|
  s.name        = 'bit-twiddle'
  s.version     = '0.0.5'
  s.date        = Time.now.strftime('%Y-%m-%d')
  s.summary     = 'Fast bitwise operations for Ruby'
  s.description = 'Fast (native) bitwise operations for Ruby, in addition to the ones provided by the standard library'
  s.authors     = ['Alex Dowad']
  s.email       = 'alexinbeijing@gmail.com'
  s.files       = Dir["lib/**/*", "ext/**/*", "LICENSE", "*.md"]
  s.extensions  = ['ext/bit_twiddle/extconf.rb']
  s.homepage    = 'http://github.com/alexdowad/bit-twiddle'
  s.license     = 'None (Public Domain)'

  s.required_ruby_version = ">= 2.2.8"

  s.add_development_dependency "rspec", "~> 3.7"
  s.add_development_dependency "rake",  "~> 12.2"
  s.add_development_dependency "rake-compiler", "~> 0.9"
  s.add_development_dependency "benchmark-ips", "~> 2.7"
  s.add_development_dependency "yard", "~> 0.9"
end
