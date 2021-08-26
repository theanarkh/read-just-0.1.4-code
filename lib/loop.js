const { runMicroTasks } = just.sys
const {
  create, wait, control, EPOLL_CLOEXEC, EPOLL_CTL_ADD,
  EPOLL_CTL_DEL, EPOLL_CTL_MOD, EPOLLIN
} = just.loop

function createLoop (nevents = 128) {
  const evbuf = new ArrayBuffer(nevents * 12)
  const events = new Uint32Array(evbuf)
  // 创建一个epoll
  const loopfd = create(EPOLL_CLOEXEC)
  const handles = {}
  // 判断是否有事件触发
  function poll (timeout = -1, sigmask) {
    let r = 0
    // 对epoll_wait的封装
    if (sigmask) {
      r = wait(loopfd, evbuf, timeout, sigmask)
    } else {
      r = wait(loopfd, evbuf, timeout)
    }
    if (r > 0) {
      let off = 0
      for (let i = 0; i < r; i++) {
        const fd = events[off + 1]
        // 事件触发，执行回调
        handles[fd](fd, events[off])
        off += 3
      }
    }
    return r
  }
  // 注册新的fd和事件
  function add (fd, callback, events = EPOLLIN) {
    const r = control(loopfd, EPOLL_CTL_ADD, fd, events)
    if (r === 0) {
      handles[fd] = callback
      instance.count++
    }
    return r
  }
  // 删除之前注册的fd和事件
  function remove (fd) {
    const r = control(loopfd, EPOLL_CTL_DEL, fd)
    if (r === 0) {
      delete handles[fd]
      instance.count--
    }
    return r
  }
  // 更新之前注册的fd和事件
  function update (fd, events = EPOLLIN) {
    const r = control(loopfd, EPOLL_CTL_MOD, fd, events)
    return r
  }
  const instance = { fd: loopfd, poll, add, remove, update, handles, count: 0 }
  return instance
}

const factory = {
  // 事件循环数组，可以有多个事件循环
  loops: [],
  paused: true,
  // 创建一个事件循环
  create: (nevents = 128) => {
    const loop = createLoop(nevents)
    factory.loops.push(loop)
    return loop
  },
  // 执行事件循环，即遍历每个事件循环
  run: (ms = -1) => {
    factory.paused = false
    let empty = 0
    // TODO: this is a bug. if we put something on loop it won't run unless we explicitly kick the factory
    while (!factory.paused) {
      let total = 0
      for (const loop of factory.loops) {
        if (loop.count > 0) loop.poll(ms)
        total += loop.count
      }
      // 执行微任务
      runMicroTasks()
      if (total === 0) {
        runMicroTasks()
        empty++
        if (empty === 3) {
          factory.paused = true
          // todo: we should only break if loop is empty after a number of cycles, otherwise user has to do nextTick when closing handles
          break
        }
      }
    }
  },
  stop: () => {
    factory.paused = true
  },
  shutdown: () => {
    // todo: clean shutdown of all the loops
  }
}

module.exports = { factory }
