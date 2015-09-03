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

  s.add_development_dependency "rspec", "~> 3.0"
  s.add_development_dependency "rake",  "~> 10.1"
  s.add_development_dependency "benchmark-ips", "~> 2.1"
end
