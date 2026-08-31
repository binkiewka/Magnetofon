/* eslint-disable react/prop-types */
import { useEffect, useState, useRef } from 'react'
import {
  ChevronDown,
  FolderOpen,
  Library,
  Pause,
  Play,
  PlayCircle,
  Plus,
  Power,
  Save,
  SkipBack,
  SkipForward,
  Square,
  Trash2,
  X,
  Volume2,
  Sliders,
  Disc,
  Radio,
  Maximize2,
  Minimize2,
  GripVertical
} from 'lucide-react'
import { engine } from '../audio/audioEngine'
import { usePlayerStore } from '../store/playerStore'
import { CassetteDeck } from './CassetteDeck'
import { VuMeter } from './VuMeter'
import { Knob } from './Knob'
import { PLACEHOLDER_ALBUM_ART } from '../assets/placeholderArt'
import { VisualsControlPanel } from './VisualsControlPanel'
import '../assets/hifi.css'

const eqBandsList = ['31', '62', '125', '250', '500', '1k', '2k', '4k', '8k', '16k']

const presets = {
  FLAT: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  ROCK: [4, 3, 2, 0, -1, -1, 0, 2, 3, 4],
  POP: [-1, 0, 2, 3, 4, 3, 2, 0, -1, -1],
  CLASSICAL: [4, 3, 2, 2, 0, 0, 0, 2, 3, 3],
  JAZZ: [3, 2, 1, 2, -1, -1, 0, 1, 2, 3],
  'BASS BOOST': [6, 5, 4, 2, 0, 0, 0, 0, 0, 0]
}

function getAudioFormatLabel(metadata, filePath) {
  const codecOrFormat = `${metadata?.format || ''} ${metadata?.codec || ''}`.toLowerCase()
  const isFlac = codecOrFormat.includes('flac') || filePath?.toLowerCase().endsWith('.flac')

  if (isFlac) {
    const isHighRes =
      (metadata?.bitsPerSample && metadata.bitsPerSample > 16) ||
      (metadata?.sampleRate && metadata.sampleRate > 44100)
    return isHighRes ? 'HQ FLAC' : 'FLAC'
  }

  return metadata?.format?.toUpperCase() || 'PCM STEREO'
}

function HifiButton({ children, className = '', ...props }) {
  return (
    <button className={`hifi-button ${className}`} {...props}>
      {children}
    </button>
  )
}

function EqSlider({ value, onChange, isSecondary = false }) {
  const normalized = (value + 12) / 24

  const updateValue = (e) => {
    const rect = e.currentTarget.getBoundingClientRect()
    const y = Math.max(0, Math.min(rect.height, e.clientY - rect.top))
    onChange((1 - y / rect.height) * 24 - 12)
  }

  return (
    <div
      className="hifi-eq-slider"
      onPointerDown={(e) => {
        e.currentTarget.setPointerCapture(e.pointerId)
        updateValue(e)
      }}
      onPointerMove={(e) => {
        if (e.buttons) updateValue(e)
      }}
    >
      <div className="hifi-eq-rail" />
      <div
        className={isSecondary ? 'hifi-eq-fill is-secondary' : 'hifi-eq-fill'}
        style={{ height: `${normalized * 100}%` }}
      />
      <div className="hifi-eq-thumb" style={{ bottom: `${normalized * 100}%` }}>
        <div className="eq-thumb-led" />
      </div>
    </div>
  )
}

/** High-DPI Razor-Sharp Centered 16-Bar Real-time FFT Spectrum Analyzer (McIntosh Sapphire Blue) */
function SpectrumAnalyzer({ isPlaying }) {
  const containerRef = useRef(null)
  const canvasRef = useRef(null)
  const animRef = useRef(null)

  useEffect(() => {
    const canvas = canvasRef.current
    const container = containerRef.current
    if (!canvas || !container) return
    const ctx = canvas.getContext('2d')
    let running = true

    const updateSize = () => {
      const dpr = window.devicePixelRatio || 1
      const rect = container.getBoundingClientRect()
      const w = Math.max(80, Math.floor(rect.width))
      const h = Math.max(16, Math.floor(rect.height))

      if (canvas.width !== w * dpr || canvas.height !== h * dpr) {
        canvas.width = w * dpr
        canvas.height = h * dpr
      }
    }

    updateSize()
    const resizeObserver = new ResizeObserver(() => updateSize())
    resizeObserver.observe(container)

    const bars = 16
    const levels = new Array(bars).fill(0)

    const render = () => {
      if (!running) return

      const dpr = window.devicePixelRatio || 1
      const rect = container.getBoundingClientRect()
      const w = Math.floor(rect.width)
      const h = Math.floor(rect.height)

      ctx.save()
      ctx.scale(dpr, dpr)
      ctx.clearRect(0, 0, w, h)

      let freqData = null
      if (isPlaying && engine.hasFreshNativeAudio()) {
        freqData = engine.getFrequencyData()
      }

      const barWidth = Math.floor((w - (bars - 1) * 3) / bars)
      const totalWidth = bars * barWidth + (bars - 1) * 3
      const offsetX = Math.max(0, Math.floor((w - totalWidth) / 2))

      for (let i = 0; i < bars; i++) {
        let target = 0
        if (freqData) {
          const sampleIdx = Math.floor((i / bars) * Math.min(256, freqData.length))
          target = freqData[sampleIdx] / 255
        } else if (isPlaying) {
          const time = Date.now() / 180 + i * 0.4
          target = Math.max(0.04, 0.35 + 0.3 * Math.sin(time) + 0.2 * Math.cos(time * 1.7))
        }

        levels[i] += (target - levels[i]) * 0.35
        const barH = Math.max(2, Math.floor(levels[i] * (h - 2)))
        const x = offsetX + i * (barWidth + 3)

        const segments = 8
        const segH = Math.max(2, Math.floor((h - 2) / segments))

        for (let s = 0; s < segments; s++) {
          const segY = h - (s + 1) * (segH + 1)
          if (segY < h - barH) continue

          let color = '#0077ff'
          if (s >= 3) color = '#00a8ff'
          if (s >= 6) color = '#00e5ff'
          if (s >= 8) color = '#ff3366'

          ctx.fillStyle = color
          ctx.fillRect(x, segY, barWidth, segH)
        }
      }

      ctx.restore()
      animRef.current = requestAnimationFrame(render)
    }

    render()
    return () => {
      running = false
      resizeObserver.disconnect()
      if (animRef.current) cancelAnimationFrame(animRef.current)
    }
  }, [isPlaying])

  return (
    <div className="hifi-spectrum-bay" ref={containerRef}>
      <canvas ref={canvasRef} className="hifi-spectrum-canvas" />
    </div>
  )
}

function formatTime(seconds) {
  if (!seconds || isNaN(seconds)) return '00:00'
  const m = Math.floor(seconds / 60)
    .toString()
    .padStart(2, '0')
  const s = Math.floor(seconds % 60)
    .toString()
    .padStart(2, '0')
  return `${m}:${s}`
}

/* ==========================================================================
   ROW 1 LEFT: SLICK AMPLIFIER PANEL
   ========================================================================== */
function AmplifierPanel() {
  const volume = usePlayerStore((s) => s.volume)
  const setVolume = usePlayerStore((s) => s.setVolume)
  const surroundMode = usePlayerStore((s) => s.surroundMode)
  const setSurroundMode = usePlayerStore((s) => s.setSurroundMode)
  const volDb = volume === 0 ? '-INF' : `${Math.round((volume - 1) * 60)}dB`

  return (
    <section className="hifi-panel hifi-row1-panel hifi-amp-panel">
      <div className="hifi-panel-header">
        <div className="hifi-header-title">
          <Volume2 size={15} className="hifi-header-icon" />
          <span>POWER STAGE</span>
        </div>
      </div>

      <div className="hifi-amp-body">
        <div className="hifi-volume-stage">
          <Knob
            value={volume}
            min={0}
            max={1}
            step={0.01}
            onChange={(v) => setVolume(v)}
            onReset={() => setVolume(0.7)}
            size={60}
            label="MASTER VOLUME"
            displayValue={volDb}
          />
        </div>

        <div className="hifi-surround-stage">
          <span className="stage-label">PLAYBACK MODE</span>
          <div className="hifi-mode-selector">
            {['AUTO', 'STEREO', 'SURROUND'].map((mode) => (
              <button
                key={mode}
                className={surroundMode === mode ? 'mode-btn active' : 'mode-btn'}
                onClick={() => setSurroundMode(mode)}
              >
                {mode}
              </button>
            ))}
          </div>
        </div>
      </div>
    </section>
  )
}

/* ==========================================================================
   ROW 1 RIGHT: COMPACT GRAPHIC EQUALIZER PANEL
   ========================================================================== */
function EqualizerPanel() {
  const eqEnabled = usePlayerStore((s) => s.eqEnabled)
  const setEqEnabled = usePlayerStore((s) => s.setEqEnabled)
  const preamp = usePlayerStore((s) => s.preamp)
  const setPreamp = usePlayerStore((s) => s.setPreamp)
  const eqBands = usePlayerStore((s) => s.eqBands)
  const setEqBand = usePlayerStore((s) => s.setEqBand)
  const isPlaying = usePlayerStore((s) => s.isPlaying)

  return (
    <section className="hifi-panel hifi-row1-panel hifi-eq-panel">
      <div className="hifi-panel-header">
        <div className="hifi-header-title">
          <Sliders size={15} className="hifi-header-icon" />
          <span>10 BAND FREQUENCY PROCESSOR</span>
        </div>
        <div className="hifi-eq-actions">
          <div className="hifi-select-wrap">
            <select
              onChange={(e) => {
                const preset = presets[e.target.value]
                if (preset) preset.forEach((val, idx) => setEqBand(idx, val))
              }}
            >
              <option value="CUSTOM">PRESET: CUSTOM</option>
              {Object.keys(presets).map((name) => (
                <option key={name} value={name}>
                  {name}
                </option>
              ))}
            </select>
            <ChevronDown size={12} />
          </div>
          <HifiButton
            className={eqEnabled ? 'power on' : 'power'}
            onClick={() => setEqEnabled(!eqEnabled)}
          >
            <Power size={12} /> {eqEnabled ? 'ON' : 'BYPASS'}
          </HifiButton>
        </div>
      </div>

      <SpectrumAnalyzer isPlaying={isPlaying} />

      <div className={eqEnabled ? 'hifi-eq-bay' : 'hifi-eq-bay disabled'}>
        <div className="hifi-eq-band preamp">
          <EqSlider value={preamp} onChange={(v) => eqEnabled && setPreamp(v)} isSecondary />
          <span>GAIN</span>
        </div>
        {eqBandsList.map((band, idx) => (
          <div className="hifi-eq-band" key={band}>
            <EqSlider value={eqBands[idx]} onChange={(v) => eqEnabled && setEqBand(idx, v)} />
            <span>{band}</span>
          </div>
        ))}
      </div>
    </section>
  )
}

/* ==========================================================================
   ROW 2 LEFT: VU METER PANEL
   ========================================================================== */
function VuMeterPanel() {
  const isPlaying = usePlayerStore((s) => s.isPlaying)

  return (
    <section className="hifi-panel hifi-row2-panel hifi-vu-panel">
      <div className="hifi-panel-header">
        <div className="hifi-header-title">
          <Radio size={15} className="hifi-header-icon" />
          <span>TWIN SIGNAL LEVEL MONITOR</span>
        </div>
      </div>

      <VuMeter isPlaying={isPlaying} />
    </section>
  )
}

/* ==========================================================================
   ROW 2 RIGHT: PROGRAM MONITOR PANEL (Fail-Safe Artwork & Slow Marquee Scroll)
   ========================================================================== */
function ProgramMonitorPanel() {
  const file = usePlayerStore((s) => s.file)
  const metadata = usePlayerStore((s) => s.metadata)
  const duration = usePlayerStore((s) => s.duration)
  const progress = usePlayerStore((s) => s.progress)
  const setProgress = usePlayerStore((s) => s.setProgress)

  const title = file
    ? metadata?.title ||
      file
        .split(/[\\/]/)
        .pop()
        ?.replace(/\.[^.]+$/, '')
    : 'READY FOR INPUT'
  const artist = metadata?.artist || 'UNKNOWN ARTIST'
  const album = metadata?.album || 'MAGNETOFON SYSTEM'
  const formatLabel = getAudioFormatLabel(metadata, file)
  const isHqFlac = formatLabel === 'HQ FLAC'

  const sampleRateStr = metadata?.sampleRate
    ? `${(metadata.sampleRate / 1000).toFixed(1)} kHz`
    : '44.1 kHz'
  const bitrateStr = metadata?.bitrate
    ? metadata.bitrate > 1000000
      ? `${(metadata.bitrate / 1000000).toFixed(1)} Mbps`
      : `${Math.round(metadata.bitrate / 1000)} kbps`
    : '320 kbps'
  const bitsStr = metadata?.bitsPerSample ? `${metadata.bitsPerSample}-BIT` : '16-BIT'
  const yearStr = metadata?.year ? ` • ${metadata.year}` : ''
  const trackNoStr = metadata?.trackNo ? `TRK ${String(metadata.trackNo).padStart(2, '0')}` : ''

  const progressIndex = duration > 0 ? Math.floor((progress / duration) * 44) : 0

  // Marquee Measurement State
  const containerRef = useRef(null)
  const textRef = useRef(null)
  const [isMarquee, setIsMarquee] = useState(false)
  const [titleLength, setTitleLength] = useState(0)

  useEffect(() => {
    const checkOverflow = () => {
      const container = containerRef.current
      const textEl = textRef.current
      if (container && textEl) {
        const fullW = textEl.scrollWidth
        const containerW = container.clientWidth
        if (fullW > containerW + 4) {
          setIsMarquee(true)
          setTitleLength(fullW)
        } else {
          setIsMarquee(false)
          setTitleLength(0)
        }
      }
    }

    checkOverflow()
    const timer = setTimeout(checkOverflow, 120)
    window.addEventListener('resize', checkOverflow)
    return () => {
      clearTimeout(timer)
      window.removeEventListener('resize', checkOverflow)
    }
  }, [title])

  // Slower, smooth luxury marquee scroll speed (22-26s)
  const marqueeDuration = Math.max(22, Math.round((titleLength * 2) / 18))

  return (
    <section className="hifi-panel hifi-row2-panel hifi-monitor-panel">
      <div className="hifi-panel-header">
        <div className="hifi-header-title">
          <Disc size={15} className="hifi-header-icon" />
          <span>INPUT & ACTIVE TRACK DECK</span>
        </div>
        <div className={`hifi-format-badge ${isHqFlac ? 'hq-flac' : ''}`}>{formatLabel}</div>
      </div>

      <div className="hifi-vfd-display vfd-4col">
        {/* Col 1: Artwork / Fail-Safe Custom Base64 Placeholder Image */}
        <div className="vfd-artwork-bay">
          <img
            src={metadata?.picture || PLACEHOLDER_ALBUM_ART}
            onError={(e) => {
              e.currentTarget.onerror = null
              e.currentTarget.src = PLACEHOLDER_ALBUM_ART
            }}
            alt="Album Art"
            className="vfd-artwork-img"
          />
        </div>

        {/* Col 2: Artist & Title (Continuous Slow 1-Direction Infinite Marquee Ticker) */}
        <div className="vfd-track-details">
          <div className="vfd-title-wrapper" ref={containerRef}>
            {isMarquee ? (
              <div
                className="vfd-title-marquee-track"
                style={{ animationDuration: `${marqueeDuration}s` }}
              >
                <strong className="vfd-title-text">{title} &nbsp;&nbsp;✦&nbsp;&nbsp;&nbsp;</strong>
                <strong className="vfd-title-text">{title} &nbsp;&nbsp;✦&nbsp;&nbsp;&nbsp;</strong>
              </div>
            ) : (
              <strong ref={textRef} className="vfd-title-text static" title={title}>
                {title}
              </strong>
            )}
          </div>
          <small className="vfd-artist-text">
            {artist} • {album}
          </small>
          {(trackNoStr || yearStr) && (
            <span className="vfd-meta-tag">
              {trackNoStr}
              {yearStr}
            </span>
          )}
        </div>

        {/* Col 3: Time Counter */}
        <div className="vfd-time-counter">
          <span>{formatTime(progress)}</span>
          <small>/ {formatTime(duration)}</small>
        </div>

        {/* Col 4: Vertical Tech Specs Stack (kHz, Bit, Bitrate) */}
        <div className="vfd-vertical-specs">
          <span className={`spec-val primary ${isHqFlac ? 'hq' : ''}`}>{sampleRateStr}</span>
          <span className={`spec-val ${isHqFlac ? 'hq' : ''}`}>{bitsStr}</span>
          <span className="spec-val secondary">{bitrateStr}</span>
        </div>
      </div>

      <div
        className="hifi-progress-bar"
        onClick={(e) => {
          if (!duration) return
          const rect = e.currentTarget.getBoundingClientRect()
          const clickProgress = ((e.clientX - rect.left) / rect.width) * duration
          engine.seek(clickProgress)
          setProgress(clickProgress)
        }}
      >
        {Array.from({ length: 44 }).map((_, i) => (
          <span key={i} className={i < progressIndex ? 'active' : ''} />
        ))}
      </div>
    </section>
  )
}

/* ==========================================================================
   ROW 3 LEFT: PLAYLIST MEDIA VAULT PANEL (HQ FLAC Support)
   ========================================================================== */
function PlaylistPanel() {
  const playlist = usePlayerStore((s) => s.playlist)
  const activeFile = usePlayerStore((s) => s.file)
  const currentPlaylistName = usePlayerStore((s) => s.currentPlaylistName)
  const setFile = usePlayerStore((s) => s.setFile)
  const setIsPlaying = usePlayerStore((s) => s.setIsPlaying)
  const removeFromPlaylist = usePlayerStore((s) => s.removeFromPlaylist)
  const clearPlaylist = usePlayerStore((s) => s.clearPlaylist)
  const setPlaylist = usePlayerStore((s) => s.setPlaylist)
  const addToPlaylist = usePlayerStore((s) => s.addToPlaylist)
  const moveTrackInPlaylist = usePlayerStore((s) => s.moveTrackInPlaylist)

  const [savedPlaylists, setSavedPlaylists] = useState([])
  const [isExpanded, setIsExpanded] = useState(false)
  const [selectedIndex, setSelectedIndex] = useState(0)
  const [draggedIndex, setDraggedIndex] = useState(null)
  const [dragOverIndex, setDragOverIndex] = useState(null)
  const playlistBayRef = useRef(null)

  useEffect(() => {
    let cancelled = false
    window.api.playlists
      .list()
      .then((names) => {
        if (!cancelled) setSavedPlaylists(names)
      })
      .catch((err) => console.error('Failed to load saved playlists', err))
    return () => {
      cancelled = true
    }
  }, [])

  // Keyboard navigation & track reordering
  const handleKeyDown = (e) => {
    if (!playlist.length) return

    if (e.key === 'ArrowDown') {
      e.preventDefault()
      setSelectedIndex((prev) => Math.min(playlist.length - 1, prev + 1))
    } else if (e.key === 'ArrowUp') {
      e.preventDefault()
      setSelectedIndex((prev) => Math.max(0, prev - 1))
    } else if (e.key === 'PageDown' || (e.shiftKey && e.key === 'ArrowDown')) {
      e.preventDefault()
      if (selectedIndex < playlist.length - 1) {
        moveTrackInPlaylist(selectedIndex, selectedIndex + 1)
        setSelectedIndex((prev) => prev + 1)
      }
    } else if (e.key === 'PageUp' || (e.shiftKey && e.key === 'ArrowUp')) {
      e.preventDefault()
      if (selectedIndex > 0) {
        moveTrackInPlaylist(selectedIndex, selectedIndex - 1)
        setSelectedIndex((prev) => prev - 1)
      }
    } else if (e.key === 'Enter') {
      e.preventDefault()
      const target = playlist[selectedIndex]
      if (target) {
        setFile(target.file, target.metadata)
        setIsPlaying(true)
      }
    }
  }

  // Mouse Drag & Drop Reorder Handlers
  const handleDragStart = (e, index) => {
    setDraggedIndex(index)
    e.dataTransfer.effectAllowed = 'move'
    e.dataTransfer.setData('text/plain', String(index))
  }

  const handleDragOver = (e, index) => {
    e.preventDefault()
    e.dataTransfer.dropEffect = 'move'
    if (dragOverIndex !== index) {
      setDragOverIndex(index)
    }
  }

  const handleDrop = (e, dropIndex) => {
    e.preventDefault()
    e.stopPropagation()
    const fromIdx = draggedIndex ?? parseInt(e.dataTransfer.getData('text/plain'), 10)
    if (!isNaN(fromIdx) && fromIdx !== dropIndex) {
      moveTrackInPlaylist(fromIdx, dropIndex)
      setSelectedIndex(dropIndex)
    }
    setDraggedIndex(null)
    setDragOverIndex(null)
  }

  const handleDragEnd = () => {
    setDraggedIndex(null)
    setDragOverIndex(null)
  }

  const handleAddFiles = async () => {
    const paths = await window.api.metadata.openMultipleFiles()
    if (!paths?.length) return

    const tracks = await Promise.all(
      paths.map(async (path) => ({ path, meta: await window.api.metadata.parse(path) }))
    )
    tracks.forEach((track) => addToPlaylist(track))
  }

  const handleLoadPlaylist = async () => {
    const data = await window.api.playlists.loadFromFile()
    if (data) setPlaylist(data.playlist, data.name)
  }

  return (
    <section
      className={`hifi-panel hifi-row3-panel hifi-playlist-panel ${isExpanded ? 'is-expanded' : ''}`}
      tabIndex={0}
      onKeyDown={handleKeyDown}
    >
      <div className="hifi-panel-header">
        <div className="hifi-header-title">
          <Library size={15} className="hifi-header-icon" />
          <span>{currentPlaylistName || 'PROGRAM MEMORY'}</span>
        </div>

        <div className="hifi-playlist-actions-bar">
          <HifiButton onClick={handleAddFiles}>
            <Plus size={10} /> ADD
          </HifiButton>
          <HifiButton onClick={handleLoadPlaylist}>
            <FolderOpen size={10} /> LOAD
          </HifiButton>
          <HifiButton onClick={() => window.api.playlists.saveToFile(playlist)}>
            <Save size={10} /> SAVE
          </HifiButton>
          <HifiButton onClick={() => confirm('Purge playlist?') && clearPlaylist()}>
            <Trash2 size={10} /> CLEAR
          </HifiButton>
        </div>
      </div>

      {savedPlaylists.length > 0 && (
        <div className="hifi-saved-playlists">
          <Library size={11} />
          {savedPlaylists.map((name) => (
            <button
              key={name}
              className={name === currentPlaylistName ? 'active' : ''}
              onClick={async () => {
                const data = await window.api.playlists.load(name)
                setPlaylist(data.playlist, data.name)
              }}
            >
              {name}
            </button>
          ))}
        </div>
      )}

      <div className="hifi-playlist-bay" ref={playlistBayRef}>
        {playlist.length === 0 ? (
          <div className="hifi-empty-playlist">PROGRAM_BUFFER_EMPTY</div>
        ) : (
          playlist.map((track, idx) => {
            const isActive = track.file === activeFile
            const isSelected = selectedIndex === idx
            const isDragging = draggedIndex === idx
            const isDragOver = dragOverIndex === idx
            const trackFormat = getAudioFormatLabel(track.metadata, track.file)
            const mins = Math.floor((track.metadata?.duration || 0) / 60)
            const secs = Math.floor((track.metadata?.duration || 0) % 60)

            return (
              <div
                key={track.id || track.file}
                className={`hifi-track ${isActive ? 'active' : ''} ${isSelected ? 'selected' : ''} ${isDragging ? 'is-dragging' : ''} ${isDragOver ? 'drag-over' : ''}`}
                draggable
                onDragStart={(e) => handleDragStart(e, idx)}
                onDragOver={(e) => handleDragOver(e, idx)}
                onDrop={(e) => handleDrop(e, idx)}
                onDragEnd={handleDragEnd}
                onClick={() => {
                  setSelectedIndex(idx)
                  setFile(track.file, track.metadata)
                  setIsPlaying(true)
                }}
              >
                <GripVertical size={12} className="track-grab-handle" title="Drag to reorder" />
                <span className="track-index">{String(idx + 1).padStart(2, '0')}</span>
                <div className="track-main">
                  <strong>{track.metadata?.title || track.file.split('/').pop()}</strong>
                  <small>
                    {track.metadata?.artist || 'UNKNOWN'} • {trackFormat}
                  </small>
                </div>
                {isActive && <PlayCircle className="track-playing" size={14} />}
                <span className="track-time">
                  {mins}:{secs.toString().padStart(2, '0')}
                </span>
                <button
                  className="track-remove"
                  onClick={(e) => {
                    e.stopPropagation()
                    removeFromPlaylist(track.id || track.file)
                  }}
                  title="Remove track"
                >
                  <Trash2 size={12} />
                </button>
              </div>
            )
          })
        )}
      </div>

      <div className="hifi-playlist-footer">
        <span>COUNT: {playlist.length} TRACKS</span>
        <HifiButton
          className={isExpanded ? 'active' : ''}
          onClick={() => setIsExpanded(!isExpanded)}
        >
          {isExpanded ? <Minimize2 size={10} /> : <Maximize2 size={10} />}
          {isExpanded ? 'RESTORE' : 'EXPAND'}
        </HifiButton>
      </div>
    </section>
  )
}

/* ==========================================================================
   ROW 3 RIGHT: STEREO CASSETTE PLAYER PANEL
   ========================================================================== */
function CassettePanel() {
  const file = usePlayerStore((s) => s.file)
  const isPlaying = usePlayerStore((s) => s.isPlaying)
  const progress = usePlayerStore((s) => s.progress)
  const duration = usePlayerStore((s) => s.duration)
  const surroundMode = usePlayerStore((s) => s.surroundMode)
  const togglePlay = usePlayerStore((s) => s.togglePlay)
  const setIsPlaying = usePlayerStore((s) => s.setIsPlaying)
  const setProgress = usePlayerStore((s) => s.setProgress)

  const handleOpenFile = async () => {
    const path = await window.api.metadata.openFile()
    if (!path) return
    const meta = await window.api.metadata.parse(path)
    usePlayerStore.getState().setFile(path, meta)
    usePlayerStore.getState().setIsPlaying(true)
  }

  const handlePrevious = () => {
    const { playlist, file } = usePlayerStore.getState()
    const currentIndex = playlist.findIndex((t) => t.file === file)
    if (currentIndex > 0) {
      const prev = playlist[currentIndex - 1]
      usePlayerStore.getState().setFile(prev.file, prev.metadata)
      usePlayerStore.getState().setIsPlaying(true)
    }
  }

  const handleNext = () => {
    const { playlist, file } = usePlayerStore.getState()
    const currentIndex = playlist.findIndex((t) => t.file === file)
    if (currentIndex !== -1 && currentIndex < playlist.length - 1) {
      const next = playlist[currentIndex + 1]
      usePlayerStore.getState().setFile(next.file, next.metadata)
      usePlayerStore.getState().setIsPlaying(true)
    }
  }

  return (
    <section className="hifi-panel hifi-row3-panel hifi-cassette-panel">
      <div className="hifi-panel-header">
        <div className="hifi-header-title">
          <Disc size={15} className="hifi-header-icon" />
          <span>TAPE PLAYBACK ENGINE</span>
        </div>
      </div>

      <CassetteDeck
        file={file}
        isPlaying={isPlaying}
        progress={progress}
        duration={duration}
        mode={surroundMode}
      />

      <div className="hifi-transport-bay">
        <HifiButton onClick={handleOpenFile} title="Open File / Eject">
          EJECT
        </HifiButton>

        <div className="hifi-transport-keys">
          <HifiButton onClick={handlePrevious} title="Rewind / Previous">
            <SkipBack size={15} />
          </HifiButton>
          <HifiButton
            className={isPlaying ? 'primary active' : 'primary'}
            onClick={togglePlay}
            title="Play / Pause"
          >
            {isPlaying ? (
              <Pause size={18} fill="currentColor" />
            ) : (
              <Play size={18} fill="currentColor" />
            )}
          </HifiButton>
          <HifiButton
            className="stop"
            onClick={() => {
              setIsPlaying(false)
              engine
                .destroy()
                .catch((err) => console.warn('[CassettePanel] Native stop failed:', err))
              setProgress(0)
            }}
            title="Stop"
          >
            <Square size={15} fill="currentColor" />
          </HifiButton>
          <HifiButton onClick={handleNext} title="Fast Forward / Next">
            <SkipForward size={15} />
          </HifiButton>
        </div>
      </div>
    </section>
  )
}

/* ==========================================================================
   MAIN HIFI SYSTEM VIEW (MCINTOSH / SONY ES ICY SAPPHIRE BLUE THEME)
   ========================================================================== */
export function HifiSystem() {
  const [showVisualsControl, setShowVisualsControl] = useState(false)

  return (
    <main className="hifi-system-container">
      <div className="hifi-cabinet-casing">
        <div className="cabinet-cheek cheek-left" />
        <div className="cabinet-cheek cheek-right" />
        <div className="cabinet-screws screw-t-left" />
        <div className="cabinet-screws screw-t-right" />
        <div className="cabinet-screws screw-b-left" />
        <div className="cabinet-screws screw-b-right" />

        <div className="hifi-cabinet-inner">
          <div className="hifi-titlebar">
            <div className="hifi-brand">
              <strong>MAGNETOFON</strong>
              <span>HI-FI STEREO CONSOLE / MODEL ST-8000</span>
            </div>
            <div className="hifi-title-actions">
              <HifiButton onClick={() => window.api?.windowManager?.toggle('visuals')}>
                VISUALS
              </HifiButton>
              <HifiButton
                className={showVisualsControl ? 'active' : ''}
                onClick={() => setShowVisualsControl(!showVisualsControl)}
                title="Visuals Control & Audio Reactivity Engine"
              >
                <Sliders size={12} style={{ marginRight: '4px' }} />
                VIS CTRL
              </HifiButton>
              <HifiButton
                className="close"
                onClick={() => window.api?.windowManager?.closeCurrent()}
              >
                <X size={16} />
              </HifiButton>
            </div>
          </div>

          <div className="hifi-2col-layout">
            <div className="hifi-column col-left">
              <AmplifierPanel />
              <VuMeterPanel />
              <PlaylistPanel />
            </div>

            <div className="hifi-column col-right">
              <EqualizerPanel />
              <ProgramMonitorPanel />
              <CassettePanel />
            </div>
          </div>
        </div>

        {showVisualsControl && <VisualsControlPanel onClose={() => setShowVisualsControl(false)} />}

        <div className="cabinet-feet">
          <div className="foot foot-left" />
          <div className="foot foot-right" />
        </div>
      </div>
    </main>
  )
}
