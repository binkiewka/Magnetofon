/* eslint-disable react/prop-types */
import { useEffect, useMemo, useState } from 'react'
import {
  Power,
  RefreshCw,
  X,
  Folder,
  Sparkles,
  Activity,
  Clock,
  Shuffle,
  Heart,
  CheckCircle2,
  Search,
  ArrowRight,
  Clipboard,
  Keyboard
} from 'lucide-react'

const MAX_VISIBLE_PRESETS = 250

export function VisualsControlPanel({ onClose }) {
  const [settings, setSettings] = useState({
    presetSource: 'main',
    presetDuration: 30,
    transitionDuration: 3,
    shuffleEnabled: true,
    fps: 60,
    beatSensitivity: 1.0,
    hardCutsEnabled: true,
    hardCutDuration: 20,
    hardCutSensitivity: 1.0,
    fullscreen: false,
    audioDevice: 'auto',
    presetCategory: 'all',
    curatedPresets: {}
  })

  const [isRunning, setIsRunning] = useState(false)
  const [audioDevices, setAudioDevices] = useState([])
  const [presetsList, setPresetsList] = useState([])
  const [searchQuery, setSearchQuery] = useState('')
  const [statusMessage, setStatusMessage] = useState('')
  const [curationStats, setCurationStats] = useState(null)

  const [packStatus, setPackStatus] = useState({
    installed: false,
    fullCount: 0,
    curatedCount: 0,
    activePack: ''
  })
  const [packProgress, setPackProgress] = useState(null)
  const [isInstallingPack, setIsInstallingPack] = useState(false)

  useEffect(() => {
    let mounted = true
    const init = async () => {
      if (!window.api?.visuals) return
      try {
        const [savedSettings, status, devices, list, pStatus] = await Promise.all([
          window.api.visuals.getSettings(),
          window.api.visuals.getStatus(),
          window.api.visuals.getAudioDevices
            ? window.api.visuals.getAudioDevices()
            : Promise.resolve([]),
          window.api.visuals.getPresets
            ? window.api.visuals.getPresets('all')
            : Promise.resolve([]),
          window.api.visuals.getPackStatus
            ? window.api.visuals.getPackStatus()
            : Promise.resolve(null)
        ])
        if (!mounted) return
        if (savedSettings) setSettings((prev) => ({ ...prev, ...savedSettings }))
        if (status) setIsRunning(Boolean(status.isRunning))
        if (Array.isArray(devices)) setAudioDevices(devices)
        if (Array.isArray(list)) setPresetsList(list)
        if (pStatus) setPackStatus(pStatus)
      } catch (err) {
        console.error('[VisualsControlPanel] Failed to load state:', err)
      }
    }
    init()

    let unsubscribeProgress
    if (window.api?.visuals?.onPackProgress) {
      unsubscribeProgress = window.api.visuals.onPackProgress((data) => {
        if (!mounted) return
        setPackProgress(data)
      })
    }

    return () => {
      mounted = false
      if (unsubscribeProgress) unsubscribeProgress()
    }
  }, [])

  const handleDownloadPack = async () => {
    if (!window.api?.visuals?.downloadPack) return
    setIsInstallingPack(true)
    setStatusMessage('Starting MilkDrop 9k+ Presets installation...')
    try {
      const status = await window.api.visuals.downloadPack()
      if (status) setPackStatus(status)
      await refreshPresetsList()
      setStatusMessage('9,000+ Curated Presets installed & active!')
    } catch (err) {
      console.error('[VisualsControlPanel] Pack download failed:', err)
      setStatusMessage(`Preset pack installation failed: ${err.message}`)
    } finally {
      setIsInstallingPack(false)
      setTimeout(() => setStatusMessage(''), 5000)
    }
  }

  const refreshPresetsList = async () => {
    if (!window.api?.visuals?.getPresets) return
    try {
      const list = await window.api.visuals.getPresets('all')
      if (Array.isArray(list)) {
        setPresetsList(list)
      }
    } catch (err) {
      console.error('[VisualsControlPanel] Failed to refresh preset list:', err)
    }
  }

  const updateSetting = async (key, value) => {
    const updated = { ...settings, [key]: value }
    setSettings(updated)

    try {
      if (window.api?.visuals?.saveSettings) {
        await window.api.visuals.saveSettings(updated)
      }
      setStatusMessage('Settings saved')
      setTimeout(() => setStatusMessage(''), 2000)

      if (window.api?.visuals?.getStatus) {
        const st = await window.api.visuals.getStatus()
        setIsRunning(Boolean(st?.isRunning))
      }
    } catch (err) {
      console.error('[VisualsControlPanel] Save failed:', err)
      setStatusMessage('Failed to save settings')
    }
  }

  const toggleLikePreset = async (filename) => {
    const currentCurated = { ...(settings.curatedPresets || {}) }
    const isLiked = currentCurated[filename] === 'liked'

    if (isLiked) {
      delete currentCurated[filename]
      setStatusMessage(`Unliked "${filename}"`)
    } else {
      currentCurated[filename] = 'liked'
      setStatusMessage(`Liked "${filename}" - Click "Move Liked Presets" to send to Curated!`)
    }

    await updateSetting('curatedPresets', currentCurated)
    setTimeout(() => setStatusMessage(''), 3000)
  }

  const handleLikeFromClipboard = async () => {
    if (!window.api?.visuals?.getClipboardText) return
    try {
      const text = await window.api.visuals.getClipboardText()
      if (!text || !text.trim()) {
        setStatusMessage('Clipboard is empty! Press Ctrl+C in projectM window first.')
        setTimeout(() => setStatusMessage(''), 3500)
        return
      }
      const clean = text.trim().replace(/^.*[/\\]/, '')
      const filename = clean.endsWith('.milk') ? clean : `${clean}.milk`

      const currentCurated = { ...(settings.curatedPresets || {}) }
      currentCurated[filename] = 'liked'
      await updateSetting('curatedPresets', currentCurated)

      setStatusMessage(`Liked "${filename.replace(/\.milk$/i, '')}" from clipboard!`)
      setTimeout(() => setStatusMessage(''), 4000)
    } catch (err) {
      console.error('[VisualsControlPanel] Like from clipboard failed:', err)
      setStatusMessage('Could not read preset from clipboard')
    }
  }

  const handleCollectSingle = async (filename) => {
    if (!window.api?.visuals?.collectPreset) return
    try {
      setStatusMessage(`Moving "${filename}" to Curated folder...`)
      const res = await window.api.visuals.collectPreset(filename)
      if (res?.ok) {
        setStatusMessage(`Moved "${filename}" & textures to Curated folder!`)
        await refreshPresetsList()
      }
      setTimeout(() => setStatusMessage(''), 3000)
    } catch (err) {
      console.error('[VisualsControlPanel] Single collect failed:', err)
      setStatusMessage(`Failed to move "${filename}"`)
    }
  }

  const handleToggleVisualizer = async () => {
    if (!window.api?.visuals?.toggle) return
    try {
      const status = await window.api.visuals.toggle(settings)
      setIsRunning(Boolean(status?.isRunning))
    } catch (err) {
      console.error('[VisualsControlPanel] Toggle failed:', err)
    }
  }

  const handleRestartVisualizer = async () => {
    if (!window.api?.visuals?.restart) return
    try {
      await window.api.visuals.restart()
      setIsRunning(true)
      setStatusMessage('Visualizer restarted with active settings')
      setTimeout(() => setStatusMessage(''), 2500)
    } catch (err) {
      console.error('[VisualsControlPanel] Restart failed:', err)
    }
  }

  const handleCollectFavorites = async () => {
    if (!window.api?.visuals?.collectFavorites) return
    try {
      setStatusMessage('Collecting liked presets into curated folder...')
      const res = await window.api.visuals.collectFavorites()
      if (res?.ok) {
        setCurationStats(res)
        setStatusMessage(
          `Moved ${res.movedPresetsCount || 0} presets & ${res.movedTexturesCount || 0} textures to Curated!`
        )
        await refreshPresetsList()
      } else {
        setStatusMessage('No liked presets found to move.')
      }
      setTimeout(() => setStatusMessage(''), 4500)
    } catch (err) {
      console.error('[VisualsControlPanel] Collect favorites failed:', err)
      setStatusMessage('Failed to collect favorites.')
    }
  }

  const filteredPresets = useMemo(() => {
    const normalizedQuery = searchQuery.trim().toLowerCase()
    if (!normalizedQuery) return presetsList
    return presetsList.filter((item) => item.filename.toLowerCase().includes(normalizedQuery))
  }, [presetsList, searchQuery])
  const visiblePresets = useMemo(
    () => filteredPresets.slice(0, MAX_VISIBLE_PRESETS),
    [filteredPresets]
  )

  const likedCount = Object.keys(settings.curatedPresets || {}).filter(
    (key) => settings.curatedPresets[key] === 'liked'
  ).length

  return (
    <div className="hifi-visuals-modal-overlay" onClick={onClose}>
      <div className="hifi-visuals-panel" onClick={(e) => e.stopPropagation()}>
        {/* Title bar */}
        <div className="hifi-titlebar">
          <div className="hifi-brand">
            <strong>MAGNETOFON</strong>
            <span>VISUALS CONTROL & AUDIO REACTIVITY ENGINE</span>
          </div>
          <button className="hifi-button close" onClick={onClose}>
            <X size={16} />
          </button>
        </div>

        {/* Top Status & Main Power Bar */}
        <div className="visuals-power-bar">
          <div className="visuals-status-indicator">
            <div className={`status-led ${isRunning ? 'led-active' : 'led-off'}`} />
            <div className="status-text">
              <span className="status-label">NATIVE PROJECTM STATUS:</span>
              <strong className={isRunning ? 'active-text' : 'inactive-text'}>
                {isRunning ? 'RUNNING & SYNCED' : 'STOPPED'}
              </strong>
            </div>
          </div>

          <div className="visuals-power-actions">
            <button
              className={`hifi-button power-btn ${isRunning ? 'is-active' : ''}`}
              onClick={handleToggleVisualizer}
            >
              <Power size={14} style={{ marginRight: '6px' }} />
              {isRunning ? 'STOP VISUALS' : 'LAUNCH VISUALS'}
            </button>

            {isRunning && (
              <button className="hifi-button" onClick={handleRestartVisualizer}>
                <RefreshCw size={14} style={{ marginRight: '6px' }} />
                RESTART / APPLY
              </button>
            )}
          </div>
        </div>

        {statusMessage && (
          <div className="visuals-toast-message">
            <CheckCircle2 size={14} /> {statusMessage}
          </div>
        )}

        <div className="visuals-settings-grid">
          {/* SECTION 1: Preset Source Folder */}
          <div className="visuals-card-panel">
            <div className="card-header">
              <Folder size={16} className="card-icon" />
              <h3>VISUALS PRESET FOLDER SOURCE</h3>
            </div>
            <p className="card-description">
              Select which folder projectM scans by default. Curated contains your verified
              favorites.
            </p>

            <div className="folder-source-selector">
              <button
                className={`folder-opt-btn ${settings.presetSource === 'main' ? 'active' : ''}`}
                onClick={() => updateSetting('presetSource', 'main')}
              >
                <Folder size={18} />
                <div className="folder-opt-text">
                  <strong>MAIN FOLDER (Default)</strong>
                  <span>visuals/presets (Full Library)</span>
                </div>
              </button>

              <button
                className={`folder-opt-btn ${settings.presetSource === 'curated' ? 'active' : ''}`}
                onClick={() => updateSetting('presetSource', 'curated')}
              >
                <Sparkles size={18} />
                <div className="folder-opt-text">
                  <strong>CURATED FOLDER</strong>
                  <span>visuals/curated/presets (Favorites)</span>
                </div>
              </button>
            </div>
          </div>

          {/* SECTION 1.5: Preset Pack Downloader & Manager */}
          <div className="visuals-card-panel">
            <div className="card-header">
              <Sparkles size={16} className="card-icon" />
              <h3>MILKDROP 9k+ PRESET PACK MANAGER</h3>
            </div>
            <p className="card-description">
              Install the high-quality Isosceles &quot;Cream of the Crop&quot; pre-curated pack
              (9,000+ MilkDrop presets &amp; textures).
            </p>

            <div className="preset-pack-status-box">
              <div className="pack-status-info">
                <strong className={packStatus.installed ? 'installed-tag' : 'bundled-tag'}>
                  {packStatus.installed
                    ? 'STATUS: 9k+ PRESET PACK INSTALLED'
                    : 'STATUS: DEFAULT BUNDLED MODE'}
                </strong>
                <span>
                  Active Pack: {packStatus.activePack || 'Default Curated Pack (~150 Presets)'}
                </span>
              </div>

              {isInstallingPack || (packProgress && packProgress.phase !== 'completed') ? (
                <div className="pack-progress-wrap">
                  <div className="pack-progress-bar">
                    <div
                      className="pack-progress-fill"
                      style={{ width: `${packProgress?.percent || 0}%` }}
                    />
                  </div>
                  <span className="pack-progress-text">
                    {packProgress?.detail || 'Processing preset pack...'} (
                    {packProgress?.percent || 0}%)
                  </span>
                </div>
              ) : (
                <button
                  className="hifi-button install-pack-btn"
                  onClick={handleDownloadPack}
                  disabled={isInstallingPack}
                >
                  <Sparkles size={14} style={{ marginRight: '6px' }} />
                  {packStatus.installed
                    ? 'REINSTALL / RE-EXTRACT 9k+ PRESET PACK'
                    : 'DOWNLOAD & UNPACK 9k+ PRESETS (~137 MB)'}
                </button>
              )}
            </div>
          </div>

          {/* SECTION 1.6: Visual Theme & Category Filter */}
          <div className="visuals-card-panel">
            <div className="card-header">
              <Sparkles size={16} className="card-icon" />
              <h3>VISUAL THEME &amp; CATEGORY FILTER</h3>
            </div>
            <p className="card-description">
              Filter visualizer preset rotation by visual theme (from the Isosceles Cream of the
              Crop pack).
            </p>

            <div className="category-theme-grid">
              {[
                { id: 'all', label: 'ALL THEMES' },
                { id: 'dancer', label: '💃 DANCER' },
                { id: 'drawing', label: '✏️ DRAWING' },
                { id: 'fractal', label: '🌀 FRACTAL' },
                { id: 'geometric', label: '📐 GEOMETRIC' },
                { id: 'hypnotic', label: '👁️ HYPNOTIC' },
                { id: 'particles', label: '✨ PARTICLES' },
                { id: 'reaction', label: '🧪 REACTION' },
                { id: 'sparkle', label: '🌟 SPARKLE' },
                { id: 'supernova', label: '💥 SUPERNOVA' },
                { id: 'waveform', label: '🌊 WAVEFORM' }
              ].map((cat) => (
                <button
                  key={cat.id}
                  className={`category-theme-btn ${
                    (settings.presetCategory || 'all') === cat.id ? 'active' : ''
                  }`}
                  onClick={() => updateSetting('presetCategory', cat.id)}
                >
                  {cat.label}
                </button>
              ))}
            </div>
          </div>

          {/* SECTION 2: Live Keyboard & Mouse Shortcuts Guide */}
          <div className="visuals-card-panel">
            <div className="card-header">
              <Keyboard size={16} className="card-icon" />
              <h3>LIVE PLAYBACK KEYBOARD & MOUSE CONTROLS</h3>
            </div>
            <p className="card-description">
              Use these shortcuts directly inside the playing projectM Visualizer window:
            </p>

            <div className="shortcuts-guide-grid">
              <div className="shortcut-item">
                <kbd>Ctrl + C</kbd>
                <span>Copy active preset path to clipboard</span>
              </div>
              <div className="shortcut-item">
                <kbd>Spacebar</kbd>
                <span>Freeze / Lock currently playing visual</span>
              </div>
              <div className="shortcut-item">
                <kbd>N</kbd> / <kbd>P</kbd>
                <span>Skip to Next / Previous preset</span>
              </div>
              <div className="shortcut-item">
                <kbd>Esc</kbd> / <kbd>Right Click</kbd>
                <span>Open native projectM overlay menu</span>
              </div>
            </div>
          </div>

          {/* SECTION 3: Display Timing & Rotation */}
          <div className="visuals-card-panel">
            <div className="card-header">
              <Clock size={16} className="card-icon" />
              <h3>DISPLAY TIMING & ROTATION</h3>
            </div>

            <div className="control-row">
              <div className="control-info">
                <label>Preset Duration (Switch Interval)</label>
                <span className="control-subtext">
                  How long each visual is displayed before next one
                </span>
              </div>
              <div className="control-input-group">
                <input
                  type="range"
                  min="5"
                  max="120"
                  step="5"
                  value={settings.presetDuration}
                  onChange={(e) => updateSetting('presetDuration', Number(e.target.value))}
                />
                <span className="unit-badge">{settings.presetDuration}s</span>
              </div>
            </div>

            <div className="control-row">
              <div className="control-info">
                <label>Transition Duration</label>
                <span className="control-subtext">Crossfade blend time between visuals</span>
              </div>
              <div className="control-input-group">
                <input
                  type="range"
                  min="0"
                  max="10"
                  step="1"
                  value={settings.transitionDuration}
                  onChange={(e) => updateSetting('transitionDuration', Number(e.target.value))}
                />
                <span className="unit-badge">{settings.transitionDuration}s</span>
              </div>
            </div>

            <div className="control-row-inline">
              <div className="toggle-field">
                <label>Shuffle Mode</label>
                <button
                  className={`hifi-switch ${settings.shuffleEnabled ? 'active' : ''}`}
                  onClick={() => updateSetting('shuffleEnabled', !settings.shuffleEnabled)}
                >
                  <Shuffle size={14} style={{ marginRight: '4px' }} />
                  {settings.shuffleEnabled ? 'SHUFFLE ON' : 'SEQUENTIAL'}
                </button>
              </div>

              <div className="toggle-field">
                <label>Target FPS</label>
                <div className="fps-segmented-control">
                  {[30, 60, 120].map((rate) => (
                    <button
                      key={rate}
                      className={`fps-btn ${settings.fps === rate ? 'active' : ''}`}
                      onClick={() => updateSetting('fps', rate)}
                    >
                      {rate} FPS
                    </button>
                  ))}
                </div>
              </div>
            </div>
          </div>

          {/* SECTION 4: Music Reactivity Controls */}
          <div className="visuals-card-panel">
            <div className="card-header">
              <Activity size={16} className="card-icon" />
              <h3>MUSIC REACTIVITY CONTROLS</h3>
            </div>
            <p className="card-description">
              Tune how dynamically visuals distort, zoom, move, and switch in rhythm with playing
              music.
            </p>

            <div className="control-row">
              <div className="control-info">
                <label>Beat Sensitivity</label>
                <span className="control-subtext">
                  Visual pulse & expansion strength from audio beat
                </span>
              </div>
              <div className="control-input-group">
                <input
                  type="range"
                  min="0.1"
                  max="2.0"
                  step="0.1"
                  value={settings.beatSensitivity}
                  onChange={(e) => updateSetting('beatSensitivity', Number(e.target.value))}
                />
                <span className="unit-badge">{Number(settings.beatSensitivity).toFixed(1)}x</span>
              </div>
            </div>

            <div className="control-row">
              <div className="control-info">
                <label>Hard Cut Scene Reactivity</label>
                <span className="control-subtext">
                  Trigger instant visual cuts on strong bass/drums drops
                </span>
              </div>
              <button
                className={`hifi-switch ${settings.hardCutsEnabled ? 'active' : ''}`}
                onClick={() => updateSetting('hardCutsEnabled', !settings.hardCutsEnabled)}
              >
                {settings.hardCutsEnabled ? 'ENABLED' : 'DISABLED'}
              </button>
            </div>

            {settings.hardCutsEnabled && (
              <>
                <div className="control-row indent-row">
                  <div className="control-info">
                    <label>Hard Cut Sensitivity</label>
                    <span className="control-subtext">Peak threshold required to trigger cut</span>
                  </div>
                  <div className="control-input-group">
                    <input
                      type="range"
                      min="0.1"
                      max="5.0"
                      step="0.1"
                      value={settings.hardCutSensitivity}
                      onChange={(e) => updateSetting('hardCutSensitivity', Number(e.target.value))}
                    />
                    <span className="unit-badge">
                      {Number(settings.hardCutSensitivity).toFixed(1)}x
                    </span>
                  </div>
                </div>

                <div className="control-row indent-row">
                  <div className="control-info">
                    <label>Minimum Time Between Hard Cuts</label>
                    <span className="control-subtext">Cooldown interval before next hard cut</span>
                  </div>
                  <div className="control-input-group">
                    <input
                      type="range"
                      min="2"
                      max="60"
                      step="2"
                      value={settings.hardCutDuration}
                      onChange={(e) => updateSetting('hardCutDuration', Number(e.target.value))}
                    />
                    <span className="unit-badge">{settings.hardCutDuration}s</span>
                  </div>
                </div>
              </>
            )}

            {/* Audio Capture Device Selection */}
            {audioDevices.length > 0 && (
              <div className="control-row">
                <div className="control-info">
                  <label>Audio Capture Device</label>
                  <span className="control-subtext">
                    PulseAudio monitor source feed to projectM
                  </span>
                </div>
                <select
                  className="hifi-select"
                  value={settings.audioDevice || 'auto'}
                  onChange={(e) => updateSetting('audioDevice', e.target.value)}
                >
                  <option value="auto">Auto Detect System Output Monitor</option>
                  {audioDevices.map((dev, idx) => (
                    <option key={dev.name || idx} value={dev.name}>
                      {dev.description || dev.name}
                    </option>
                  ))}
                </select>
              </div>
            )}
          </div>

          {/* SECTION 5: Preset Rating & Curation Manager */}
          <div className="visuals-card-panel">
            <div className="card-header">
              <Heart size={16} className="card-icon" />
              <h3>PRESET RATING & CURATION MANAGER</h3>
            </div>
            <p className="card-description">
              Press <strong>Ctrl+C</strong> in the projectM window while watching to copy the
              playing visual name, then click <strong>&quot;LIKE FROM CLIPBOARD&quot;</strong>{' '}
              below! Or click the Heart on any preset in the list.
            </p>

            <div className="curation-action-bar">
              <button className="hifi-button clipboard-like-btn" onClick={handleLikeFromClipboard}>
                <Clipboard size={14} style={{ marginRight: '6px' }} />
                LIKE PLAYING PRESET FROM CLIPBOARD (Ctrl+C)
              </button>

              <button className="hifi-button curated-action-btn" onClick={handleCollectFavorites}>
                <Heart
                  size={15}
                  style={{ marginRight: '6px', color: '#ec4899', fill: '#ec4899' }}
                />
                MOVE {likedCount > 0 ? `${likedCount} ` : ''}LIKED PRESET
                {likedCount === 1 ? '' : 'S'} TO CURATED FOLDER
              </button>
            </div>

            {curationStats && (
              <div className="curation-stats-summary">
                <CheckCircle2 size={14} /> Moved {curationStats.movedPresetsCount} presets and{' '}
                {curationStats.movedTexturesCount} textures to <code>visuals/curated</code>.
              </div>
            )}

            {/* Interactive Preset Browser List */}
            <div className="preset-browser-container">
              <div className="preset-search-bar">
                <Search size={14} className="search-icon" />
                <input
                  type="text"
                  placeholder="Search presets library..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                />
                <span className="preset-count-badge">{filteredPresets.length} PRESETS</span>
              </div>

              <div className="preset-list-scroll">
                {filteredPresets.length === 0 ? (
                  <div className="empty-presets-text">No presets found matching query</div>
                ) : (
                  visiblePresets.map((item) => {
                    const isLiked = settings.curatedPresets?.[item.filename] === 'liked'
                    const isCuratedFolder = item.source === 'curated'

                    return (
                      <div
                        key={`${item.source}-${item.filename}`}
                        className={`preset-item-row ${isLiked ? 'is-liked' : ''} ${isCuratedFolder ? 'is-curated' : ''}`}
                      >
                        <button
                          className={`like-heart-btn ${isLiked ? 'liked' : ''}`}
                          onClick={() => toggleLikePreset(item.filename)}
                          title={isLiked ? 'Unlike preset' : 'Click heart to Like this preset'}
                        >
                          <Heart
                            size={14}
                            fill={isLiked ? '#ec4899' : 'none'}
                            color={isLiked ? '#ec4899' : '#94a3b8'}
                          />
                        </button>

                        <div className="preset-item-name">
                          <strong>{item.filename.replace(/\.milk$/i, '')}</strong>
                          <span className="preset-source-tag">
                            {isCuratedFolder ? 'CURATED FOLDER' : 'MAIN FOLDER'}
                          </span>
                        </div>

                        {!isCuratedFolder && (
                          <button
                            className="hifi-button mini-move-btn"
                            onClick={() => handleCollectSingle(item.filename)}
                            title="Move this specific preset directly to Curated folder"
                          >
                            <ArrowRight size={11} /> MOVE TO CURATED
                          </button>
                        )}
                      </div>
                    )
                  })
                )}
                {filteredPresets.length > visiblePresets.length && (
                  <div className="preset-limit-message">
                    Showing the first {MAX_VISIBLE_PRESETS} of {filteredPresets.length} presets.
                    Search to narrow the list.
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>

        {/* Footer */}
        <div className="visuals-panel-footer">
          <button className="hifi-button primary-btn" onClick={onClose}>
            DONE / CLOSE
          </button>
        </div>
      </div>
    </div>
  )
}
