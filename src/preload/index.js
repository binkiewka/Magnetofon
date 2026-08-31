import { contextBridge, ipcRenderer, webUtils } from 'electron'

// Custom APIs for renderer
const api = {
  windowManager: {
    toggle: (name) => ipcRenderer.send('toggle-window', name),
    closeCurrent: () => ipcRenderer.send('close-current-window'),
    toggleFullscreen: () => ipcRenderer.send('toggle-fullscreen')
  },
  metadata: {
    getPath: (file) => webUtils.getPathForFile(file),
    parse: (filePath) => ipcRenderer.invoke('parse-metadata', filePath),
    openFile: () => ipcRenderer.invoke('open-file').then((res) => (res ? res[0] : null)),
    openMultipleFiles: () => ipcRenderer.invoke('open-file'),
    sendState: (state) => ipcRenderer.send('sync-state', state),
    getConfig: () => ipcRenderer.invoke('get-settings'),
    onStateSync: (callback) => {
      const subscription = (_event, state) => callback(state)
      ipcRenderer.on('state-synced', subscription)
      return () => ipcRenderer.removeListener('state-synced', subscription)
    }
  },
  nativeSurround: {
    start: (options) => ipcRenderer.invoke('native-surround-start', options),
    stop: () => ipcRenderer.invoke('native-surround-stop'),
    setPaused: (paused) => ipcRenderer.invoke('native-surround-set-paused', paused),
    seek: (time) => ipcRenderer.invoke('native-surround-seek', time),
    setVolume: (volume) => ipcRenderer.invoke('native-surround-set-volume', volume),
    status: () => ipcRenderer.invoke('native-surround-status'),
    onAudioData: (callback) => {
      const subscription = (_event, data) => callback(data)
      ipcRenderer.on('native-surround-audio-data', subscription)
      return () => ipcRenderer.removeListener('native-surround-audio-data', subscription)
    },
    onPosition: (callback) => {
      const subscription = (_event, data) => callback(data)
      ipcRenderer.on('native-surround-position', subscription)
      return () => ipcRenderer.removeListener('native-surround-position', subscription)
    },
    onEnded: (callback) => {
      const subscription = (_event, data) => callback(data)
      ipcRenderer.on('native-surround-ended', subscription)
      return () => ipcRenderer.removeListener('native-surround-ended', subscription)
    }
  },
  visuals: {
    getPresets: (mode) => ipcRenderer.invoke('get-presets-list', mode),
    getSettings: () => ipcRenderer.invoke('get-visuals-settings'),
    saveSettings: (settings) => ipcRenderer.invoke('save-visuals-settings', settings),
    restart: () => ipcRenderer.invoke('restart-native-projectm'),
    stop: () => ipcRenderer.invoke('stop-native-projectm'),
    getStatus: () => ipcRenderer.invoke('get-visuals-status'),
    getAudioDevices: () => ipcRenderer.invoke('get-audio-devices'),
    getClipboardText: () => ipcRenderer.invoke('get-clipboard-text'),
    collectPreset: (filename) => ipcRenderer.invoke('collect-preset', filename),
    collectFavorites: () => ipcRenderer.invoke('collect-favorites'),
    getPackStatus: () => ipcRenderer.invoke('get-preset-pack-status'),
    downloadPack: () => ipcRenderer.invoke('download-preset-pack'),
    onPackProgress: (callback) => {
      const subscription = (_event, data) => callback(data)
      ipcRenderer.on('preset-pack-progress', subscription)
      return () => ipcRenderer.removeListener('preset-pack-progress', subscription)
    }
  },
  playlists: {
    list: () => ipcRenderer.invoke('list-saved-playlists'),
    save: (name, playlist) => ipcRenderer.invoke('save-playlist', { name, playlist }),
    load: (name) => ipcRenderer.invoke('load-playlist', name),
    remove: (name) => ipcRenderer.invoke('delete-playlist', name),
    saveToFile: (playlist) => ipcRenderer.invoke('save-playlist-file', playlist),
    loadFromFile: () => ipcRenderer.invoke('load-playlist-file')
  }
}

if (process.contextIsolated) {
  try {
    contextBridge.exposeInMainWorld('api', api)
  } catch (error) {
    console.error('[Preload] Failed to expose contextBridge API:', error)
  }
} else {
  window.api = api
}
