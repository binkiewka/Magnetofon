import { app, shell, BrowserWindow, ipcMain, protocol } from 'electron'
import { execFile, spawn } from 'node:child_process'
import { existsSync } from 'node:fs'
import { promises as fs } from 'node:fs'
import net from 'node:net'
import path, { join } from 'node:path'
import { electronApp, optimizer, is } from '@electron-toolkit/utils'
import icon from '../../resources/icon.png?asset'

app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required')

const windows = {
  player: null,
  playlist: null,
  equalizer: null,
  visuals: null
}

protocol.registerSchemesAsPrivileged([
  {
    scheme: 'local',
    privileges: {
      secure: true,
      standard: true,
      supportFetchAPI: true,
      stream: true,
      corsEnabled: true
    }
  }
])

const windowConfigs = {
  player: { width: 940, height: 640, title: 'Magnetofon' },
  playlist: {
    width: 450,
    height: 400,
    minWidth: 450,
    minHeight: 220,
    title: 'Magnetofon Playlist'
  },
  equalizer: {
    width: 360,
    height: 335,
    minWidth: 300,
    minHeight: 335,
    title: 'Magnetofon Equalizer'
  },
  visuals: { width: 960, height: 640, minWidth: 400, minHeight: 300, title: 'Magnetofon Visuals' }
}

const settingsPath = join(app.getPath('userData'), 'config.json')
const visualsSettingsPath = join(app.getPath('userData'), 'visuals.json')

const nativeProjectM = {
  child: null,
  stderrTail: '',
  lastError: null,
  cachedAudioDevice: null,
  cachedAudioDeviceAt: 0
}

const nativeSurround = {
  child: null,
  monitorChild: null,
  positionTimer: null,
  file: null,
  device: null,
  monitorSource: null,
  cachedDevice: null,
  cachedDeviceAt: 0,
  volume: 80,
  paused: true,
  mode: 'off',
  lastError: null,
  ipcPath: null,
  stderrTail: '',
  stopping: false,
  stopPromise: null,
  startPromise: null
}

function execFileText(command, args) {
  return new Promise((resolve) => {
    execFile(command, args, { timeout: 3000 }, (error, stdout, stderr) => {
      resolve({ ok: !error, stdout: stdout || '', stderr: stderr || '', error })
    })
  })
}

async function listMpvAudioDevices() {
  const result = await execFileText('mpv', [
    '--no-config',
    '--idle=yes',
    '--frames=0',
    '--term-playing-msg=',
    '--audio-device=help'
  ])

  if (!result.ok) return []
  return result.stdout
    .split('\n')
    .map((line) => {
      const match = line.match(/^\s*'([^']+)'\s*\((.+)\)\s*$/)
      return match ? { id: match[1], description: match[2] } : null
    })
    .filter(Boolean)
}

function normalizePlaybackMode(mode) {
  const normalized = String(mode || 'AUTO').toUpperCase()
  return ['AUTO', 'STEREO', 'SURROUND'].includes(normalized) ? normalized : 'AUTO'
}

async function detectNativeAudioDevice({ surround = false, force = false } = {}) {
  if (!force && nativeSurround.cachedDevice && Date.now() - nativeSurround.cachedDeviceAt < 30000) {
    return nativeSurround.cachedDevice
  }

  const [info, sinks, mpvDevices] = await Promise.all([
    execFileText('pactl', ['info']),
    execFileText('pactl', ['list', 'sinks']),
    listMpvAudioDevices()
  ])

  if (!sinks.ok) return nativeSurround.cachedDevice || null

  const defaultSink = info.stdout.match(/^Default Sink:\s*(.+)$/m)?.[1]?.trim()
  const sinkBlocks = sinks.stdout.split(/\n(?=Sink #)/g)
  const parsedSinks = sinkBlocks
    .map((block) => {
      const name = block.match(/^\s*Name:\s*(.+)$/m)?.[1]?.trim()
      const channels = Number(block.match(/^\s*audio\.channels\s*=\s*"?(\d+)/m)?.[1] || 0)
      const description = block.match(/^\s*Description:\s*(.+)$/m)?.[1]?.trim() || ''
      return { name, channels, description, block }
    })
    .filter((sink) => sink.name)

  const defaultParsed = parsedSinks.find((sink) => sink.name === defaultSink)
  const selectedSink = surround
    ? (defaultParsed?.channels >= 6 && defaultParsed) ||
      parsedSinks.find(
        (sink) =>
          sink.channels >= 6 && /hdmi|surround|5\.1/i.test(`${sink.name} ${sink.description}`)
      ) ||
      parsedSinks.find((sink) => sink.channels >= 6)
    : defaultParsed || parsedSinks[0]

  const deviceCandidates = selectedSink?.name
    ? [`pipewire/${selectedSink.name}`, `pulse/${selectedSink.name}`]
    : ['pipewire', 'pulse', 'auto']
  const device =
    deviceCandidates.find((candidate) =>
      mpvDevices.some((mpvDevice) => mpvDevice.id === candidate)
    ) ||
    deviceCandidates[0] ||
    null

  const result = {
    device,
    sinkName: selectedSink?.name || null,
    channels: selectedSink?.channels || 2,
    surroundCapable: (selectedSink?.channels || 2) >= 6
  }

  nativeSurround.cachedDevice = result
  nativeSurround.cachedDeviceAt = Date.now()
  return result
}

function appendNativeSurroundLog(line) {
  if (!line) return
  nativeSurround.stderrTail = `${nativeSurround.stderrTail}
${line}`
    .trim()
    .slice(-4000)
}

function broadcast(channel, payload) {
  BrowserWindow.getAllWindows().forEach((win) => {
    if (!win.isDestroyed()) win.webContents.send(channel, payload)
  })
}

function waitForMpvReady(child, ipcPath, timeoutMs = 2000) {
  return new Promise((resolve, reject) => {
    const startedAt = Date.now()
    let settled = false

    const cleanup = () => {
      settled = true
      child.off('exit', onExit)
      child.off('error', onError)
    }

    const onExit = (code, signal) => {
      if (settled) return
      cleanup()
      reject(
        new Error(
          `mpv exited before IPC became ready: code=${code}, signal=${signal || 'none'}${nativeSurround.stderrTail ? `\n${nativeSurround.stderrTail}` : ''}`
        )
      )
    }

    const onError = (err) => {
      if (settled) return
      cleanup()
      reject(err)
    }

    const probe = () => {
      if (settled) return
      const socket = net.createConnection(ipcPath)
      socket.once('connect', () => {
        socket.end()
        cleanup()
        resolve()
      })
      socket.once('error', () => {
        socket.destroy()
        if (Date.now() - startedAt >= timeoutMs) {
          cleanup()
          reject(
            new Error(
              `mpv IPC socket was not ready at ${ipcPath}${nativeSurround.stderrTail ? `\n${nativeSurround.stderrTail}` : ''}`
            )
          )
        } else {
          setTimeout(probe, 75).unref?.()
        }
      })
    }

    child.once('exit', onExit)
    child.once('error', onError)
    probe()
  })
}

function sendMpvCommand(command, { expectResponse = false } = {}) {
  if (!nativeSurround.child || !nativeSurround.ipcPath) return Promise.resolve(false)
  if (nativeSurround.child.exitCode !== null || nativeSurround.child.signalCode !== null) {
    return Promise.resolve(false)
  }

  return new Promise((resolve) => {
    const socket = net.createConnection(nativeSurround.ipcPath)
    let settled = false
    let timer = null
    let response = ''

    const finish = (ok, err = null) => {
      if (settled) return
      settled = true
      if (timer) clearTimeout(timer)
      socket.end()
      socket.destroy()
      if (err) {
        nativeSurround.lastError = err.message
        console.warn('[NativeSurround] IPC command failed:', err.message)
      }
      if (!expectResponse) {
        resolve(ok)
        return
      }
      if (!ok) {
        resolve(null)
        return
      }
      try {
        resolve(JSON.parse(response.trim().split('\n').filter(Boolean).at(-1) || '{}'))
      } catch {
        resolve(null)
      }
    }

    socket.once('connect', () => {
      socket.write(`${JSON.stringify({ command })}\n`, (err) => {
        if (err) finish(false, err)
      })
      timer = setTimeout(() => finish(true), 500)
      timer.unref?.()
    })
    socket.on('data', (chunk) => {
      response += chunk.toString()
      finish(true)
    })
    socket.once('end', () => finish(true))
    socket.once('error', (err) => finish(false, err))
  })
}

function getSinkNameFromMpvDevice(device) {
  const name = String(device || '').replace(/^(pipewire|pulse)\//, '')
  return name && !['auto', 'pipewire', 'pulse'].includes(name) ? name : null
}

function stopNativeAudioMonitor() {
  const monitor = nativeSurround.monitorChild
  nativeSurround.monitorChild = null
  nativeSurround.monitorSource = null
  if (monitor && monitor.exitCode === null && monitor.signalCode === null) {
    monitor.kill('SIGTERM')
    setTimeout(() => {
      if (monitor.exitCode === null && monitor.signalCode === null) monitor.kill('SIGKILL')
    }, 500).unref?.()
  }
}

function startNativeAudioMonitor(device) {
  stopNativeAudioMonitor()
  const sinkName = getSinkNameFromMpvDevice(device)
  if (!sinkName) return

  const monitorSource = `${sinkName}.monitor`
  const child = spawn(
    'parec',
    [
      `--device=${monitorSource}`,
      '--format=s16le',
      '--rate=44100',
      '--channels=2',
      '--latency-msec=35'
    ],
    {
      stdio: ['ignore', 'pipe', 'pipe'],
      windowsHide: true
    }
  )

  nativeSurround.monitorChild = child
  nativeSurround.monitorSource = monitorSource

  child.stdout.on('data', (chunk) => {
    const arrayBuffer = chunk.buffer.slice(chunk.byteOffset, chunk.byteOffset + chunk.byteLength)
    broadcast('native-surround-audio-data', {
      sampleRate: 44100,
      channels: 2,
      format: 's16le',
      buffer: arrayBuffer
    })
  })

  child.stderr.on('data', (chunk) => {
    const line = chunk.toString().trim()
    if (line) console.warn('[NativeSurround/monitor]', line)
  })

  child.on('exit', (code, signal) => {
    if (nativeSurround.monitorChild === child) {
      nativeSurround.monitorChild = null
      nativeSurround.monitorSource = null
    }
    if (code || signal) console.warn('[NativeSurround] monitor exited', { code, signal })
  })
}

function stopNativePositionPolling() {
  if (nativeSurround.positionTimer) clearInterval(nativeSurround.positionTimer)
  nativeSurround.positionTimer = null
}

function startNativePositionPolling() {
  stopNativePositionPolling()
  nativeSurround.positionTimer = setInterval(async () => {
    if (!nativeSurround.child || nativeSurround.paused) return
    const result = await sendMpvCommand(['get_property', 'time-pos'], { expectResponse: true })
    if (result && result.error === 'success' && Number.isFinite(result.data)) {
      broadcast('native-surround-position', { time: result.data, file: nativeSurround.file })
    }
  }, 250)
  nativeSurround.positionTimer.unref?.()
}

async function killKnownMagnetofonMpvChildren() {
  try {
    const tempRoot = app.getPath('temp')
    const entries = await fs.readdir(tempRoot)
    for (const entry of entries) {
      if (entry.startsWith('magnetofon-mpv-')) {
        await fs.rm(path.join(tempRoot, entry), { recursive: true, force: true }).catch(() => {})
      }
    }
  } catch {
    // Ignore temp cleanup errors on startup
  }
}

function waitForChildExit(child, timeoutMs = 1500) {
  return new Promise((resolve) => {
    if (!child || child.exitCode !== null || child.signalCode !== null) {
      resolve()
      return
    }

    let done = false
    const finish = () => {
      if (done) return
      done = true
      clearTimeout(timer)
      child.off('exit', finish)
      resolve()
    }

    const timer = setTimeout(() => {
      if (!child.killed) child.kill('SIGKILL')
      finish()
    }, timeoutMs)
    timer.unref?.()
    child.once('exit', finish)
  })
}

async function stopNativeSurround() {
  if (nativeSurround.stopPromise) return nativeSurround.stopPromise

  nativeSurround.stopPromise = (async () => {
    const child = nativeSurround.child
    const ipcPath = nativeSurround.ipcPath
    const ipcDir = nativeSurround.ipcDir

    nativeSurround.child = null
    nativeSurround.file = null
    nativeSurround.mode = 'stopping'
    nativeSurround.stopping = true
    nativeSurround.ipcPath = null
    nativeSurround.ipcDir = null
    stopNativePositionPolling()
    stopNativeAudioMonitor()

    if (child) {
      if (ipcPath) {
        await new Promise((resolve) => {
          const socket = net.createConnection(ipcPath)
          let settled = false
          const finish = () => {
            if (settled) return
            settled = true
            socket.destroy()
            resolve()
          }
          socket.once('connect', () => {
            socket.end(`${JSON.stringify({ command: ['quit'] })}\n`, finish)
          })
          socket.once('error', finish)
          setTimeout(finish, 250).unref?.()
        })
      } else if (!child.killed) {
        child.kill('SIGTERM')
      }

      await waitForChildExit(child)
    }

    if (ipcDir) {
      await fs.rm(ipcDir, { recursive: true, force: true }).catch(() => {})
    } else if (ipcPath) {
      await fs.unlink(ipcPath).catch(() => {})
    }

    nativeSurround.mode = 'off'
    nativeSurround.stopping = false
  })()

  try {
    await nativeSurround.stopPromise
  } finally {
    nativeSurround.stopPromise = null
  }
}

async function isSupportedNativeAudioFile(file) {
  try {
    const stat = await fs.stat(String(file || ''))
    return stat.isFile()
  } catch {
    return false
  }
}

function buildMpvAudioFilters({
  surroundMode = 'SURROUND',
  eqEnabled = false,
  preamp = 0,
  eqBands = []
} = {}) {
  const filters = []
  if (surroundMode === 'SURROUND') {
    filters.push(
      'lavfi=[pan=5.1|FL=FL|FR=FR|FC=0.55*FL+0.55*FR|LFE=0.25*FL+0.25*FR|BL=0.45*FR|BR=0.45*FL]'
    )
  }
  if (eqEnabled) {
    const clampedPreamp = Math.max(-12, Math.min(12, Number(preamp) || 0))
    if (clampedPreamp !== 0) {
      filters.push(`volume=gain=${clampedPreamp}`)
    }
    const bandsArray = Array.isArray(eqBands) ? eqBands : []
    const clampedBands = Array.from({ length: 10 }, (_, i) =>
      Math.max(-12, Math.min(12, Number(bandsArray[i]) || 0))
    )
    if (clampedBands.some((b) => b !== 0)) {
      filters.push(`equalizer=${clampedBands.join(':')}`)
    }
  }
  return filters.length > 0 ? filters.join(',') : ''
}

async function doStartNativeSurround({
  file,
  startTime = 0,
  volume = 0.8,
  paused = false,
  surroundMode = 'SURROUND',
  eqEnabled = false,
  preamp = 0,
  eqBands = []
} = {}) {
  if (!file) throw new Error('No file supplied to native surround backend')
  if (!(await isSupportedNativeAudioFile(file))) {
    throw new Error(`Native playback only accepts files: ${path.basename(String(file))}`)
  }

  const requestedMode = normalizePlaybackMode(surroundMode)
  const audioTarget = await detectNativeAudioDevice({ surround: requestedMode !== 'STEREO' })
  const normalizedMode =
    requestedMode === 'AUTO'
      ? audioTarget?.surroundCapable
        ? 'SURROUND'
        : 'STEREO'
      : requestedMode

  if (
    nativeSurround.child &&
    nativeSurround.file === file &&
    nativeSurround.mode === normalizedMode
  ) {
    nativeSurround.volume = Math.round(Math.max(0, Math.min(1, volume)) * 100)
    nativeSurround.paused = Boolean(paused)
    await sendMpvCommand(['set_property', 'volume', nativeSurround.volume])
    await sendMpvCommand(['set_property', 'pause', nativeSurround.paused])
    const afFilter = buildMpvAudioFilters({
      surroundMode: normalizedMode,
      eqEnabled,
      preamp,
      eqBands
    })
    await sendMpvCommand(['set_property', 'af', afFilter])
    if (Number.isFinite(startTime))
      await sendMpvCommand(['seek', Math.max(0, startTime), 'absolute', 'exact'])
    return {
      ok: true,
      reused: true,
      device: nativeSurround.device,
      mode: nativeSurround.mode,
      requestedMode: nativeSurround.requestedMode
    }
  }

  if (nativeSurround.child) await stopNativeSurround()

  const device = audioTarget?.device || null
  const volumePercent = Math.round(Math.max(0, Math.min(1, volume)) * 100)
  const ipcDir = await fs.mkdtemp(path.join(app.getPath('temp'), 'magnetofon-mpv-'))
  const ipcPath = path.join(ipcDir, 'mpv.sock')

  const args = [
    '--no-video',
    '--force-window=no',
    '--input-terminal=no',
    `--input-ipc-server=${ipcPath}`,
    '--gapless-audio=no',
    '--keep-open=no',
    '--msg-level=all=warn,ao=info,af=info',
    `--volume=${volumePercent}`,
    `--start=${Math.max(0, Number(startTime) || 0)}`
  ]

  if (normalizedMode === 'SURROUND') {
    args.push('--audio-channels=5.1,stereo', '--audio-normalize-downmix=no')
  } else {
    args.push('--audio-channels=stereo')
  }

  const afFilter = buildMpvAudioFilters({
    surroundMode: normalizedMode,
    eqEnabled,
    preamp,
    eqBands
  })
  if (afFilter) {
    args.push(`--af=${afFilter}`)
  }

  if (device) args.push(`--audio-device=${device}`)
  if (paused) args.push('--pause')
  args.push('--', file)

  const child = spawn('mpv', args, {
    stdio: ['ignore', 'ignore', 'pipe'],
    windowsHide: true
  })

  nativeSurround.child = child
  nativeSurround.file = file
  nativeSurround.device = device || 'auto'
  nativeSurround.volume = volumePercent
  nativeSurround.paused = Boolean(paused)
  nativeSurround.mode = 'starting'
  nativeSurround.requestedMode = requestedMode
  nativeSurround.lastError = null
  nativeSurround.stderrTail = ''
  nativeSurround.ipcDir = ipcDir
  nativeSurround.ipcPath = ipcPath
  nativeSurround.stopping = false

  child.stderr.on('data', (chunk) => {
    const line = chunk.toString().trim()
    appendNativeSurroundLog(line)
    if (line) console.log('[NativeSurround/mpv]', line)
  })

  child.on('error', (err) => {
    nativeSurround.lastError = err.message
    nativeSurround.mode = 'error'
    console.error('[NativeSurround] mpv failed:', err)
  })

  child.on('exit', (code, signal) => {
    const endedNaturally = !nativeSurround.stopping && nativeSurround.child === child && code === 0
    if (nativeSurround.child === child) {
      const endedFile = nativeSurround.file
      nativeSurround.child = null
      nativeSurround.file = null
      nativeSurround.mode = 'off'
      nativeSurround.ipcPath = null
      stopNativePositionPolling()
      stopNativeAudioMonitor()
      if (endedNaturally) broadcast('native-surround-ended', { file: endedFile })
    }
    fs.unlink(ipcPath).catch(() => {})
    console.log('[NativeSurround] mpv exited', { code, signal })
  })

  try {
    await waitForMpvReady(child, ipcPath)
    nativeSurround.mode = normalizedMode
    startNativeAudioMonitor(nativeSurround.device)
    startNativePositionPolling()
    return {
      ok: true,
      reused: false,
      device: nativeSurround.device,
      monitorSource: nativeSurround.monitorSource,
      mode: nativeSurround.mode,
      requestedMode: nativeSurround.requestedMode,
      surroundCapable: Boolean(audioTarget?.surroundCapable),
      channels: audioTarget?.channels || 2
    }
  } catch (err) {
    nativeSurround.lastError = err.message
    await stopNativeSurround()
    throw err
  }
}

async function startNativeSurround(options = {}) {
  if (nativeSurround.startPromise) await nativeSurround.startPromise.catch(() => {})

  nativeSurround.startPromise = doStartNativeSurround(options)
  try {
    return await nativeSurround.startPromise
  } finally {
    nativeSurround.startPromise = null
  }
}

async function loadSettings() {
  try {
    const data = await fs.readFile(settingsPath, 'utf8')
    return JSON.parse(data)
  } catch {
    return {
      volume: 0.8,
      eqEnabled: false,
      preamp: -3,
      eqBands: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      surroundMode: 'AUTO'
    }
  }
}

async function saveSettings(settings) {
  try {
    const current = await loadSettings()
    const updated = {
      ...current,
      volume: typeof settings.volume === 'number' ? settings.volume : current.volume,
      eqEnabled: typeof settings.eqEnabled === 'boolean' ? settings.eqEnabled : current.eqEnabled,
      preamp: typeof settings.preamp === 'number' ? settings.preamp : current.preamp,
      eqBands: Array.isArray(settings.eqBands) ? settings.eqBands : current.eqBands,
      surroundMode: normalizePlaybackMode(settings.surroundMode || current.surroundMode)
    }
    await fs.writeFile(settingsPath, JSON.stringify(updated, null, 2), 'utf8')
  } catch (e) {
    console.error('[Main] Failed to save settings:', e)
  }
}

async function loadVisualsSettings() {
  try {
    const data = await fs.readFile(visualsSettingsPath, 'utf8')
    return JSON.parse(data)
  } catch {
    return { curatedPresets: {} }
  }
}

async function saveVisualsSettings(settings) {
  try {
    const current = await loadVisualsSettings()
    const updated = { ...current, ...settings }
    await fs.writeFile(visualsSettingsPath, JSON.stringify(updated, null, 2), 'utf8')
  } catch (e) {
    console.error('[Main] Failed to save visuals settings:', e)
  }
}

function presetIdVariants(filename) {
  const safe = String(filename || '').trim()
  const withoutExt = safe.replace(/\.milk$/i, '').trim()
  return [...new Set([safe, withoutExt, withoutExt ? `${withoutExt}.milk` : ''].filter(Boolean))]
}

async function listMilkdropPresetFiles(rootDir) {
  const files = []

  async function walk(dir) {
    let entries = []
    try {
      entries = await fs.readdir(dir, { withFileTypes: true })
    } catch {
      return
    }

    for (const entry of entries) {
      if (entry.name === 'discarded' || entry.name.startsWith('.')) continue
      const fullPath = path.join(dir, entry.name)
      if (entry.isDirectory()) {
        await walk(fullPath)
      } else if (entry.isFile() && entry.name.toLowerCase().endsWith('.milk')) {
        files.push(path.relative(rootDir, fullPath).split(path.sep).join('/'))
      }
    }
  }

  await walk(rootDir)
  return files.sort((a, b) => a.localeCompare(b))
}

function resolvePresetPath(rootDir, filename) {
  const relativeName = String(filename || '')
    .trim()
    .replace(/^[/\\]+/, '')
  const normalized = path.normalize(relativeName)

  if (!normalized || normalized.includes('..') || !normalized.toLowerCase().endsWith('.milk')) {
    throw new Error('A local .milk preset path is required')
  }

  const resolvedRoot = path.resolve(rootDir)
  const resolvedPath = path.resolve(rootDir, normalized)
  if (resolvedPath !== resolvedRoot && !resolvedPath.startsWith(resolvedRoot + path.sep)) {
    throw new Error('Preset path escapes preset library')
  }

  return {
    filePath: resolvedPath,
    relativePath: path.relative(rootDir, resolvedPath).split(path.sep).join('/')
  }
}

function extractTextureNames(milkContent) {
  const textureRegex = /([-a-zA-Z0-9_.() ]+\.(jpg|jpeg|png|tga|dds))/gi
  return [...new Set(String(milkContent || '').match(textureRegex) || [])].map((tex) =>
    require('path').basename(tex)
  )
}

async function uniqueDestinationPath(destPath) {
  const path = require('path')
  const exists = require('fs').existsSync
  if (!exists(destPath)) return destPath

  const ext = path.extname(destPath)
  const base = destPath.slice(0, -ext.length)
  let i = 2
  let candidate = `${base} (${i})${ext}`
  while (exists(candidate)) {
    i += 1
    candidate = `${base} (${i})${ext}`
  }
  return candidate
}

function isWindowUsable(win) {
  return !!win && !win.isDestroyed()
}

function isWindowVisible(name) {
  const win = windows[name]
  return isWindowUsable(win) && win.isVisible()
}

function getDockedBounds(name) {
  if (!isWindowUsable(windows.player)) return null

  const playerBounds = windows.player.getBounds()
  const eqVisible = isWindowVisible('equalizer')
  const eqWidth = eqVisible ? windows.equalizer.getBounds().width : windowConfigs.equalizer.width

  if (name === 'equalizer') {
    return {
      x: playerBounds.x + playerBounds.width,
      y: playerBounds.y,
      width: isWindowUsable(windows.equalizer)
        ? windows.equalizer.getBounds().width
        : windowConfigs.equalizer.width,
      height: playerBounds.height
    }
  }

  if (name === 'playlist') {
    return {
      x: playerBounds.x,
      y: playerBounds.y + playerBounds.height,
      width: playerBounds.width + (eqVisible ? eqWidth : 0),
      height: isWindowUsable(windows.playlist)
        ? windows.playlist.getBounds().height
        : windowConfigs.playlist.height
    }
  }

  if (name === 'visuals') {
    return {
      x: playerBounds.x + 40,
      y: playerBounds.y + 40,
      width: windowConfigs.visuals.width,
      height: windowConfigs.visuals.height
    }
  }

  return null
}

function projectMBundleKey() {
  const arch = process.arch === 'x64' ? 'x64' : process.arch
  return `${process.platform}-${arch}`
}

function getProjectMRuntimeRoot() {
  const relative = path.join('projectm', projectMBundleKey())
  if (app.isPackaged) return path.join(process.resourcesPath, relative)
  return path.join(process.cwd(), 'resources', relative)
}

function getProjectMExecutable() {
  const runtimeRoot = getProjectMRuntimeRoot()
  const exeName = process.platform === 'win32' ? 'projectMSDL.exe' : 'projectMSDL'
  const executable = path.join(runtimeRoot, 'bin', exeName)
  return existsSync(executable) ? executable : null
}

function firstExistingDir(paths) {
  return paths.find((candidate) => candidate && existsSync(candidate)) || null
}

const PRESET_CATEGORIES = {
  all: 'All Categories',
  dancer: 'Dancer',
  drawing: 'Drawing',
  fractal: 'Fractal',
  geometric: 'Geometric',
  hypnotic: 'Hypnotic',
  particles: 'Particles',
  reaction: 'Reaction',
  sparkle: 'Sparkle',
  supernova: 'Supernova',
  waveform: 'Waveform'
}

function getProjectMPaths(presetSource = 'main', presetCategory = 'all') {
  const runtimeRoot = getProjectMRuntimeRoot()
  const userDataPresetsDir = path.join(app.getPath('userData'), 'visuals', 'presets')
  const userDataTexturesDir = path.join(app.getPath('userData'), 'visuals', 'textures')
  const localPresetsDir = path.join(process.cwd(), 'visuals', 'presets')
  const curatedPresetsDir = path.join(process.cwd(), 'visuals', 'curated', 'presets')
  const localTexturesDir = path.join(process.cwd(), 'visuals', 'textures')
  const curatedTexturesDir = path.join(process.cwd(), 'visuals', 'curated', 'textures')
  const bundledPresetsDir = path.join(runtimeRoot, 'presets')
  const bundledTexturesDir = path.join(runtimeRoot, 'textures')

  const basePresetOrder =
    presetSource === 'curated'
      ? [curatedPresetsDir, userDataPresetsDir, localPresetsDir, bundledPresetsDir]
      : [userDataPresetsDir, localPresetsDir, curatedPresetsDir, bundledPresetsDir]

  const textureOrder =
    presetSource === 'curated'
      ? [curatedTexturesDir, userDataTexturesDir, localTexturesDir, bundledTexturesDir]
      : [userDataTexturesDir, localTexturesDir, curatedTexturesDir, bundledTexturesDir]

  let presetOrder = basePresetOrder

  if (presetCategory && presetCategory !== 'all') {
    const subfolder = PRESET_CATEGORIES[presetCategory.toLowerCase()] || presetCategory
    const categoryCandidates = basePresetOrder.map((dir) => path.join(dir, subfolder))
    const validCategoryPath = firstExistingDir(categoryCandidates)
    if (validCategoryPath) {
      presetOrder = [validCategoryPath, ...basePresetOrder]
    }
  }

  return {
    presetPath: firstExistingDir(presetOrder),
    texturePath: firstExistingDir(textureOrder)
  }
}

async function detectProjectMAudioDevice({ force = false } = {}) {
  if (process.platform !== 'linux') return null
  if (
    !force &&
    nativeProjectM.cachedAudioDevice &&
    Date.now() - nativeProjectM.cachedAudioDeviceAt < 30000
  ) {
    return nativeProjectM.cachedAudioDevice
  }

  const [info, sources] = await Promise.all([
    execFileText('pactl', ['info']),
    execFileText('pactl', ['list', 'sources'])
  ])

  if (!sources.ok) return nativeProjectM.cachedAudioDevice || null

  const defaultSink = info.stdout.match(/^Default Sink:\s*(.+)$/m)?.[1]?.trim()
  const defaultMonitor = defaultSink ? `${defaultSink}.monitor` : null
  const sourceBlocks = sources.stdout.split(/\n(?=Source #)/g)
  const parsedSources = sourceBlocks
    .map((block) => {
      const name = block.match(/^\s*Name:\s*(.+)$/m)?.[1]?.trim()
      const description = block.match(/^\s*Description:\s*(.+)$/m)?.[1]?.trim() || ''
      return { name, description }
    })
    .filter((source) => source.name)

  const selectedSource =
    parsedSources.find((source) => source.name === defaultMonitor) ||
    parsedSources.find(
      (source) =>
        /\.monitor$/.test(source.name) &&
        /hdmi|surround|5\.1/i.test(`${source.name} ${source.description}`)
    ) ||
    parsedSources.find((source) => /\.monitor$/.test(source.name))

  const device = selectedSource?.description || selectedSource?.name || null
  nativeProjectM.cachedAudioDevice = device
  nativeProjectM.cachedAudioDeviceAt = Date.now()
  return device
}

async function listAudioDevices() {
  if (process.platform !== 'linux') return []
  try {
    const sources = await execFileText('pactl', ['list', 'sources'])
    if (!sources.ok) return []
    const sourceBlocks = sources.stdout.split(/\n(?=Source #)/g)
    return sourceBlocks
      .map((block) => {
        const name = block.match(/^\s*Name:\s*(.+)$/m)?.[1]?.trim()
        const description = block.match(/^\s*Description:\s*(.+)$/m)?.[1]?.trim() || ''
        return { name, description }
      })
      .filter((source) => source.name)
  } catch {
    return []
  }
}

function isNativeProjectMRunning() {
  return Boolean(nativeProjectM.child && !nativeProjectM.child.killed)
}

async function stopNativeProjectM() {
  const child = nativeProjectM.child
  if (!child) return
  nativeProjectM.child = null
  try {
    if (!child.killed) {
      child.kill('SIGTERM')
      await waitForChildExit(child, 1500)
    }
  } catch (err) {
    console.warn('[projectM] Could not stop helper:', err)
  }
}

async function launchNativeProjectM({ restartIfRunning = false } = {}) {
  const isRunning = isNativeProjectMRunning()
  if (isRunning) {
    await stopNativeProjectM()
    if (!restartIfRunning) return true
    await new Promise((resolve) => setTimeout(resolve, 250))
  }

  const executable = getProjectMExecutable()
  if (!executable) {
    console.warn(`[projectM] Bundled helper missing for ${projectMBundleKey()}`)
    return false
  }

  const settings = await loadVisualsSettings()
  const presetSource = settings.presetSource || 'main'
  const { presetPath, texturePath } = getProjectMPaths(presetSource)

  const args = []
  if (presetPath) args.push('--presetPath', presetPath)
  if (texturePath) args.push('--texturePath', texturePath)

  const audioDeviceSetting = settings.audioDevice
  if (audioDeviceSetting && audioDeviceSetting !== 'auto' && audioDeviceSetting !== 'default') {
    args.push('--audioDevice', audioDeviceSetting)
  } else {
    const detected = await detectProjectMAudioDevice()
    if (detected) args.push('--audioDevice', detected)
  }

  // Preset Duration
  const presetDuration = Number(settings.presetDuration) || 30
  args.push('--presetDuration', String(presetDuration))

  // Transition Duration
  const transitionDuration =
    settings.transitionDuration !== undefined ? Number(settings.transitionDuration) : 3
  args.push('--transitionDuration', String(transitionDuration))

  // Shuffle Mode
  if (settings.shuffleEnabled !== false) {
    args.push('--shuffleEnabled', '1')
  } else {
    args.push('--shuffleEnabled', '0')
  }

  // FPS
  if (settings.fps) {
    args.push('--fps', String(settings.fps))
  }

  // Music Reactivity: Beat Sensitivity (0.0 to 2.0)
  if (settings.beatSensitivity !== undefined) {
    args.push('--beatSensitivity', String(settings.beatSensitivity))
  }

  // Music Reactivity: Hard Cuts
  if (settings.hardCutsEnabled) {
    args.push('--hardCutsEnabled', '1')
    if (settings.hardCutSensitivity !== undefined) {
      args.push('--hardCutSensitivity', String(settings.hardCutSensitivity))
    }
    if (settings.hardCutDuration !== undefined) {
      args.push('--hardCutDuration', String(settings.hardCutDuration))
    }
  } else if (settings.hardCutsEnabled === false) {
    args.push('--hardCutsEnabled', '0')
  }

  // Fullscreen Mode
  if (settings.fullscreen) {
    args.push('--fullscreen', '1')
  }

  console.log('[projectM] Launching bundled helper with settings:', executable, args)
  nativeProjectM.stderrTail = ''
  nativeProjectM.lastError = null

  const runtimeRoot = getProjectMRuntimeRoot()
  const bundledLibPath = path.join(runtimeRoot, 'lib')
  const child = spawn(executable, args, {
    cwd: path.dirname(executable),
    stdio: ['ignore', 'ignore', 'pipe'],
    env: {
      ...process.env,
      LD_LIBRARY_PATH: [bundledLibPath, process.env.LD_LIBRARY_PATH].filter(Boolean).join(':'),
      MAGNETOFON_PROJECTM_PRESET_PATH: presetPath || '',
      MAGNETOFON_PROJECTM_TEXTURE_PATH: texturePath || ''
    }
  })

  nativeProjectM.child = child

  child.stderr?.on('data', (chunk) => {
    nativeProjectM.stderrTail = `${nativeProjectM.stderrTail}${chunk.toString()}`.slice(-4000)
  })

  child.on('error', (err) => {
    nativeProjectM.lastError = err.message
    console.error('[projectM] helper failed:', err)
    if (nativeProjectM.child === child) nativeProjectM.child = null
  })

  child.on('exit', (code, signal) => {
    console.log('[projectM] helper exited:', { code, signal })
    if (nativeProjectM.child === child) nativeProjectM.child = null
  })

  return true
}

let isRelayoutingDockedWindows = false

function setBoundsIfChanged(win, nextBounds) {
  if (!isWindowUsable(win) || !nextBounds) return
  const current = win.getBounds()
  if (
    current.x === nextBounds.x &&
    current.y === nextBounds.y &&
    current.width === nextBounds.width &&
    current.height === nextBounds.height
  ) {
    return
  }
  win.setBounds(nextBounds)
}

function relayoutDockedWindows({ syncSizes = true } = {}) {
  if (isRelayoutingDockedWindows) return
  isRelayoutingDockedWindows = true

  try {
    if (isWindowVisible('equalizer')) {
      const eqBounds = getDockedBounds('equalizer')
      setBoundsIfChanged(windows.equalizer, eqBounds)
    }

    if (isWindowVisible('playlist')) {
      const playlistBounds = getDockedBounds('playlist')
      if (!syncSizes && playlistBounds && isWindowUsable(windows.playlist)) {
        playlistBounds.width = windows.playlist.getBounds().width
        playlistBounds.height = windows.playlist.getBounds().height
      }
      setBoundsIfChanged(windows.playlist, playlistBounds)
    }
  } finally {
    isRelayoutingDockedWindows = false
  }
}

// OS Level Bounding Box Snapping
const SNAP_THRESHOLD = 15
function handleSnapping(movedWin) {
  const bounds = movedWin.getBounds()
  let snapX = bounds.x
  let snapY = bounds.y
  let snapped = false

  Object.values(windows).forEach((win) => {
    if (!win || win === movedWin || !win.isVisible() || win.isDestroyed()) return

    const targetBounds = win.getBounds()

    // Check vertical snapping (edges aligning left or right)
    const isVerticallyAligned =
      bounds.y + bounds.height > targetBounds.y - SNAP_THRESHOLD &&
      bounds.y < targetBounds.y + targetBounds.height + SNAP_THRESHOLD

    // Check horizontal snapping (edges aligning top or bottom)
    const isHorizontallyAligned =
      bounds.x + bounds.width > targetBounds.x - SNAP_THRESHOLD &&
      bounds.x < targetBounds.x + targetBounds.width + SNAP_THRESHOLD

    if (isVerticallyAligned) {
      if (Math.abs(bounds.x + bounds.width - targetBounds.x) < SNAP_THRESHOLD) {
        snapX = targetBounds.x - bounds.width
        snapped = true
      } else if (Math.abs(bounds.x - (targetBounds.x + targetBounds.width)) < SNAP_THRESHOLD) {
        snapX = targetBounds.x + targetBounds.width
        snapped = true
      } else if (Math.abs(bounds.x - targetBounds.x) < SNAP_THRESHOLD) {
        snapX = targetBounds.x
        snapped = true
      }
    }

    if (isHorizontallyAligned) {
      if (Math.abs(bounds.y + bounds.height - targetBounds.y) < SNAP_THRESHOLD) {
        snapY = targetBounds.y - bounds.height
        snapped = true
      } else if (Math.abs(bounds.y - (targetBounds.y + targetBounds.height)) < SNAP_THRESHOLD) {
        snapY = targetBounds.y + targetBounds.height
        snapped = true
      } else if (Math.abs(bounds.y - targetBounds.y) < SNAP_THRESHOLD) {
        snapY = targetBounds.y
        snapped = true
      }
    }
  })

  if (snapped) {
    movedWin.setBounds({ x: snapX, y: snapY, width: bounds.width, height: bounds.height })
  }
}

function createChildWindow(name, initX, initY) {
  if (isWindowUsable(windows[name])) {
    if (windows[name].isVisible()) {
      windows[name].hide()
      if (name === 'equalizer' || name === 'playlist') relayoutDockedWindows()
    } else {
      const nextBounds =
        name === 'visuals'
          ? windows[name].getBounds()
          : getDockedBounds(name) || {
              x: initX,
              y: initY,
              width: windowConfigs[name].width,
              height: windowConfigs[name].height
            }
      windows[name].setBounds(nextBounds)
      windows[name].show()
      if (name === 'equalizer' || name === 'playlist') relayoutDockedWindows()
    }
    return
  }

  const fallbackBounds = {
    x: initX,
    y: initY,
    width: windowConfigs[name].width,
    height: windowConfigs[name].height
  }
  const initialBounds = getDockedBounds(name) || fallbackBounds

  windows[name] = new BrowserWindow({
    width: initialBounds.width,
    height: initialBounds.height,
    x: initialBounds.x,
    y: initialBounds.y,
    minWidth: windowConfigs[name].minWidth,
    minHeight: windowConfigs[name].minHeight,
    show: false,
    title: windowConfigs[name].title,
    frame: false,
    transparent: name !== 'visuals',
    resizable: name === 'visuals' || name === 'playlist',
    maximizable: name === 'visuals',
    skipTaskbar: true,
    autoHideMenuBar: true,
    parent: name === 'visuals' ? undefined : windows.player || undefined,
    icon,
    webPreferences: {
      preload: join(__dirname, '../preload/index.js'),
      sandbox: false,
      contextIsolation: true,
      nodeIntegration: false,
      webSecurity: true
    }
  })

  windows[name].on('ready-to-show', () => {
    console.log(`[Window] ready-to-show: ${name}`)
    windows[name].show()
    if (name === 'equalizer' || name === 'playlist') relayoutDockedWindows()
  })

  windows[name].on('move', () => {
    if (isRelayoutingDockedWindows) return
    handleSnapping(windows[name])
  })

  windows[name].on('resize', () => {
    if (isRelayoutingDockedWindows) return
    if (name === 'equalizer') relayoutDockedWindows({ syncSizes: false })
  })

  windows[name].on('show', () => {
    if (name === 'equalizer' || name === 'playlist') relayoutDockedWindows()
  })

  windows[name].on('hide', () => {
    if (name === 'equalizer' || name === 'playlist') relayoutDockedWindows()
  })

  windows[name].webContents.setWindowOpenHandler((details) => {
    safeOpenExternal(details.url)
    return { action: 'deny' }
  })

  if (is.dev && process.env['ELECTRON_RENDERER_URL']) {
    const targetUrl = `${process.env['ELECTRON_RENDERER_URL']}#${name}`
    console.log(`[Window] loadURL ${name}: ${targetUrl}`)
    windows[name].loadURL(targetUrl)
  } else {
    windows[name].loadFile(join(__dirname, '../renderer/index.html'), { hash: name })
  }

  windows[name].on('closed', () => {
    windows[name] = null
    if (name === 'equalizer' || name === 'playlist') relayoutDockedWindows()
  })
}

function safeOpenExternal(rawUrl) {
  try {
    const parsed = new URL(rawUrl)
    if (parsed.protocol === 'http:' || parsed.protocol === 'https:') {
      shell.openExternal(rawUrl)
    } else {
      console.warn('[Security] Blocked non-http(s) external URL:', rawUrl)
    }
  } catch (err) {
    console.warn('[Security] Invalid URL format:', rawUrl, err)
  }
}

function createMainWindow() {
  windows.player = new BrowserWindow({
    width: windowConfigs.player.width,
    height: windowConfigs.player.height,
    show: false,
    frame: false,
    transparent: true,
    backgroundColor: '#00000000',
    resizable: true,
    minWidth: windowConfigs.player.width,
    minHeight: windowConfigs.player.height,
    autoHideMenuBar: true,
    title: 'Magnetofon',
    icon,
    webPreferences: {
      preload: join(__dirname, '../preload/index.js'),
      sandbox: false,
      contextIsolation: true,
      nodeIntegration: false,
      webSecurity: true
    }
  })

  windows.player.on('ready-to-show', () => {
    windows.player.show()
  })

  windows.player.on('move', () => {
    handleSnapping(windows.player)
    relayoutDockedWindows()
  })

  windows.player.on('closed', () => {
    app.quit()
  })

  windows.player.webContents.setWindowOpenHandler((details) => {
    safeOpenExternal(details.url)
    return { action: 'deny' }
  })

  if (is.dev && process.env['ELECTRON_RENDERER_URL']) {
    windows.player.loadURL(`${process.env['ELECTRON_RENDERER_URL']}#player`)
  } else {
    windows.player.loadFile(join(__dirname, '../renderer/index.html'), { hash: 'player' })
  }
}

app.whenReady().then(() => {
  electronApp.setAppUserModelId('com.magnetofon.app')
  killKnownMagnetofonMpvChildren().catch((err) => {
    console.warn('[NativeSurround] stale mpv cleanup failed:', err)
  })

  app.on('browser-window-created', (_, window) => {
    optimizer.watchWindowShortcuts(window)
  })

  ipcMain.on('debug-log', (event, ...args) => {
    console.log('[RendererDebug]', ...args)
  })

  ipcMain.on('toggle-window', async (e, name) => {
    if (!windows.player) return
    const ALLOWED_WINDOWS = ['player', 'playlist', 'equalizer', 'visuals']
    if (!ALLOWED_WINDOWS.includes(name)) return

    const playerBounds = windows.player.getBounds()
    const fallbackBounds = getDockedBounds(name) || {
      x: playerBounds.x,
      y: playerBounds.y + playerBounds.height,
      width: windowConfigs[name]?.width || 500,
      height: windowConfigs[name]?.height || 400
    }

    if (name === 'visuals') {
      await launchNativeProjectM()
      return
    }

    createChildWindow(name, fallbackBounds.x, fallbackBounds.y)
  })

  ipcMain.on('close-current-window', (event) => {
    const win = BrowserWindow.fromWebContents(event.sender)
    win?.close()
  })

  ipcMain.on('toggle-fullscreen', (event) => {
    const win = BrowserWindow.fromWebContents(event.sender)
    if (win) {
      if (win.isFullScreen()) {
        win.setFullScreen(false)
      } else {
        // Clear constraints temporarily to ensure WM allows expansion
        win.setMinimumSize(0, 0)
        win.setMaximumSize(99999, 99999)
        win.setFullScreen(true)
      }
    }
  })

  createMainWindow()

  app.on('activate', function () {
    if (BrowserWindow.getAllWindows().length === 0) createMainWindow()
  })

  protocol.registerFileProtocol('local', (request, callback) => {
    try {
      let urlPath = request.url.replace(/^local:\/\/app\//, '')
      urlPath = decodeURIComponent(urlPath)
      const normalized = path.normalize(urlPath)
      if (!path.isAbsolute(normalized)) {
        return callback({ error: -6 })
      }
      const resolved = path.resolve(normalized)
      const allowedRoots = [
        app.getPath('userData'),
        app.getPath('music'),
        app.getPath('downloads'),
        app.getPath('desktop'),
        process.cwd()
      ].map((r) => path.resolve(r))

      const isAllowed = allowedRoots.some(
        (root) => resolved === root || resolved.startsWith(root + path.sep)
      )

      if (isAllowed && existsSync(resolved)) {
        return callback({ path: resolved })
      }
      return callback({ error: -6 })
    } catch (error) {
      console.error('[Protocol] File decode error:', error)
      return callback({ error: -6 })
    }
  })

  let lastSavedStateJson = ''
  let lastSavedVisualsJson = ''
  ipcMain.on('sync-state', (event, state) => {
    // Broadcast state to all other windows
    BrowserWindow.getAllWindows().forEach((win) => {
      if (win.webContents !== event.sender) {
        win.webContents.send('state-synced', state)
      }
    })

    // Persist relevant settings only when changed (avoids disk write spam during playback)
    const settingsToSave = {
      volume: state.volume,
      eqEnabled: state.eqEnabled,
      preamp: state.preamp,
      eqBands: state.eqBands,
      surroundMode: state.surroundMode
    }
    const settingsJson = JSON.stringify(settingsToSave)
    if (settingsJson !== lastSavedStateJson) {
      lastSavedStateJson = settingsJson
      saveSettings(settingsToSave)
    }

    if (state.curatedPresets) {
      const curatedJson = JSON.stringify(state.curatedPresets)
      if (curatedJson !== lastSavedVisualsJson) {
        lastSavedVisualsJson = curatedJson
        saveVisualsSettings({ curatedPresets: state.curatedPresets })
      }
    }
  })

  ipcMain.handle('get-visuals-settings', async () => {
    return await loadVisualsSettings()
  })

  ipcMain.handle('save-visuals-settings', async (_event, settings) => {
    await saveVisualsSettings(settings)
    if (isNativeProjectMRunning()) {
      await launchNativeProjectM({ restartIfRunning: true })
    }
    return { ok: true }
  })

  ipcMain.handle('restart-native-projectm', async () => {
    return await launchNativeProjectM({ restartIfRunning: true })
  })

  ipcMain.handle('stop-native-projectm', async () => {
    stopNativeProjectM()
    return true
  })

  ipcMain.handle('get-visuals-status', async () => {
    return {
      isRunning: isNativeProjectMRunning(),
      activeDevice: nativeProjectM.cachedAudioDevice
    }
  })

  ipcMain.handle('get-audio-devices', async () => {
    return await listAudioDevices()
  })

  ipcMain.handle('get-clipboard-text', () => {
    const { clipboard } = require('electron')
    return clipboard.readText()
  })

  ipcMain.handle('get-settings', async () => {
    return await loadSettings()
  })

  ipcMain.handle('open-file', async () => {
    const { dialog } = require('electron')
    const result = await dialog.showOpenDialog({
      properties: ['openFile', 'multiSelections'],
      filters: [{ name: 'Audio', extensions: ['mp3', 'flac', 'ogg', 'wav', 'm4a', 'aac'] }]
    })
    if (result.canceled) return null
    return result.filePaths
  })

  ipcMain.handle('parse-metadata', async (event, filePath) => {
    if (!filePath) {
      console.error('parse-metadata: filePath is undefined')
      return null
    }
    try {
      const mm = await import('music-metadata')
      const metadata = await mm.parseFile(filePath)

      let pictureUrl = null
      if (metadata.common.picture && metadata.common.picture.length > 0) {
        const pic = metadata.common.picture[0]
        const mime = pic.format || 'image/jpeg'
        pictureUrl = `data:${mime};base64,${pic.data.toString('base64')}`
      }

      if (!pictureUrl) {
        try {
          const dir = path.dirname(filePath)
          const files = await fs.readdir(dir)
          const imagePatterns = [
            /^cover\.(jpe?g|png|webp)$/i,
            /^folder\.(jpe?g|png|webp)$/i,
            /^album\.(jpe?g|png|webp)$/i,
            /^front\.(jpe?g|png|webp)$/i,
            /^art\.(jpe?g|png|webp)$/i,
            /\.(jpe?g|png|webp)$/i
          ]

          let foundFile = null
          for (const pattern of imagePatterns) {
            const match = files.find((f) => pattern.test(f))
            if (match) {
              foundFile = path.join(dir, match)
              break
            }
          }

          if (foundFile) {
            const buf = await fs.readFile(foundFile)
            const ext = path.extname(foundFile).toLowerCase().replace('.', '')
            const mime = ext === 'png' ? 'image/png' : ext === 'webp' ? 'image/webp' : 'image/jpeg'
            pictureUrl = `data:${mime};base64,${buf.toString('base64')}`
          }
        } catch (dirErr) {
          console.warn('[Metadata] Directory artwork scan warning:', dirErr)
        }
      }

      return {
        title: metadata.common.title,
        artist: metadata.common.artist,
        album: metadata.common.album,
        year: metadata.common.year,
        trackNo: metadata.common.track?.no,
        duration: metadata.format.duration,
        format: metadata.format.container || metadata.format.codec,
        codec: metadata.format.codec,
        sampleRate: metadata.format.sampleRate,
        bitrate: metadata.format.bitrate,
        bitsPerSample: metadata.format.bitsPerSample,
        channels: metadata.format.numberOfChannels,
        picture: pictureUrl
      }
    } catch (err) {
      console.error('Metadata parse error', err)
      return null
    }
  })

  // Visuals specific IPC
  ipcMain.on('visuals-audio-data', (event, data) => {
    if (windows.visuals && !windows.visuals.isDestroyed()) {
      windows.visuals.webContents.send('visuals-audio-data', data)
    }
  })

  ipcMain.handle('get-presets-list', async (_event, mode = 'inbox') => {
    const exists = require('fs').existsSync
    const libraryMode = ['inbox', 'curated', 'all'].includes(mode) ? mode : 'inbox'
    const devPresetsDir = path.join(process.cwd(), 'visuals', 'presets')
    const curatedPresetsDir = path.join(process.cwd(), 'visuals', 'curated', 'presets')

    if (!exists(devPresetsDir) && !exists(curatedPresetsDir)) {
      console.warn('[Visuals] Preset directories NOT found')
      return []
    }

    try {
      const [localFiles, curatedFiles] = await Promise.all([
        exists(devPresetsDir) ? listMilkdropPresetFiles(devPresetsDir) : Promise.resolve([]),
        exists(curatedPresetsDir) ? listMilkdropPresetFiles(curatedPresetsDir) : Promise.resolve([])
      ])
      const curatedFileIds = new Set(curatedFiles.flatMap((filename) => presetIdVariants(filename)))
      const visualsSettings = await loadVisualsSettings()
      const curated = visualsSettings.curatedPresets || {}
      const seenIds = new Set(Object.keys(curated).filter((id) => curated[id]))

      const uncuratedFiles = localFiles.filter((filename) => {
        const variants = presetIdVariants(filename)
        return !variants.some((id) => seenIds.has(id) || curatedFileIds.has(id))
      })

      const localItems = (libraryMode === 'inbox' ? uncuratedFiles : localFiles).map(
        (filename) => ({ filename, source: 'local' })
      )
      const curatedItems = curatedFiles.map((filename) => ({ filename, source: 'curated' }))
      const result =
        libraryMode === 'curated'
          ? curatedItems
          : libraryMode === 'all'
            ? [...curatedItems, ...localItems]
            : localItems

      console.log(`[Visuals] Found ${result.length} presets for ${libraryMode} mode`)
      return result
    } catch (err) {
      console.error('[Visuals] Failed to list presets:', err)
      return []
    }
  })

  ipcMain.handle('load-preset-content', async (_event, presetRef) => {
    const devPresetsDir = path.join(process.cwd(), 'visuals', 'presets')
    const curatedPresetsDir = path.join(process.cwd(), 'visuals', 'curated', 'presets')
    const source =
      typeof presetRef === 'object' && presetRef?.source === 'curated' ? 'curated' : 'local'
    const filename = typeof presetRef === 'object' ? presetRef.filename : presetRef
    const rootDir = source === 'curated' ? curatedPresetsDir : devPresetsDir

    try {
      const { filePath } = resolvePresetPath(rootDir, filename)
      return await fs.readFile(filePath, 'utf8')
    } catch (err) {
      console.error('Failed to read preset:', err)
      return null
    }
  })

  ipcMain.handle('delete-preset', async (event, filename) => {
    const fs = require('fs/promises')
    const path = require('path')
    const devPresetsDir = path.join(process.cwd(), 'visuals', 'presets')
    const devTexturesDir = path.join(process.cwd(), 'visuals', 'textures')

    try {
      const { filePath } = resolvePresetPath(devPresetsDir, filename)
      const milkContent = await fs.readFile(filePath, 'utf8')
      const textureRegex = /([-a-zA-Z0-9_.() ]+\.(jpg|jpeg|png|tga|dds))/gi
      const foundTextures = [...new Set(milkContent.match(textureRegex) || [])]

      // Delete the preset
      await fs.unlink(filePath)
      console.log(`[Visuals] Deleted preset: ${filename}`)

      // Safe texture deletion logic
      const otherMilkFiles = (await fs.readdir(devPresetsDir)).filter((f) => f.endsWith('.milk'))

      for (const tex of foundTextures) {
        const texName = path.basename(tex)
        const texPath = path.join(devTexturesDir, texName)

        if (!require('fs').existsSync(texPath)) continue

        let isShared = false
        // Optimization: only read if we really have textures to check
        for (const otherFile of otherMilkFiles) {
          const otherPath = path.join(devPresetsDir, otherFile)
          const otherContent = await fs.readFile(otherPath, 'utf8')
          if (otherContent.includes(texName)) {
            isShared = true
            break
          }
        }

        if (!isShared) {
          await fs.unlink(texPath)
          console.log(`[Visuals] Deleted unused texture: ${texName}`)
        } else {
          console.log(`[Visuals] Texture ${texName} is shared, skipping deletion`)
        }
      }

      return { ok: true }
    } catch (err) {
      console.error('Failed to delete preset:', err)
      throw err
    }
  })

  ipcMain.handle('collect-preset', async (_event, filename) => {
    const fs = require('fs/promises')
    const path = require('path')
    const exists = require('fs').existsSync
    const devPresetsDir = path.join(process.cwd(), 'visuals', 'presets')
    const devTexturesDir = path.join(process.cwd(), 'visuals', 'textures')
    const curatedPresetsDir = path.join(process.cwd(), 'visuals', 'curated', 'presets')
    const curatedTexturesDir = path.join(process.cwd(), 'visuals', 'curated', 'textures')

    const { filePath, relativePath } = resolvePresetPath(devPresetsDir, filename)
    const curatedPath = path.join(curatedPresetsDir, relativePath)

    try {
      await fs.mkdir(curatedPresetsDir, { recursive: true })
      await fs.mkdir(curatedTexturesDir, { recursive: true })

      let movedPresetsCount = 0
      let movedTexturesCount = 0

      if (exists(filePath)) {
        const milkContent = await fs.readFile(filePath, 'utf8')
        const foundTextures = extractTextureNames(milkContent)
        await fs.mkdir(path.dirname(curatedPath), { recursive: true })
        const destPresetPath = await uniqueDestinationPath(curatedPath)

        await fs.rename(filePath, destPresetPath)
        movedPresetsCount = 1

        for (const texName of foundTextures) {
          const texPath = path.join(devTexturesDir, texName)
          const destTexPath = path.join(curatedTexturesDir, texName)

          if (exists(texPath) && !exists(destTexPath)) {
            await fs.copyFile(texPath, destTexPath)
            movedTexturesCount++
          }
        }
      } else if (!exists(curatedPath)) {
        throw new Error(`Preset not found: ${relativePath}`)
      }

      const visualsSettings = await loadVisualsSettings()
      const curated = { ...(visualsSettings.curatedPresets || {}) }
      presetIdVariants(relativePath).forEach((id) => delete curated[id])
      await saveVisualsSettings({ curatedPresets: curated })

      return { ok: true, movedPresetsCount, movedTexturesCount }
    } catch (err) {
      console.error('Collect preset failed:', err)
      throw err
    }
  })

  ipcMain.handle('collect-favorites', async () => {
    const fs = require('fs/promises')
    const path = require('path')
    const devPresetsDir = path.join(process.cwd(), 'visuals', 'presets')
    const devTexturesDir = path.join(process.cwd(), 'visuals', 'textures')
    const curatedPresetsDir = path.join(process.cwd(), 'visuals', 'curated', 'presets')
    const curatedTexturesDir = path.join(process.cwd(), 'visuals', 'curated', 'textures')

    const visualsSettings = await loadVisualsSettings()
    const curated = visualsSettings.curatedPresets || {}
    const likedFiles = Object.keys(curated).filter((id) => curated[id] === 'liked')

    try {
      if (likedFiles.length === 0) return { ok: true, movedPresetsCount: 0, movedTexturesCount: 0 }

      await fs.mkdir(curatedPresetsDir, { recursive: true })
      await fs.mkdir(curatedTexturesDir, { recursive: true })

      let movedPresetsCount = 0
      let movedTexturesCount = 0
      const processedIds = new Set()

      for (const id of likedFiles) {
        let presetPathInfo
        try {
          const trimmed = id.trim()
          const nameToResolve = trimmed.endsWith('.milk') ? trimmed : `${trimmed}.milk`
          presetPathInfo = resolvePresetPath(devPresetsDir, nameToResolve)
        } catch {
          console.warn('[Curation] Invalid favorite preset key skipped:', id)
          continue
        }

        const { filePath, relativePath } = presetPathInfo
        const targetPath = path.join(curatedPresetsDir, relativePath)

        if (!require('fs').existsSync(filePath)) {
          if (require('fs').existsSync(targetPath)) {
            processedIds.add(id)
          }
          continue
        }

        const milkContent = await fs.readFile(filePath, 'utf8')
        const foundTextures = extractTextureNames(milkContent)

        await fs.mkdir(path.dirname(targetPath), { recursive: true })
        await fs.rename(filePath, targetPath)
        movedPresetsCount++
        processedIds.add(id)

        for (const texName of foundTextures) {
          const texPath = path.join(devTexturesDir, texName)
          const destTexPath = path.join(curatedTexturesDir, texName)

          if (require('fs').existsSync(texPath) && !require('fs').existsSync(destTexPath)) {
            await fs.copyFile(texPath, destTexPath)
            movedTexturesCount++
          }
        }
      }

      // Cleanup curation status for moved or verified files
      const updatedCurated = { ...curated }
      processedIds.forEach((id) =>
        presetIdVariants(id).forEach((variant) => delete updatedCurated[variant])
      )
      await saveVisualsSettings({ curatedPresets: updatedCurated })

      return { ok: true, movedPresetsCount, movedTexturesCount }
    } catch (err) {
      console.error('Collect favorites failed:', err)
      throw err
    }
  })

  ipcMain.handle('batch-curate-presets', async () => {
    // Legacy / Discard logic - now moves ignored presets to a trash subfolder
    const fs = require('fs/promises')
    const path = require('path')
    const devPresetsDir = path.join(process.cwd(), 'visuals', 'presets')
    const devTexturesDir = path.join(process.cwd(), 'visuals', 'textures')
    const discardedPresetsDir = path.join(devPresetsDir, 'discarded')
    const discardedTexturesDir = path.join(devTexturesDir, 'discarded')

    const visualsSettings = await loadVisualsSettings()
    const curated = visualsSettings.curatedPresets || {}
    // Only move things explicitly marked as 'discarded'
    const toDiscard = Object.keys(curated).filter((f) => curated[f] === 'discarded')

    try {
      if (toDiscard.length === 0) return { ok: true, movedPresetsCount: 0, movedTexturesCount: 0 }
      await fs.mkdir(discardedPresetsDir, { recursive: true })
      await fs.mkdir(discardedTexturesDir, { recursive: true })

      let movedPresetsCount = 0
      let movedTexturesCount = 0

      for (const filename of toDiscard) {
        let presetPathInfo
        try {
          const trimmed = filename.trim()
          const nameToResolve = trimmed.endsWith('.milk') ? trimmed : `${trimmed}.milk`
          presetPathInfo = resolvePresetPath(devPresetsDir, nameToResolve)
        } catch {
          console.warn('[Curation] Invalid discarded preset key skipped:', filename)
          continue
        }

        const { filePath, relativePath } = presetPathInfo
        if (!require('fs').existsSync(filePath)) continue

        const milkContent = await fs.readFile(filePath, 'utf8')
        const foundTextures = extractTextureNames(milkContent)

        const targetPath = path.join(discardedPresetsDir, relativePath)
        await fs.mkdir(path.dirname(targetPath), { recursive: true })
        await fs.rename(filePath, targetPath)
        movedPresetsCount++

        const remainingMilkFiles = (await fs.readdir(devPresetsDir)).filter((f) =>
          f.endsWith('.milk')
        )
        for (const texName of foundTextures) {
          const texPath = path.join(devTexturesDir, texName)
          if (require('fs').existsSync(texPath)) {
            let isShared = false
            for (const rFile of remainingMilkFiles) {
              const rContent = await fs.readFile(path.join(devPresetsDir, rFile), 'utf8')
              if (rContent.includes(texName)) {
                isShared = true
                break
              }
            }
            if (!isShared) {
              await fs.rename(texPath, path.join(discardedTexturesDir, texName))
              movedTexturesCount++
            }
          }
        }
        delete curated[filename]
      }
      await saveVisualsSettings({ curatedPresets: curated })
      return { ok: true, movedPresetsCount, movedTexturesCount }
    } catch (err) {
      console.error('Batch curate presets failed:', err)
      throw err
    }
  })

  ipcMain.handle('get-textures-list', async () => {
    const fs = require('fs')
    const path = require('path')
    const devTexturesDir = path.join(process.cwd(), 'visuals', 'textures')
    if (!fs.existsSync(devTexturesDir)) return []
    // Include jpg/jpeg/png AND tga/dds which many MilkDrop presets reference
    return fs.readdirSync(devTexturesDir).filter((f) => /\.(jpg|jpeg|png|tga|dds)$/i.test(f))
  })

  ipcMain.handle('get-textures-dir', () => {
    const path = require('path')
    return path.join(process.cwd(), 'visuals', 'textures')
  })

  async function countMilkFiles(dir) {
    let count = 0
    try {
      const entries = await fs.readdir(dir, { withFileTypes: true })
      for (const entry of entries) {
        if (entry.isDirectory()) {
          count += await countMilkFiles(path.join(dir, entry.name))
        } else if (entry.isFile() && entry.name.toLowerCase().endsWith('.milk')) {
          count++
        }
      }
    } catch {
      // Ignore unreadable or non-existent directories
    }
    return count
  }

  async function getPresetPackStatus() {
    const userDataDir = path.join(app.getPath('userData'), 'visuals')
    const localDir = path.join(process.cwd(), 'visuals')

    const userDataPresets = path.join(userDataDir, 'presets')
    const localPresets = path.join(localDir, 'presets')
    const curatedPresets = path.join(localDir, 'curated', 'presets')

    const userCount = await countMilkFiles(userDataPresets)
    const localCount = await countMilkFiles(localPresets)
    const curatedCount = await countMilkFiles(curatedPresets)

    const fullCount = userCount + localCount

    return {
      installed: fullCount > 500,
      fullCount,
      curatedCount,
      activePack:
        fullCount > 500 ? 'Isosceles "Cream of the Crop" (9k+ Presets)' : 'Default Curated Pack',
      presetsDir:
        fullCount > 500 ? (userCount > 500 ? userDataPresets : localPresets) : curatedPresets
    }
  }

  ipcMain.handle('get-preset-pack-status', async () => {
    return await getPresetPackStatus()
  })

  ipcMain.handle('download-preset-pack', async (event) => {
    const sendProgress = (payload) => {
      if (event?.sender && !event.sender.isDestroyed()) {
        event.sender.send('preset-pack-progress', payload)
      }
    }

    const localZip = path.join(
      process.cwd(),
      'visuals',
      'Isosceles_CreamOfTheCrop_MilkdropPresetsPack.zip'
    )
    const userZip = path.join(
      app.getPath('userData'),
      'Isosceles_CreamOfTheCrop_MilkdropPresetsPack.zip'
    )
    const targetExtractDir = app.isPackaged
      ? path.join(app.getPath('userData'), 'visuals')
      : path.join(process.cwd(), 'visuals')

    let zipToExtract = null
    if (existsSync(localZip)) {
      zipToExtract = localZip
    } else if (existsSync(userZip)) {
      zipToExtract = userZip
    }

    if (!zipToExtract) {
      sendProgress({
        phase: 'downloading',
        percent: 0,
        speed: '0 MB/s',
        detail: 'Connecting to GitHub Releases...'
      })
      try {
        const { net } = require('electron')
        const url =
          'https://github.com/magnetofon/Magnetofon/releases/download/v1.0.0/Isosceles_CreamOfTheCrop_MilkdropPresetsPack.zip'

        await new Promise((resolve, reject) => {
          const downloadReq = (downloadUrl) => {
            const req = net.request(downloadUrl)
            req.on('response', (res) => {
              if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                const redirectUrl = Array.isArray(res.headers.location)
                  ? res.headers.location[0]
                  : res.headers.location
                downloadReq(redirectUrl)
                return
              }
              if (res.statusCode !== 200) {
                reject(new Error(`Download failed with status ${res.statusCode}`))
                return
              }
              const totalBytes = Number(res.headers['content-length'] || 143372759)
              let downloaded = 0
              const fileStream = require('fs').createWriteStream(userZip)

              res.on('data', (chunk) => {
                downloaded += chunk.length
                fileStream.write(chunk)
                const percent = Math.min(99, Math.round((downloaded / totalBytes) * 100))
                const speed = `${(downloaded / (1024 * 1024)).toFixed(1)} MB / ${(totalBytes / (1024 * 1024)).toFixed(1)} MB`
                sendProgress({
                  phase: 'downloading',
                  percent,
                  speed,
                  detail: `Downloading Isosceles Pack (${speed})`
                })
              })
              res.on('end', () => {
                fileStream.end()
                zipToExtract = userZip
                resolve()
              })
              res.on('error', reject)
            })
            req.on('error', reject)
            req.end()
          }
          downloadReq(url)
        })
      } catch (err) {
        console.error('[PresetPack] Download error:', err)
        sendProgress({
          phase: 'error',
          percent: 0,
          speed: '',
          detail: `Download failed: ${err.message}`
        })
        throw err
      }
    }

    sendProgress({
      phase: 'extracting',
      percent: 15,
      speed: '',
      detail: 'Extracting 9,000+ presets & textures...'
    })
    await fs.mkdir(targetExtractDir, { recursive: true })

    try {
      const res = await execFileText('unzip', ['-o', zipToExtract, '-d', targetExtractDir])
      if (!res.ok) {
        if (process.platform === 'win32') {
          const psCmd = `Expand-Archive -Force -Path "${zipToExtract}" -DestinationPath "${targetExtractDir}"`
          await execFileText('powershell', ['-NoProfile', '-Command', psCmd])
        } else {
          throw new Error(`Unzip failed: ${res.stderr || res.error?.message}`)
        }
      }
    } catch (err) {
      console.error('[PresetPack] Extract error:', err)
      sendProgress({
        phase: 'error',
        percent: 0,
        speed: '',
        detail: `Extraction failed: ${err.message}`
      })
      throw err
    }

    // Standardize folder casing (Presets -> presets, Textures -> textures)
    const capPresets = path.join(targetExtractDir, 'Presets')
    const capTextures = path.join(targetExtractDir, 'Textures')
    const targetPresets = path.join(targetExtractDir, 'presets')
    const targetTextures = path.join(targetExtractDir, 'textures')

    if (existsSync(capPresets) && capPresets !== targetPresets) {
      await fs.mkdir(targetPresets, { recursive: true })
      const files = await fs.readdir(capPresets)
      for (const f of files) {
        await fs
          .cp(path.join(capPresets, f), path.join(targetPresets, f), {
            recursive: true,
            force: true
          })
          .catch(() => {})
      }
    }
    if (existsSync(capTextures) && capTextures !== targetTextures) {
      await fs.mkdir(targetTextures, { recursive: true })
      const files = await fs.readdir(capTextures)
      for (const f of files) {
        await fs
          .cp(path.join(capTextures, f), path.join(targetTextures, f), {
            recursive: true,
            force: true
          })
          .catch(() => {})
      }
    }

    sendProgress({
      phase: 'completed',
      percent: 100,
      speed: '',
      detail: '9k+ Curated Presets installed successfully!'
    })

    if (isNativeProjectMRunning()) {
      await launchNativeProjectM({ restartIfRunning: true })
    }

    return await getPresetPackStatus()
  })

  ipcMain.handle('native-surround-start', async (_event, options) => {
    return startNativeSurround(options)
  })

  ipcMain.handle('native-surround-stop', async () => {
    await stopNativeSurround()
    return { ok: true }
  })

  ipcMain.handle('native-surround-set-paused', async (_event, paused) => {
    nativeSurround.paused = Boolean(paused)
    await sendMpvCommand(['set_property', 'pause', nativeSurround.paused])
    return { ok: true }
  })

  ipcMain.handle('native-surround-seek', async (_event, time) => {
    await sendMpvCommand(['seek', Math.max(0, Number(time) || 0), 'absolute', 'exact'])
    return { ok: true }
  })

  ipcMain.handle('native-surround-set-volume', async (_event, volume) => {
    nativeSurround.volume = Math.round(Math.max(0, Math.min(1, Number(volume) || 0)) * 100)
    await sendMpvCommand(['set_property', 'volume', nativeSurround.volume])
    return { ok: true }
  })

  ipcMain.handle('native-surround-status', async () => {
    return {
      ok: true,
      active: Boolean(nativeSurround.child),
      file: nativeSurround.file,
      device: nativeSurround.device,
      monitorSource: nativeSurround.monitorSource,
      monitorActive: Boolean(nativeSurround.monitorChild),
      mode: nativeSurround.mode,
      requestedMode: nativeSurround.requestedMode,
      lastError: nativeSurround.lastError
    }
  })

  const playlistsDir = join(app.getPath('userData'), 'playlists')

  ipcMain.handle('list-saved-playlists', async () => {
    await fs.mkdir(playlistsDir, { recursive: true })
    const files = await fs.readdir(playlistsDir)
    return files
      .filter((file) => file.endsWith('.json'))
      .map((file) => file.replace(/\.json$/i, ''))
      .sort((a, b) => a.localeCompare(b))
  })

  ipcMain.handle('save-playlist', async (_event, { name, playlist }) => {
    const safeName = String(name || '')
      .trim()
      .replace(/[\\/:*?"<>|]/g, '_')
    if (!safeName) throw new Error('Playlist name is required')

    await fs.mkdir(playlistsDir, { recursive: true })
    const filePath = join(playlistsDir, `${safeName}.json`)
    const payload = {
      name: safeName,
      savedAt: new Date().toISOString(),
      playlist: Array.isArray(playlist) ? playlist : []
    }
    await fs.writeFile(filePath, JSON.stringify(payload, null, 2), 'utf8')
    return { ok: true, name: safeName }
  })

  ipcMain.handle('load-playlist', async (_event, name) => {
    const safeName = String(name || '')
      .trim()
      .replace(/[\\/:*?"<>|]/g, '_')
    if (!safeName) throw new Error('Playlist name is required')

    const filePath = join(playlistsDir, `${safeName}.json`)
    const raw = await fs.readFile(filePath, 'utf8')
    const data = JSON.parse(raw)
    return {
      name: data.name || safeName,
      playlist: Array.isArray(data.playlist) ? data.playlist : []
    }
  })

  ipcMain.handle('delete-playlist', async (_event, name) => {
    const safeName = String(name || '')
      .trim()
      .replace(/[\\/:*?"<>|]/g, '_')
    if (!safeName) throw new Error('Playlist name is required')

    const filePath = join(playlistsDir, `${safeName}.json`)
    await fs.rm(filePath, { force: true })
    return { ok: true }
  })

  ipcMain.handle('save-playlist-file', async (_event, playlist) => {
    const { dialog } = require('electron')
    const result = await dialog.showSaveDialog({
      title: 'Save Magnetofon Playlist',
      defaultPath: join(app.getPath('documents'), 'playlist.lap'),
      filters: [
        { name: 'Magnetofon Playlist', extensions: ['lap'] },
        { name: 'M3U Playlist', extensions: ['m3u'] }
      ]
    })

    if (result.canceled || !result.filePath) return { canceled: true }

    const isM3U = result.filePath.endsWith('.m3u')
    if (isM3U) {
      const content =
        '#EXTM3U\n' +
        playlist
          .map(
            (t) =>
              `#EXTINF:${Math.floor(t.metadata?.duration || 0)},${t.metadata?.artist || 'Unknown'} - ${t.metadata?.title || 'Unknown'}\n${t.file}`
          )
          .join('\n')
      await fs.writeFile(result.filePath, content, 'utf8')
    } else {
      const payload = {
        version: '1.0',
        savedAt: new Date().toISOString(),
        playlist: playlist
      }
      await fs.writeFile(result.filePath, JSON.stringify(payload, null, 2), 'utf8')
    }
    return { ok: true, filePath: result.filePath }
  })

  ipcMain.handle('load-playlist-file', async () => {
    const { dialog } = require('electron')
    const result = await dialog.showOpenDialog({
      title: 'Load Magnetofon Playlist',
      filters: [{ name: 'Playlists', extensions: ['lap', 'm3u'] }]
    })

    if (result.canceled || result.filePaths.length === 0) return null

    const filePath = result.filePaths[0]
    const content = await fs.readFile(filePath, 'utf8')

    if (filePath.endsWith('.m3u')) {
      const lines = content.split('\n')
      const playlist = []
      for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim()
        if (line && !line.startsWith('#')) {
          playlist.push({ file: line, metadata: { title: line.split(/[\\/]/).pop() } })
        }
      }
      return { name: filePath.split(/[\\/]/).pop(), playlist }
    } else {
      const data = JSON.parse(content)
      return {
        name: filePath
          .split(/[\\/]/)
          .pop()
          .replace(/\.lap$/i, ''),
        playlist: data.playlist || []
      }
    }
  })

  ipcMain.on('open-local-preset-folder', () => {
    const target = join(process.cwd(), 'visuals', 'presets')
    execFile('xdg-open', [target], () => {})
  })
})

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

app.on('before-quit', (e) => {
  e.preventDefault()
  Promise.all([
    stopNativeProjectM().catch((err) => console.warn('[projectM] shutdown failed:', err)),
    stopNativeSurround().catch((err) => console.warn('[NativePlayback] shutdown failed:', err))
  ]).finally(() => {
    Object.keys(windows).forEach((name) => {
      const win = windows[name]
      if (win && !win.isDestroyed()) {
        try {
          win.close()
        } catch (err) {
          console.warn(`[Main] Could not close window "${name}":`, err)
        }
      }
      windows[name] = null
    })
    process.exit(0)
  })
})
