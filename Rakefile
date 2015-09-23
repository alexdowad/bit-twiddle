require "rake/extensiontask"
require "rspec/core/rake_task"
require "yard"

ROOT = File.dirname(__FILE__)

desc "Run all the tests in ./spec"
RSpec::Core::RakeTask.new(:spec)

desc "Rake the Yard (...actually, generate HTML documentation)"
YARD::Rake::YardocTask.new

Rake::ExtensionTask.new "bit_twiddle" do |ext|
  ext.lib_dir = "lib"
end

def benchmarks
  Dir[ROOT + '/bench/*_bench.rb']
end
def bench_task_name(file_name)
  file_name.sub(ROOT+'/', '').sub(/\_bench.rb$/, '').to_s.tr('/', ':')
end

benchmarks.each do |bench_file|
  name = bench_task_name(bench_file)

  desc "Benchmark #{name}"
  task name do
    $LOAD_PATH.unshift ROOT+'/lib'
    require 'benchmark/ips'
    require 'bit-twiddle'
    load bench_file
  end
end

desc "Run all benchmarks"
task bench: benchmarks.map(&method(:bench_task_name))

desc "Perform static analysis on C code to look for possible bugs"
task :scanbuild => [:clean] do
  # Show output at console, but also capture it
  result = []
  r,w = IO.pipe
  pid = Process.fork do
    $stdout.reopen(w)
    r.close
    exec("scan-build rake compile")
  end
  w.close
  r.each do |line|
    result << line if line =~ /^scan-build:/
    puts line
  end
  Process.wait(pid)

  if result.last =~ /Run 'scan-view ([^']*)' to examine bug reports/
    exec "scan-view #{$1}"
  end
end

task default: [:compile, :spec]
