import { usePlayerStore } from '../store/playerStore'

class AudioEngine {
  constructor() {
    // Only the main player window drives playback. Other windows receive state/audio over IPC.
    if (window.location.hash !== '#player' && window.location.hash !== '') return

    this.nativePlaybackActive = false
    this.nativePlaybackPending = false
    this._nativeFile = null
    this._nativeFailedFile = null
    this._nativeFailedAt = 0
    this._nativeLastPaused = null
    this._nativeLastVolume = null
    this._nativeLastMode = null
    this._lastFile = null
    this._lastIsPlaying = null
    this._lastVolume = null
    this._lastSurroundMode = null
    this._nativeSyncToken = 0
    this._nativeSyncQueue = Promise.resolve()
    this._nativeT = new Uint8Array(1024).fill(128)
    this._nativeTL = new Uint8Array(1024).fill(128)
    this._nativeTR = new Uint8Array(1024).fill(128)
    this._nativeFreq = new Uint8Array(512)
    this._nativeAudioAt = 0

    this.setupNativePlaybackEvents()

    usePlayerStore.subscribe((state) => {
      if (state.file !== this._lastFile) {
        this._lastFile = state.file
        if (state.isPlaying) {
          this._lastIsPlaying = state.isPlaying
          this.play().catch((err) => console.error('[AudioEngine] Native play failed:', err))
        } else {
          this.syncNativePlayback(state).catch((err) => {
            console.warn('[AudioEngine] Native file sync failed:', err)
          })
        }
      }

      if (state.isPlaying !== this._lastIsPlaying) {
        this._lastIsPlaying = state.isPlaying
        if (state.isPlaying) {
          this.play().catch((err) => console.error('[AudioEngine] Native play failed:', err))
        } else {
          this.pause()
        }
      }

      if (state.volume !== this._lastVolume) {
        this._lastVolume = state.volume
        if (this.nativePlaybackActive) {
          this._nativeLastVolume = state.volume
          window.api?.nativeSurround?.setVolume?.(state.volume).catch((err) => {
            console.warn('[AudioEngine] Native volume update failed:', err)
          })
        }
      }

      if (state.surroundMode !== this._lastSurroundMode) {
        this._lastSurroundMode = state.surroundMode
        if (state.file) {
          this.syncNativePlayback(state).catch((err) => {
            console.warn('[AudioEngine] Native mode sync failed:', err)
          })
        }
      }
    })
  }

  normalizePlaybackMode(mode) {
    const normalized = String(mode || 'AUTO').toUpperCase()
    return ['AUTO', 'STEREO', 'SURROUND'].includes(normalized) ? normalized : 'AUTO'
  }

  shouldUseNativePlayback(state = usePlayerStore.getState()) {
    // Native mpv/libav handles real format detection. Do not reject extensionless
    // FLAC/Qobuz-style files here — Tom's library has plenty of those.
    return Boolean(state.file)
  }

  setupNativePlaybackEvents() {
    window.api?.nativeSurround?.onAudioData?.((data) => {
      this.consumeNativePcm(data)
    })

    window.api?.nativeSurround?.onPosition?.((data) => {
      const state = usePlayerStore.getState()
      if (data?.file === state.file && this.shouldUseNativePlayback(state)) {
        usePlayerStore.getState().setProgress(data.time || 0)
      }
    })

    window.api?.nativeSurround?.onEnded?.((data) => {
      const state = usePlayerStore.getState()
      if (data?.file === state.file && this.shouldUseNativePlayback(state)) {
        this.nativePlaybackActive = false
        this.nativePlaybackPending = false
        this._nativeFile = null
        usePlayerStore.getState().playNextTrack()
      }
    })
  }

  consumeNativePcm(data) {
    if (!data?.buffer || data.format !== 's16le') return
    const pcm = new Int16Array(data.buffer)
    if (pcm.length < 2) return

    const frames = Math.floor(pcm.length / 2)
    const outLen = this._nativeT.length
    const step = Math.max(1, Math.floor(frames / outLen))

    for (let i = 0; i < outLen; i++) {
      const frame = Math.min(frames - 1, i * step)
      const l = pcm[frame * 2] || 0
      const r = pcm[frame * 2 + 1] || 0
      const mono = (l + r) / 2
      this._nativeTL[i] = Math.max(0, Math.min(255, 128 + Math.round((l / 32768) * 127)))
      this._nativeTR[i] = Math.max(0, Math.min(255, 128 + Math.round((r / 32768) * 127)))
      this._nativeT[i] = Math.max(0, Math.min(255, 128 + Math.round((mono / 32768) * 127)))
    }

    const bins = this._nativeFreq.length
    const n = Math.min(512, outLen)
    for (let b = 0; b < bins; b++) {
      const start = Math.floor((b / bins) * n)
      const end = Math.max(start + 1, Math.floor(((b + 1) / bins) * n))
      let sum = 0
      for (let i = start; i < end; i++) sum += Math.abs(this._nativeT[i] - 128)
      this._nativeFreq[b] = Math.max(0, Math.min(255, Math.round((sum / (end - start)) * 2.2)))
    }

    this._nativeAudioAt = Date.now()
  }

  hasFreshNativeAudio() {
    return Date.now() - this._nativeAudioAt < 750
  }

  getFrequencyData() {
    return this._nativeFreq
  }

  getTimeDomainData() {
    return { t: this._nativeT, tL: this._nativeTL, tR: this._nativeTR }
  }

  async stopNativePlayback() {
    this._nativeSyncToken++
    this.nativePlaybackActive = false
    this.nativePlaybackPending = false
    this._nativeFile = null
    this._nativeLastPaused = null
    this._nativeLastVolume = null
    this._nativeLastMode = null
    await window.api?.nativeSurround?.stop?.()
  }

  syncNativePlayback(state = usePlayerStore.getState()) {
    const snapshot = {
      file: state.file,
      progress: state.progress,
      volume: state.volume,
      isPlaying: state.isPlaying,
      surroundMode: this.normalizePlaybackMode(state.surroundMode),
      eqEnabled: state.eqEnabled,
      preamp: state.preamp,
      eqBands: state.eqBands
    }
    const token = ++this._nativeSyncToken

    this._nativeSyncQueue = this._nativeSyncQueue
      .catch(() => {})
      .then(() => this.performNativePlaybackSync(snapshot, token))

    return this._nativeSyncQueue
  }

  async performNativePlaybackSync(state, token) {
    const nativeApi = window.api?.nativeSurround
    if (!nativeApi) return
    if (token !== this._nativeSyncToken) return

    const currentFile = state.file
    if (!currentFile) {
      if (this.nativePlaybackActive || this.nativePlaybackPending || this._nativeFile) {
        await this.stopNativePlayback()
      }
      return
    }

    if (
      !this.nativePlaybackActive &&
      this._nativeFailedFile === currentFile &&
      Date.now() - this._nativeFailedAt < 10000
    ) {
      return
    }

    const paused = !state.isPlaying
    const mode = this.normalizePlaybackMode(state.surroundMode)

    if (
      !this.nativePlaybackActive ||
      this._nativeFile !== currentFile ||
      this._nativeLastMode !== mode
    ) {
      this.nativePlaybackPending = true
      try {
        const result = await nativeApi.start({
          file: currentFile,
          startTime: Math.max(0, state.progress || 0),
          volume: state.volume,
          paused,
          surroundMode: mode,
          eqEnabled: state.eqEnabled,
          preamp: state.preamp,
          eqBands: state.eqBands
        })

        if (token !== this._nativeSyncToken) {
          this.nativePlaybackPending = false
          await nativeApi.stop()
          return
        }

        this.nativePlaybackActive = Boolean(result?.ok)
        this.nativePlaybackPending = false
        this._nativeFile = this.nativePlaybackActive ? currentFile : null
        this._nativeLastMode = this.nativePlaybackActive ? mode : null
        if (this.nativePlaybackActive) {
          this._nativeFailedFile = null
          this._nativeFailedAt = 0
          this._nativeLastPaused = paused
          this._nativeLastVolume = state.volume
        }
        console.log('[AudioEngine] Native playback status:', result)
      } catch (err) {
        if (token !== this._nativeSyncToken) {
          this.nativePlaybackPending = false
          return
        }
        this.nativePlaybackActive = false
        this.nativePlaybackPending = false
        this._nativeFile = null
        this._nativeLastPaused = null
        this._nativeLastVolume = null
        this._nativeLastMode = null
        this._nativeFailedFile = currentFile
        this._nativeFailedAt = Date.now()
        console.error('[AudioEngine] Native playback failed:', err)
      }
      return
    }

    const updates = []
    if (paused !== this._nativeLastPaused) {
      this._nativeLastPaused = paused
      updates.push(nativeApi.setPaused(paused))
    }
    if (state.volume !== this._nativeLastVolume) {
      this._nativeLastVolume = state.volume
      updates.push(nativeApi.setVolume(state.volume))
    }
    if (updates.length) await Promise.all(updates)
  }

  async initEngine() {
    await this.syncNativePlayback(usePlayerStore.getState())
  }

  async play() {
    const state = usePlayerStore.getState()
    if (!this.shouldUseNativePlayback(state)) return
    this.nativePlaybackPending = true
    await this.syncNativePlayback({ ...state, isPlaying: true })
    if (!this.nativePlaybackActive) this.nativePlaybackPending = false
  }

  pause() {
    if (this.nativePlaybackActive) {
      this._nativeLastPaused = true
      window.api?.nativeSurround?.setPaused?.(true).catch((err) => {
        console.warn('[AudioEngine] Native pause failed:', err)
      })
    }
  }

  seek(time) {
    if (this.nativePlaybackActive) {
      window.api?.nativeSurround?.seek?.(time).catch((err) => {
        console.warn('[AudioEngine] Native seek failed:', err)
      })
    }
  }

  async destroy() {
    await this.stopNativePlayback()
  }
}

export const engine = new AudioEngine()

window.__MAGNETOFON_AUDIO_ENGINE__ = engine
window.addEventListener('beforeunload', () => {
  engine.destroy().catch((e) => console.warn('[AudioEngine] destroy on unload failed:', e))
})
