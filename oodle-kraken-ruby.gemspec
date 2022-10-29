Gem::Specification.new do |s|
  s.name = 'oodle-kraken-ruby'
  s.version = "0.9.0"
  s.author = "Brice Videau"
  s.email = "brice.videau@gmail.com"
  s.homepage = "https://github.com/kerilk/oodle-kraken-ruby"
  s.summary = "Library for decrompressing Oodle compressed file"
  s.description = "Uncompress files compressed with Oodle Kraken / Mermaid / Selkie / Leviathan / LZNA / Bitknit."
  s.files =  Dir[ 'oodle-kraken.gemspec', 'LICENSE', 'lib/**/*.rb', 'ext/oodle-kraken/extconf.rb', 'ext/oodle-kraken/*.c', 'ext/oodle-kraken/ooz/*' ]
  s.extensions << 'ext/oodle-kraken/extconf.rb'
  s.license = 'GPL-3.0-or-later'
  s.required_ruby_version = '>= 2.0.0'
end
