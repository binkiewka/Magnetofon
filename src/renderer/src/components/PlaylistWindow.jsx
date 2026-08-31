import { Music, Trash2, Save, FolderOpen, Library, X, Plus, PlayCircle } from 'lucide-react'
import { useEffect, useState } from 'react'
import { usePlayerStore } from '../store/playerStore'

export function PlaylistWindow() {
  const playlist = usePlayerStore((s) => s.playlist)
  const activeFile = usePlayerStore((s) => s.file)
  const currentPlaylistName = usePlayerStore((s) => s.currentPlaylistName)
  const setFile = usePlayerStore((s) => s.setFile)
  const setIsPlaying = usePlayerStore((s) => s.setIsPlaying)
  const removeFromPlaylist = usePlayerStore((s) => s.removeFromPlaylist)
  const clearPlaylist = usePlayerStore((s) => s.clearPlaylist)
  const setPlaylist = usePlayerStore((s) => s.setPlaylist)
  const addToPlaylist = usePlayerStore((s) => s.addToPlaylist)
  const [savedPlaylists, setSavedPlaylists] = useState([])

  useEffect(() => {
    let cancelled = false

    const loadPlaylists = async () => {
      try {
        const names = await window.api.playlists.list()
        if (!cancelled) setSavedPlaylists(names)
      } catch (err) {
        console.error('Failed to load saved playlists', err)
      }
    }

    loadPlaylists()
    return () => {
      cancelled = true
    }
  }, [])

  const handleTrackClick = (track) => {
    setFile(track.file, track.metadata)
    setIsPlaying(true)
  }

  const handleSavePlaylist = async () => {
    await window.api.playlists.saveToFile(playlist)
  }

  const handleLoadPlaylist = async () => {
    const data = await window.api.playlists.loadFromFile()
    if (data) {
      setPlaylist(data.playlist, data.name)
    }
  }

  const handleAddFiles = async () => {
    const paths = await window.api.metadata.openMultipleFiles()
    if (!paths || paths.length === 0) return

    const newTracks = await Promise.all(
      paths.map(async (p) => {
        const metadata = await window.api.metadata.parse(p)
        return { path: p, meta: metadata }
      })
    )

    newTracks.forEach((track) => addToPlaylist(track))
  }

  return (
    <div
      className="metallic-surface"
      style={{
        width: '100vw',
        height: '100vh',
        boxSizing: 'border-box',
        borderRadius: '8px',
        padding: '28px',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        position: 'relative'
      }}
    >
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'flex-start',
          marginBottom: '24px',
          WebkitAppRegion: 'drag',
          cursor: 'grab'
        }}
      >
        <div style={{ display: 'flex', flexDirection: 'column' }}>
          <h1
            style={{
              margin: 0,
              fontSize: '18px',
              fontWeight: 600,
              color: 'var(--primary)',
              letterSpacing: '1px'
            }}
          >
            PLAYLIST
          </h1>
        </div>

        <div
          style={{ display: 'flex', alignItems: 'center', gap: '8px', WebkitAppRegion: 'no-drag' }}
        >
          <button
            onClick={handleAddFiles}
            style={{
              background: 'var(--surface-container-high)',
              border: 'none',
              color: 'var(--primary-fixed)',
              borderRadius: '2px',
              padding: '6px 14px',
              cursor: 'pointer',
              fontSize: '11px',
              fontFamily: 'var(--font-display)',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}
          >
            <Plus size={14} /> ADD
          </button>

          <button
            onClick={handleLoadPlaylist}
            style={{
              background: 'var(--surface-container-high)',
              border: 'none',
              color: 'var(--primary-fixed)',
              borderRadius: '2px',
              padding: '6px 14px',
              cursor: 'pointer',
              fontSize: '11px',
              fontFamily: 'var(--font-display)',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}
          >
            <FolderOpen size={14} /> LOAD
          </button>

          <button
            onClick={handleSavePlaylist}
            style={{
              background: 'var(--surface-container-high)',
              border: 'none',
              color: 'var(--secondary)',
              borderRadius: '2px',
              padding: '6px 14px',
              cursor: 'pointer',
              fontSize: '11px',
              fontFamily: 'var(--font-display)',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}
          >
            <Save size={14} /> SAVE
          </button>

          <button
            onClick={() => window.api?.windowManager?.closeCurrent()}
            style={{
              background: 'rgba(255,255,255,0.06)',
              color: 'var(--on-surface-variant)',
              border: 'none',
              borderRadius: '2px',
              width: '28px',
              height: '28px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              marginLeft: '8px'
            }}
          >
            <X size={16} />
          </button>
        </div>
      </div>

      {savedPlaylists.length > 0 && (
        <div
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            marginBottom: '20px',
            fontSize: '10px',
            color: 'var(--on-surface-variant)',
            WebkitAppRegion: 'no-drag',
            overflowX: 'auto',
            paddingBottom: '4px'
          }}
        >
          <Library size={12} style={{ opacity: 0.4 }} />
          {savedPlaylists.map((name) => (
            <button
              key={name}
              onClick={async () => {
                const data = await window.api.playlists.load(name)
                setPlaylist(data.playlist, data.name)
              }}
              style={{
                background:
                  name === currentPlaylistName
                    ? 'var(--primary-container)'
                    : 'rgba(255,255,255,0.03)',
                color:
                  name === currentPlaylistName
                    ? 'var(--on-primary-container)'
                    : 'var(--on-surface-variant)',
                border: 'none',
                borderRadius: '2px',
                padding: '4px 12px',
                cursor: 'pointer',
                whiteSpace: 'nowrap',
                fontSize: '10px',
                fontFamily: 'var(--font-display)',
                letterSpacing: '1px'
              }}
            >
              {name}
            </button>
          ))}
        </div>
      )}

      <div
        className="vintage-bay"
        style={{
          flex: 1,
          overflowY: 'auto',
          background: 'rgba(0,0,0,0.15)',
          borderRadius: '8px',
          padding: '8px',
          marginBottom: '20px',
          boxShadow: 'inset 0 2px 8px rgba(0,0,0,0.3)'
        }}
      >
        {playlist.length === 0 ? (
          <div
            style={{
              height: '100%',
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              color: 'var(--on-surface-variant)',
              opacity: 0.2
            }}
          >
            <Music size={48} strokeWidth={1} style={{ marginBottom: '16px' }} />
            <div
              style={{ fontSize: '10px', letterSpacing: '2px', fontFamily: 'var(--font-display)' }}
            >
              BUFFER_EMPTY
            </div>
          </div>
        ) : (
          playlist.map((t, idx) => {
            const isActive = t.file === activeFile
            const mins = Math.floor((t.metadata?.duration || 0) / 60)
            const secs = Math.floor((t.metadata?.duration || 0) % 60)
            const timeStr = `${mins}:${secs.toString().padStart(2, '0')}`

            return (
              <div
                key={t.id || t.file}
                onClick={() => handleTrackClick(t)}
                className={isActive ? 'indicator-active' : ''}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  padding: '12px 18px',
                  background: isActive ? 'rgba(0,255,157,0.05)' : 'transparent',
                  marginBottom: '2px',
                  borderRadius: '4px',
                  cursor: 'pointer',
                  position: 'relative',
                  transition: 'background 0.2s'
                }}
              >
                <div
                  style={{
                    width: '24px',
                    fontSize: '10px',
                    color: 'var(--on-surface-variant)',
                    opacity: 0.4,
                    fontFamily: 'var(--font-display)'
                  }}
                >
                  {String(idx + 1).padStart(2, '0')}
                </div>

                <div style={{ flex: 1, overflow: 'hidden' }}>
                  <div
                    style={{
                      fontSize: '13px',
                      color: isActive ? 'var(--primary-fixed)' : 'var(--on-surface)',
                      fontWeight: isActive ? 600 : 400,
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis'
                    }}
                  >
                    {t.metadata?.title || t.file.split('/').pop()}
                  </div>
                  <div
                    style={{
                      fontSize: '10px',
                      color: 'var(--on-surface-variant)',
                      opacity: 0.6,
                      marginTop: '2px',
                      whiteSpace: 'nowrap',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis'
                    }}
                  >
                    {t.metadata?.artist || 'UNKNOWN'} • {t.metadata?.format?.toUpperCase() || 'PCM'}
                  </div>
                </div>

                {/* Management Controls - Visible on row */}
                <div style={{ display: 'flex', gap: '4px', opacity: 0.6, marginRight: '16px' }}>
                  <button
                    onClick={(e) => {
                      e.stopPropagation()
                      removeFromPlaylist(t.id || t.file)
                    }}
                    style={{
                      background: 'rgba(255,0,0,0.1)',
                      border: 'none',
                      color: 'var(--error)',
                      borderRadius: '2px',
                      cursor: 'pointer',
                      padding: '6px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center'
                    }}
                    title="Remove track"
                  >
                    <Trash2 size={14} />
                  </button>
                </div>

                <div
                  style={{
                    width: '100px',
                    textAlign: 'right',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'flex-end',
                    gap: '12px'
                  }}
                >
                  {isActive && (
                    <div style={{ color: 'var(--primary-fixed)' }}>
                      <PlayCircle size={14} />
                    </div>
                  )}
                  <div
                    style={{
                      fontSize: '12px',
                      color: 'var(--on-surface-variant)',
                      opacity: 0.6,
                      fontFamily: 'var(--font-display)'
                    }}
                  >
                    {timeStr}
                  </div>
                </div>

                <style>{`
                  div:hover .track-controls { opacity: 0.8; }
                `}</style>
              </div>
            )
          })
        )}
      </div>

      {/* Footer - Glassmorphic Info Bar */}
      <div
        className="glass-panel"
        style={{
          position: 'relative',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          padding: '12px 20px',
          borderRadius: '10px'
        }}
      >
        <div style={{ display: 'flex', gap: '12px' }}>
          <div
            style={{
              fontSize: '10px',
              color: 'var(--on-surface-variant)',
              opacity: 0.4,
              fontFamily: 'var(--font-display)',
              letterSpacing: '1px'
            }}
          >
            COUNT: {playlist.length} TRACKS
          </div>
        </div>

        <button
          onClick={() => {
            if (confirm('Purge track playlist?')) clearPlaylist()
          }}
          style={{
            background: 'transparent',
            border: 'none',
            color: 'var(--error-container)',
            cursor: 'pointer',
            fontSize: '10px',
            fontFamily: 'var(--font-display)',
            letterSpacing: '1px'
          }}
        >
          CLEAR
        </button>
      </div>
    </div>
  )
}
