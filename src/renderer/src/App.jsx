import { useEffect, useState } from 'react'
import { HifiSystem } from './components/HifiSystem'
import { PlaylistWindow } from './components/PlaylistWindow'
import { EqualizerWindow } from './components/EqualizerWindow'
import { usePlayerStore } from './store/playerStore'

function App() {
  const [hash, setHash] = useState(window.location.hash)

  useEffect(() => {
    const handleHashChange = () => setHash(window.location.hash)
    window.addEventListener('hashchange', handleHashChange)

    const importAudioFiles = async (paths, playFirst = false) => {
      const uniquePaths = [...new Set(Array.isArray(paths) ? paths : [])].filter(Boolean)
      if (!uniquePaths.length) return

      const tracks = []
      for (const path of uniquePaths) {
        const meta = await window.api.metadata.parse(path)
        tracks.push({ path, meta })
        usePlayerStore.getState().addToPlaylist({ path, meta })
      }

      const first = tracks[0]
      if (first && (playFirst || !usePlayerStore.getState().file)) {
        usePlayerStore.getState().setFile(first.path, first.meta)
        if (playFirst) usePlayerStore.getState().setIsPlaying(true)
      }
    }

    let stopOpenFileListener = () => {}
    const isPlayerWindow = window.location.hash === '#player' || window.location.hash === ''

    if (isPlayerWindow) {
      stopOpenFileListener = window.api.metadata.onOpenFiles((paths) => {
        importAudioFiles(paths, true).catch((err) => {
          console.error('Failed to open audio files from the operating system', err)
        })
      })

      window.api.metadata
        .getLaunchFiles()
        .then((paths) => importAudioFiles(paths, true))
        .catch((err) => console.error('Failed to load launch audio files', err))
    }

    const isExternalFileDrag = (e) => {
      const types = Array.from(e.dataTransfer?.types || [])
      return types.includes('Files') || types.includes('text/uri-list')
    }

    const preventDrag = (e) => {
      if (isExternalFileDrag(e)) {
        e.preventDefault()
      }
    }

    // Global drop trap for external files bypassing React SyntheticEvents entirely
    const handleGlobalDrop = async (e) => {
      if (!isExternalFileDrag(e)) return

      e.preventDefault()
      e.stopPropagation()

      let paths = []
      const files = Array.from(e.dataTransfer?.files || [])

      if (files.length > 0 && window.api?.metadata?.getPath) {
        paths = files.map((f) => window.api.metadata.getPath(f)).filter((p) => p)
      } else {
        const uriList = e.dataTransfer?.getData('text/uri-list')
        if (uriList) {
          paths = uriList
            .split('\n')
            .map((u) => u.trim())
            .filter((u) => u && !u.startsWith('#'))
            .map((u) => u.replace(/^file:\/\//, ''))
            .map((u) => {
              try {
                return decodeURI(u)
              } catch {
                return u
              }
            })
        }
      }

      await importAudioFiles(paths)
    }

    window.addEventListener('dragover', preventDrag, { capture: true })
    window.addEventListener('dragenter', preventDrag, { capture: true })
    window.addEventListener('drop', handleGlobalDrop, { capture: true })

    return () => {
      window.removeEventListener('hashchange', handleHashChange)
      stopOpenFileListener()
      window.removeEventListener('dragover', preventDrag, { capture: true })
      window.removeEventListener('dragenter', preventDrag, { capture: true })
      window.removeEventListener('drop', handleGlobalDrop, { capture: true })
    }
  }, [])

  if (hash === '#playlist') return <PlaylistWindow />
  if (hash === '#equalizer') return <EqualizerWindow />
  // Visuals are native projectM now; no browser visualizer route.
  // Default is the unified hi-fi system. Legacy panel routes remain as dev fallbacks.
  return <HifiSystem />
}

export default App
