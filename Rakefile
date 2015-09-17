require "rake/extensiontask"

ROOT = File.dirname(__FILE__)

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