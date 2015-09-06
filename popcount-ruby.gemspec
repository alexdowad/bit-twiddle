Gem::Specification.new do |s|
  s.name        = 'popcount-ruby'
  s.version     = '0.0.1'
  s.date        = '2015-08-29'
  s.summary     = 'Fast (native) popcount for Ruby'
  s.description = s.summary
  s.authors     = ['Alex Dowad']
  s.email       = 'alexinbeijing@gmail.com'
  s.files       = Dir["lib/*", "ext/*", "LICENSE", "*.md"]
  s.extensions  = ['ext/popcount/extconf.rb']
  s.homepage    = 'http://github.com/alexdowad/popcount'
  s.license     = 'None (Public Domain)'

  s.add_development_dependency "rspec", "~> 3.3"
  s.add_development_dependency "rake",  "~> 10.4"
  s.add_development_dependency "rake-compiler", "~> 0.9"
  s.add_development_dependency "benchmark-ips", "~> 2.3"
end
