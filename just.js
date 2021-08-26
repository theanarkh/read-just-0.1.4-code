(function () {
  function wrapMemoryUsage (memoryUsage) {
    const mem = new BigUint64Array(16)
    return () => {
      memoryUsage(mem)
      return {
        rss: mem[0],
        total_heap_size: mem[1],
        used_heap_size: mem[2],
        external_memory: mem[3],
        heap_size_limit: mem[5],
        total_available_size: mem[10],
        total_heap_size_executable: mem[11],
        total_physical_size: mem[12]
      }
    }
  }

  function wrapCpuUsage (cpuUsage) {
    const cpu = new Uint32Array(4)
    const result = { elapsed: 0, user: 0, system: 0, cuser: 0, csystem: 0 }
    const clock = cpuUsage(cpu)
    const last = { user: cpu[0], system: cpu[1], cuser: cpu[2], csystem: cpu[3], clock }
    return () => {
      const clock = cpuUsage(cpu)
      result.elapsed = clock - last.clock
      result.user = cpu[0] - last.user
      result.system = cpu[1] - last.system
      result.cuser = cpu[2] - last.cuser
      result.csystem = cpu[3] - last.csystem
      last.user = cpu[0]
      last.system = cpu[1]
      last.cuser = cpu[2]
      last.csystem = cpu[3]
      last.clock = clock
      return result
    }
  }

  function wrapgetrUsage (getrUsage) {
    const res = new Float64Array(16)
    return () => {
      getrUsage(res)
      return {
        user: res[0],
        system: res[1],
        maxrss: res[2],
        ixrss: res[3],
        idrss: res[4],
        isrss: res[5],
        minflt: res[6],
        majflt: res[7],
        nswap: res[8],
        inblock: res[9],
        outblock: res[10],
        msgsnd: res[11],
        msgrcv: res[12],
        ssignals: res[13],
        nvcsw: res[14],
        nivcsw: res[15]
      }
    }
  }

  function wrapHeapUsage (heapUsage) {
    const heap = (new Array(16)).fill(0).map(v => new Float64Array(4))
    return () => {
      const usage = heapUsage(heap)
      usage.spaces = Object.keys(usage.heapSpaces).map(k => {
        const space = usage.heapSpaces[k]
        return {
          name: k,
          size: space[2],
          used: space[3],
          available: space[1],
          physicalSize: space[0]
        }
      })
      delete usage.heapSpaces
      return usage
    }
  }

  function wrapEnv (env) {
    return () => {
      // ["a=b", "c=d"] => [["a", "b"], ["c", "d"]]
      return env()
        .map(entry => entry.split('='))
        .reduce((e, pair) => { e[pair[0]] = pair[1]; return e }, {})
    }
  }
  // C++模块加载器
  function wrapLibrary (cache = {}) {
    // 加载动态库，类似Node.js的Addon
    function loadLibrary (path, name) {
      if (cache[name]) return cache[name]
      if (!just.sys.dlopen) return
      // 打开动态库
      const handle = just.sys.dlopen(path, just.sys.RTLD_LAZY)
      if (!handle) return
      // 找到动态库里约定格式的函数的虚拟地址
      const ptr = just.sys.dlsym(handle, `_register_${name}`)
      if (!ptr) return
      // 以该虚拟地址为入口执行函数
      const lib = just.load(ptr)
      if (!lib) return
      lib.close = () => just.sys.dlclose(handle)
      lib.type = 'module-external'
      cache[name] = lib
      return lib
    }
    // 加载C++模块
    function library (name, path) {
      if (cache[name]) return cache[name]
      const lib = just.load(name)
      if (!lib) {
        if (path) return loadLibrary(path, name)
        return loadLibrary(`${name}.so`, name)
      }
      lib.type = 'module'
      cache[name] = lib
      return lib
    }

    return { library, cache }
  }

  function wrapRequire (cache = {}) {
    const appRoot = just.sys.cwd()
    const { HOME, JUST_TARGET } = just.env()
    const justDir = JUST_TARGET || `${HOME}/.just`
    // 加载内置JS模块
    function requireNative (path) {
      path = `lib/${path}.js`
      if (cache[path]) return cache[path].exports
      const { vm } = just
      const params = ['exports', 'require', 'module']
      const exports = {}
      const module = { exports, type: 'native', dirName: appRoot }
      // 从数据结构中获得模块对应的源码
      module.text = just.builtin(path)
      if (!module.text) return
      // 编译
      const fun = vm.compile(module.text, path, params, [])
      module.function = fun
      cache[path] = module
      // 执行
      fun.call(exports, exports, p => just.require(p, module), module)
      return module.exports
    }
    // 一般JS模块加载器
    function require (path, parent = { dirName: appRoot }) {
      const { join, baseName, fileName } = just.path
      if (path[0] === '@') path = `${appRoot}/lib/${path.slice(1)}/${fileName(path.slice(1))}.js`
      const ext = path.split('.').slice(-1)[0]
      if (ext === 'js' || ext === 'json') {
        let dirName = parent.dirName
        const fileName = join(dirName, path)
        if (cache[fileName]) return cache[fileName].exports
        dirName = baseName(fileName)
        const params = ['exports', 'require', 'module']
        const exports = {}
        const module = { exports, dirName, fileName, type: ext }
        // todo: this is not secure
        // 文件存在则直接加载
        if (just.fs.isFile(fileName)) {
          module.text = just.fs.readFile(fileName)
        } else {
          // 否则尝试加载内置JS模块
          path = fileName.replace(appRoot, '')
          if (path[0] === '/') path = path.slice(1)
          module.text = just.builtin(path)
          if (!module.text) {
            path = `${justDir}/${path}`
            if (!just.fs.isFile(path)) return
            module.text = just.fs.readFile(path)
            if (!module.text) return
          }
        }
        cache[fileName] = module
        if (ext === 'js') {
          const fun = just.vm.compile(module.text, fileName, params, [])
          module.function = fun
          fun.call(exports, exports, p => require(p, module), module)
        } else {
          // 是json文件则直接parse
          module.exports = JSON.parse(module.text)
        }
        return module.exports
      }
      return requireNative(path, parent)
    }

    return { requireNative, require, cache }
  }

  function setTimeout (callback, timeout, repeat = 0, loop = just.factory.loop) {
    const buf = new ArrayBuffer(8)
    const timerfd = just.sys.timer(repeat, timeout)
    loop.add(timerfd, (fd, event) => {
      callback()
      just.net.read(fd, buf, 0, buf.byteLength)
      if (repeat === 0) {
        loop.remove(fd)
        just.net.close(fd)
      }
    })
    return timerfd
  }

  function setInterval (callback, timeout, loop = just.factory.loop) {
    return setTimeout(callback, timeout, timeout, loop)
  }

  function clearTimeout (fd, loop = just.factory.loop) {
    loop.remove(fd)
    just.net.close(fd)
  }

  class SystemError extends Error {
    constructor (syscall) {
      const { sys } = just
      const errno = sys.errno()
      const message = `${syscall} (${errno}) ${sys.strerror(errno)}`
      super(message)
      this.errno = errno
      this.name = 'SystemError'
    }
  }
  // 设置文件描述符为非阻塞
  function setNonBlocking (fd) {
    let flags = just.fs.fcntl(fd, just.sys.F_GETFL, 0)
    if (flags < 0) return flags
    flags |= just.net.O_NONBLOCK
    return just.fs.fcntl(fd, just.sys.F_SETFL, flags)
  }
  // 解析参数--a --b则{ "a": true, "b": true }
  function parseArgs (args) {
    const opts = {}
    args = args.filter(arg => {
      if (arg.slice(0, 2) === '--') {
        opts[arg.slice(2)] = true
        return false
      }
      return true
    })
    opts.args = args
    return opts
  }

  function main (opts) {
    // 获得C++模块加载器和缓存
    const { library, cache } = wrapLibrary()
    let debugStarted = false

    delete global.console

    global.onUnhandledRejection = err => {
      just.error('onUnhandledRejection')
      if (err) just.error(err.stack)
    }

    // load the builtin modules
    // 挂载C++模块到JS
    just.vm = library('vm').vm
    just.loop = library('epoll').epoll
    just.fs = library('fs').fs
    just.net = library('net').net
    just.sys = library('sys').sys
    // 环境变量
    just.env = wrapEnv(just.sys.env)

    // todo: what about sharedarraybuffers?
    ArrayBuffer.prototype.writeString = function(str, off = 0) { // eslint-disable-line
      return just.sys.writeString(this, str, off)
    }
    ArrayBuffer.prototype.readString = function (len = this.byteLength, off = 0) { // eslint-disable-line
      return just.sys.readString(this, len, off)
    }
    ArrayBuffer.prototype.getAddress = function () { // eslint-disable-line
      return just.sys.getAddress(this)
    }
    ArrayBuffer.prototype.copyFrom = function (src, off = 0, len = src.byteLength, soff = 0) { // eslint-disable-line
      return just.sys.memcpy(this, src, off, len, soff)
    }
    ArrayBuffer.fromString = str => just.sys.calloc(1, str)
    String.byteLength = just.sys.utf8Length

    const { requireNative, require } = wrapRequire(cache)

    Object.assign(just.fs, requireNative('fs'))
    just.config = requireNative('config')
    just.path = requireNative('path')
    just.factory = requireNative('loop').factory
    just.factory.loop = just.factory.create(128)
    just.process = requireNative('process')
    just.setTimeout = setTimeout
    just.setInterval = setInterval
    just.clearTimeout = just.clearInterval = clearTimeout
    just.SystemError = SystemError
    just.library = library
    just.requireNative = requireNative
    just.net.setNonBlocking = setNonBlocking
    just.require = global.require = require
    just.require.cache = cache
    just.memoryUsage = wrapMemoryUsage(just.memoryUsage)
    just.cpuUsage = wrapCpuUsage(just.sys.cpuUsage)
    just.rUsage = wrapgetrUsage(just.sys.getrUsage)
    just.heapUsage = wrapHeapUsage(just.sys.heapUsage)

    function startup () {
      if (!just.args.length) return true
      if (just.workerSource) {
        const scriptName = just.path.join(just.sys.cwd(), just.args[0] || 'thread')
        const source = just.workerSource
        delete just.workerSource
        just.vm.runScript(source, scriptName)
        return
      }
      if (just.args.length === 1) {
        const replModule = just.require('repl')
        if (!replModule) {
          throw new Error('REPL not enabled. Maybe I should be a standalone?')
        }
        replModule.repl()
        return
      }
      if (just.args[1] === '--') {
        // todo: limit size
        // todo: allow streaming in multiple scripts with a separator and running them all
        const buf = new ArrayBuffer(4096)
        const chunks = []
        let bytes = just.net.read(just.sys.STDIN_FILENO, buf, 0, buf.byteLength)
        while (bytes > 0) {
          chunks.push(buf.readString(bytes))
          bytes = just.net.read(just.sys.STDIN_FILENO, buf, 0, buf.byteLength)
        }
        just.vm.runScript(chunks.join(''), 'stdin')
        return
      }
      if (just.args[1] === 'eval') {
        just.vm.runScript(just.args[2], 'eval')
        return
      }
      if (just.args[1] === 'build') {
        const buildModule = just.require('build')
        if (!buildModule) throw new Error('Build not Available')
        let config
        if (just.opts.config) {
          config = require(just.args[2]) || {}
        } else {
          if (just.args.length > 2) {
            config = just.require('configure').run(just.args[2], opts)
          } else {
            config = require(just.args[2] || 'config.json') || require('config.js') || {}
          }
        }
        buildModule.run(config, opts)
          .then(cfg => {
            if (opts.dump) just.print(JSON.stringify(cfg, null, '  '))
          })
          .catch(err => just.error(err.stack))
        return
      }
      if (just.args[1] === 'init') {
        const buildModule = just.require('build')
        if (!buildModule) throw new Error('Build not Available')
        buildModule.init(just.args[2] || 'hello')
        return
      }
      if (just.args[1] === 'clean') {
        const buildModule = just.require('build')
        if (!buildModule) throw new Error('Build not Available')
        buildModule.clean()
        return
      }
      const scriptName = just.path.join(just.sys.cwd(), just.args[1])
      just.vm.runScript(just.fs.readFile(just.args[1]), scriptName)
    }
    if (opts.inspector) {
      const inspectorLib = just.library('inspector')
      if (!inspectorLib) throw new SystemError('inspector module is not enabled')
      just.inspector = inspectorLib.inspector
      // TODO: this is ugly
      Object.assign(just.inspector, require('inspector'))
      just.encode = library('encode').encode
      just.sha1 = library('sha1').sha1
      global.process = {
        pid: just.sys.pid(),
        version: 'v15.6.0',
        arch: 'x64',
        env: just.env()
      }
      const _require = global.require
      global.require = (name, path) => {
        if (name === 'module') return ['fs', 'process', 'repl']
        return _require(name, path)
      }
      global.inspector = just.inspector.createInspector({
        title: 'Just!',
        onReady: () => {
          if (debugStarted) return just.factory.run()
          debugStarted = true
          if (!startup()) just.factory.run()
        }
      })
      just.inspector.enable()
      just.factory.run(1)
      return
    }
    // 进入事件循环
    if (!startup()) just.factory.run()
  }

  const opts = parseArgs(just.args)
  just.args = opts.args
  just.opts = opts
  if (opts.bare) {
    just.load('vm').vm.runScript(just.args[1], 'eval')
  } else {
    main(opts)
  }
})()
