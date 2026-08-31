import { create } from 'zustand'

export const normalizeSurroundMode = (mode) => {
  const normalized = String(mode || 'AUTO').toUpperCase()
  return ['AUTO', 'STEREO', 'SURROUND'].includes(normalized) ? normalized : 'AUTO'
}

export const usePlayerStore = create((set) => ({
  file: null,
  metadata: null,
  isPlaying: false,
  volume: 0.8,
  progress: 0,
  duration: 0,
  currentPlaylistName: null,
  curatedPresets: {},

  eqEnabled: false,
  preamp: -3,
  eqBands: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  surroundMode: 'AUTO',

  playlist: [],

  setFile: (file, metadata) =>
    set((state) => {
      // Add to playlist if not exists
      const trackId =
        typeof crypto !== 'undefined' && crypto.randomUUID
          ? crypto.randomUUID()
          : `${Date.now()}-${Math.random()}`
      const exists = state.playlist.find((t) => t.file === file)
      const newPlaylist = exists
        ? state.playlist
        : [...state.playlist, { file, metadata, id: trackId }]

      return {
        file,
        metadata,
        progress: 0,
        duration: metadata?.duration || 0,
        playlist: newPlaylist
      }
    }),

  addToPlaylist: (item) =>
    set((state) => {
      const exists = state.playlist.find((t) => t.file === item.path)
      if (exists) return {}
      const trackId =
        typeof crypto !== 'undefined' && crypto.randomUUID
          ? crypto.randomUUID()
          : `${Date.now()}-${Math.random()}`
      return {
        playlist: [...state.playlist, { file: item.path, metadata: item.meta, id: trackId }]
      }
    }),

  removeFromPlaylist: (id) =>
    set((state) => ({
      playlist: state.playlist.filter((t) => t.id !== id)
    })),

  clearPlaylist: () => set({ playlist: [], currentPlaylistName: null }),

  reorderTrack: (index, direction) =>
    set((state) => {
      const newPlaylist = [...state.playlist]
      const targetIndex = index + direction
      if (targetIndex < 0 || targetIndex >= newPlaylist.length) return {}

      // Swap
      const temp = newPlaylist[index]
      newPlaylist[index] = newPlaylist[targetIndex]
      newPlaylist[targetIndex] = temp

      return { playlist: newPlaylist }
    }),

  moveTrackInPlaylist: (fromIndex, toIndex) =>
    set((state) => {
      if (
        fromIndex < 0 ||
        fromIndex >= state.playlist.length ||
        toIndex < 0 ||
        toIndex >= state.playlist.length ||
        fromIndex === toIndex
      ) {
        return {}
      }

      const newPlaylist = [...state.playlist]
      const [moved] = newPlaylist.splice(fromIndex, 1)
      newPlaylist.splice(toIndex, 0, moved)

      return { playlist: newPlaylist }
    }),

  setPlaylist: (playlist, name = null) =>
    set((state) => {
      const normalized = (Array.isArray(playlist) ? playlist : []).map((t) => ({
        ...t,
        id:
          t.id ||
          (typeof crypto !== 'undefined' && crypto.randomUUID
            ? crypto.randomUUID()
            : `${Date.now()}-${Math.random()}`)
      }))
      const hasCurrent = normalized.some((t) => t.file === state.file)
      const fallback = normalized[0] || null

      return {
        playlist: normalized,
        currentPlaylistName: name,
        file: hasCurrent ? state.file : fallback?.file || null,
        metadata: hasCurrent ? state.metadata : fallback?.metadata || null,
        progress: 0,
        duration: hasCurrent ? state.duration : fallback?.metadata?.duration || 0,
        isPlaying: hasCurrent ? state.isPlaying : normalized.length > 0 ? state.isPlaying : false
      }
    }),

  setCurrentPlaylistName: (name) => set({ currentPlaylistName: name }),

  setPresetStatus: (filename, status) =>
    set((state) => {
      const curatedPresets = { ...state.curatedPresets }
      if (status == null) {
        delete curatedPresets[filename]
      } else {
        curatedPresets[filename] = status
      }
      return { curatedPresets }
    }),

  playNextTrack: () =>
    set((state) => {
      if (!state.file || state.playlist.length === 0) return { isPlaying: false }

      const currentIndex = state.playlist.findIndex((t) => t.file === state.file)
      const nextTrack = currentIndex >= 0 ? state.playlist[currentIndex + 1] : null

      if (!nextTrack) {
        return {
          isPlaying: false,
          progress: 0
        }
      }

      return {
        file: nextTrack.file,
        metadata: nextTrack.metadata,
        progress: 0,
        duration: nextTrack.metadata?.duration || 0,
        isPlaying: true
      }
    }),

  playPreviousTrack: () =>
    set((state) => {
      if (!state.file || state.playlist.length === 0) return {}

      const currentIndex = state.playlist.findIndex((t) => t.file === state.file)
      const prevTrack = currentIndex > 0 ? state.playlist[currentIndex - 1] : null

      if (!prevTrack) return {}

      return {
        file: prevTrack.file,
        metadata: prevTrack.metadata,
        progress: 0,
        duration: prevTrack.metadata?.duration || 0,
        isPlaying: true
      }
    }),

  setIsPlaying: (isPlaying) => set({ isPlaying }),
  togglePlay: () => set((state) => ({ isPlaying: !state.isPlaying })),

  setVolume: (volume) => set({ volume }),
  setProgress: (progress) => set({ progress }),
  setDuration: (duration) => set({ duration }),

  setEqEnabled: (eqEnabled) => set({ eqEnabled }),
  setPreamp: (preamp) => set({ preamp }),
  setEqBand: (index, value) =>
    set((state) => {
      const newBands = [...state.eqBands]
      newBands[index] = value
      return { eqBands: newBands }
    }),
  setSurroundMode: (surroundMode) => set({ surroundMode: normalizeSurroundMode(surroundMode) })
}))

// State synchronization bridge
let isInternalUpdate = false
let lastSyncPayload = ''

usePlayerStore.subscribe((state) => {
  if (isInternalUpdate) return

  // Build lightweight IPC payload excluding high-frequency progress ticks & base64 artwork
  const payload = {
    file: state.file,
    isPlaying: state.isPlaying,
    volume: state.volume,
    duration: state.duration,
    currentPlaylistName: state.currentPlaylistName,
    eqEnabled: state.eqEnabled,
    preamp: state.preamp,
    eqBands: state.eqBands,
    surroundMode: state.surroundMode,
    curatedPresets: state.curatedPresets,
    playlist: (state.playlist || []).map((t) => ({
      id: t.id,
      file: t.file,
      metadata: t.metadata
        ? {
            title: t.metadata.title,
            artist: t.metadata.artist,
            album: t.metadata.album,
            duration: t.metadata.duration,
            sampleRate: t.metadata.sampleRate,
            numberOfChannels: t.metadata.numberOfChannels,
            bitrate: t.metadata.bitrate,
            format: t.metadata.format
          }
        : null
    })),
    metadata: state.metadata
      ? {
          title: state.metadata.title,
          artist: state.metadata.artist,
          album: state.metadata.album,
          duration: state.metadata.duration,
          sampleRate: state.metadata.sampleRate,
          numberOfChannels: state.metadata.numberOfChannels,
          bitrate: state.metadata.bitrate,
          format: state.metadata.format
        }
      : null
  }

  const payloadJson = JSON.stringify(payload)
  if (payloadJson !== lastSyncPayload) {
    lastSyncPayload = payloadJson
    window.api?.metadata?.sendState(payload)
  }
})

window.api?.metadata?.onStateSync((newState) => {
  isInternalUpdate = true
  usePlayerStore.setState(newState)
  isInternalUpdate = false
})

// Initial config load
;(async () => {
  const config = await window.api?.metadata?.getConfig()
  if (config) {
    isInternalUpdate = true
    usePlayerStore.setState({
      volume: config.volume ?? 0.8,
      eqEnabled: config.eqEnabled ?? false,
      preamp: config.preamp ?? -3,
      eqBands: config.eqBands ?? [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      surroundMode: normalizeSurroundMode(config.surroundMode),
      curatedPresets: config.curatedPresets ?? {}
    })
    isInternalUpdate = false
  }

  // Also load visuals settings if available
  const visualsConfig = await window.api?.visuals?.getSettings()
  if (visualsConfig) {
    isInternalUpdate = true
    usePlayerStore.setState({
      curatedPresets: visualsConfig.curatedPresets ?? {}
    })
    isInternalUpdate = false
  }
})()
