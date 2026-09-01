import { createHash } from 'node:crypto'
import {
  createWriteStream,
  cpSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  renameSync,
  rmSync,
  writeFileSync
} from 'node:fs'
import { finished } from 'node:stream/promises'
import { Readable } from 'node:stream'
import { tmpdir } from 'node:os'
import { dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawnSync } from 'node:child_process'

const MPV_VERSION = '0.41.0'
const MPV_BUILD = 'v0.41.0@2026-08-28_1787881017'
const MPV_URL =
  'https://github.com/pkgforge-dev/mpv-AppImage/releases/download/v0.41.0%402026-08-28_1787881017/mpv-v0.41.0-anylinux-x86_64.AppImage'
const MPV_SHA256 = '48ab5f9c52263bd617c89d0ea311134777d2a0f170c6db3a546d9f061b4d3265'

if (process.platform !== 'linux' || process.arch !== 'x64') {
  console.log('Bundled mpv runtime is only prepared for Linux x64; skipping.')
  process.exit(0)
}

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const runtimeRoot = join(projectRoot, 'resources', 'mpv', 'linux-x64', 'runtime')
const markerPath = join(runtimeRoot, 'runtime.json')
const expectedMarker = {
  generatedBy: 'scripts/prepare-linux-audio-runtime.mjs',
  layoutVersion: 2,
  mpvVersion: MPV_VERSION,
  sourceBuild: MPV_BUILD,
  sourceUrl: MPV_URL,
  sha256: MPV_SHA256
}

function runtimeIsCurrent() {
  if (!existsSync(join(runtimeRoot, 'bin', 'mpv')) || !existsSync(markerPath)) return false
  try {
    const marker = JSON.parse(readFileSync(markerPath, 'utf8'))
    return Object.entries(expectedMarker).every(([key, value]) => marker[key] === value)
  } catch {
    return false
  }
}

if (runtimeIsCurrent()) {
  console.log(`Bundled mpv ${MPV_VERSION} runtime is ready.`)
  process.exit(0)
}

if (existsSync(runtimeRoot)) {
  let marker
  try {
    marker = JSON.parse(readFileSync(markerPath, 'utf8'))
  } catch {
    marker = null
  }
  if (marker?.generatedBy !== expectedMarker.generatedBy) {
    throw new Error(`Refusing to replace unmanaged directory: ${runtimeRoot}`)
  }
}

const workRoot = mkdtempSync(join(tmpdir(), 'magnetofon-mpv-'))
const imagePath = join(workRoot, 'mpv.AppImage')
const stagingRoot = mkdtempSync(join(dirname(runtimeRoot), '.runtime-staging-'))

try {
  console.log(`Downloading pinned mpv ${MPV_VERSION} runtime…`)
  const response = await fetch(MPV_URL, { redirect: 'follow' })
  if (!response.ok || !response.body) {
    throw new Error(`mpv download failed with HTTP ${response.status}`)
  }
  await finished(
    Readable.fromWeb(response.body).pipe(createWriteStream(imagePath, { mode: 0o755 }))
  )

  const digest = createHash('sha256').update(readFileSync(imagePath)).digest('hex')
  if (digest !== MPV_SHA256) {
    throw new Error(`mpv checksum mismatch: expected ${MPV_SHA256}, received ${digest}`)
  }

  const extraction = spawnSync(imagePath, ['--appimage-extract'], {
    cwd: workRoot,
    env: { ...process.env, DISABLE_AUTO_UPDATES: '1' },
    encoding: 'utf8'
  })
  if (extraction.status !== 0) {
    throw new Error(`mpv extraction failed:\n${extraction.stderr || extraction.stdout}`)
  }

  const appDir = join(workRoot, 'AppDir')
  mkdirSync(join(stagingRoot, 'bin'), { recursive: true })
  mkdirSync(join(stagingRoot, 'shared', 'bin'), { recursive: true })
  cpSync(join(appDir, 'bin', 'mpv'), join(stagingRoot, 'bin', 'mpv'))
  cpSync(join(appDir, 'shared', 'bin', 'mpv'), join(stagingRoot, 'shared', 'bin', 'mpv'))
  for (const directory of ['lib', 'etc', 'share']) {
    cpSync(join(appDir, directory), join(stagingRoot, directory), {
      recursive: true,
      dereference: false,
      verbatimSymlinks: true
    })
  }
  for (const file of ['.env', '.foreign-dlopen-enabled', '.preload']) {
    if (existsSync(join(appDir, file))) cpSync(join(appDir, file), join(stagingRoot, file))
  }
  writeFileSync(join(stagingRoot, 'runtime.json'), `${JSON.stringify(expectedMarker, null, 2)}\n`)

  if (existsSync(runtimeRoot)) rmSync(runtimeRoot, { recursive: true, force: true })
  renameSync(stagingRoot, runtimeRoot)
  console.log(`Bundled mpv ${MPV_VERSION} runtime prepared at ${runtimeRoot}`)
} finally {
  rmSync(workRoot, { recursive: true, force: true })
  rmSync(stagingRoot, { recursive: true, force: true })
}
