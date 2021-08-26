const libs = [
  'lib/fs.js',
  'lib/loop.js',
  'lib/path.js',
  'lib/process.js',
  'lib/build.js',
  'lib/repl.js',
  'lib/configure.js',
  'lib/acorn.js'
]

const version = just.version.just
const v8flags = '--stack-trace-limit=10 --use-strict --disallow-code-generation-from-strings'
const v8flagsFromCommandLine = true
const debug = false
const capabilities = [] // list of allowed internal modules, api calls etc. TBD

const modules = [{
  name: 'sys',
  obj: [
    'modules/sys/sys.o'
  ],
  lib: ['dl', 'rt']
}, {
  name: 'fs',
  obj: [
    'modules/fs/fs.o'
  ]
}, {
  name: 'net',
  obj: [
    'modules/net/net.o'
  ]
}, {
  name: 'vm',
  obj: [
    'modules/vm/vm.o'
  ]
}, {
  name: 'epoll',
  obj: [
    'modules/epoll/epoll.o'
  ]
}]

const embeds = [
  'just.cc',
  'Makefile',
  'main.cc',
  'just.h',
  'just.js',
  'config.js',
  'lib/websocket.js',
  'lib/inspector.js'
]

const target = 'just'
const main = 'just.js'

module.exports = { version, libs, modules, capabilities, target, main, v8flags, embeds, static: false, debug, v8flagsFromCommandLine }
