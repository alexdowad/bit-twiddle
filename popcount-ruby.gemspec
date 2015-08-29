Gem::Specification.new do |s|
  s.name        = 'popcount-ruby'
  s.version     = '0.0.1'
  s.date        = '2015-08-29'
  s.summary     = 'Fast (native) popcount for Ruby'
  s.description = 'Fast (native) popcount for Ruby'
  s.authors     = ['Alex Dowad']
  s.email       = 'alexinbeijing@gmail.com'
  s.files       = Dir["lib/*", "ext/*", "LICENSE", "*.md"]
  s.extensions  = ['ext/popcount/extconf.rb']
  s.homepage    = 'http://github.com/alexdowad/popcount'
  s.license     = 'None (Public Domain)'
end
