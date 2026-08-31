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

      const store = usePlayerStore.getState()

      for (const [index, path] of paths.entries()) {
        if (!window.api?.metadata) continue
        const meta = await window.api.metadata.parse(path)

        store.addToPlaylist({ path, meta })
        if (!store.file && index === 0) {
          store.setFile(path, meta)
        }
      }
    }

    window.addEventListener('dragover', preventDrag, { capture: true })
    window.addEventListener('dragenter', preventDrag, { capture: true })
    window.addEventListener('drop', handleGlobalDrop, { capture: true })

    return () => {
      window.removeEventListener('hashchange', handleHashChange)
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
