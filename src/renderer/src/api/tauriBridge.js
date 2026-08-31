import { invoke } from '@tauri-apps/api/core'
import { listen } from '@tauri-apps/api/event'
import { readText } from '@tauri-apps/plugin-clipboard-manager'
import { open } from '@tauri-apps/plugin-dialog'

export const initTauriBridge = () => {
  if (typeof window === 'undefined') return

  if (!window.api) {
    window.api = {
      windowManager: {
        toggle: async (name) => {
          try {
            await invoke('toggle_window', { label: name })
          } catch (e) {
            console.warn('[Tauri] toggle_window error:', e)
          }
        },
        closeCurrent: () => {},
        toggleFullscreen: () => {}
      },
      metadata: {
        getPath: (file) => file.path || file.name,
        parse: async () => null,
        openFile: async () => {
          const selected = await open({
            multiple: false,
            filters: [{ name: 'Audio', extensions: ['mp3', 'flac', 'wav', 'ogg', 'm4a', 'aac'] }]
          })
          return selected
        },
        openMultipleFiles: async () => {
          const selected = await open({
            multiple: true,
            filters: [{ name: 'Audio', extensions: ['mp3', 'flac', 'wav', 'ogg', 'm4a', 'aac'] }]
          })
          return selected
        },
        sendState: () => {},
        getConfig: async () => ({}),
        onStateSync: () => () => {}
      },
      nativeSurround: {
        start: async () => {},
        stop: async () => {},
        setPaused: async () => {},
        seek: async () => {},
        setVolume: async () => {},
        status: async () => ({}),
        onAudioData: () => () => {},
        onPosition: () => () => {},
        onEnded: () => () => {}
      },
      visuals: {
        getPresets: async () => [],
        getSettings: async () => ({}),
        saveSettings: async () => {},
        restart: async (config) => invoke('launch_projectm', { config }),
        stop: async () => invoke('stop_projectm'),
        getStatus: async () => ({ isRunning: false }),
        getAudioDevices: async () => [],
        getClipboardText: async () => {
          try {
            return await readText()
          } catch {
            return ''
          }
        },
        collectPreset: async () => {},
        collectFavorites: async () => {},
        getPackStatus: async () => invoke('get_preset_pack_status'),
        downloadPack: async () => invoke('download_preset_pack'),
        onPackProgress: (callback) => {
          let unlisten = null
          listen('preset-pack-progress', (event) => callback(event.payload)).then((fn) => {
            unlisten = fn
          })
          return () => {
            if (unlisten) unlisten()
          }
        }
      },
      playlists: {
        list: async () => [],
        save: async () => {},
        load: async () => [],
        remove: async () => {},
        saveToFile: async () => {},
        loadFromFile: async () => []
      }
    }
  }
}

initTauriBridge()
