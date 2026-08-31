/* eslint-disable react/prop-types */
import { Power, ChevronDown } from 'lucide-react'
import { usePlayerStore } from '../store/playerStore'

const bands = ['31', '62', '125', '250', '500', '1k', '2k', '4k', '8k', '16k']

const presets = {
  FLAT: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
  ROCK: [4, 3, 2, 0, -1, -1, 0, 2, 3, 4],
  POP: [-1, 0, 2, 3, 4, 3, 2, 0, -1, -1],
  CLASSICAL: [4, 3, 2, 2, 0, 0, 0, 2, 3, 3],
  JAZZ: [3, 2, 1, 2, -1, -1, 0, 1, 2, 3],
  'BASS BOOST': [6, 5, 4, 2, 0, 0, 0, 0, 0, 0]
}

function Slider({ value, onChange, isSecondary = false }) {
  const normalized = (value + 12) / 24
  const color = isSecondary ? 'var(--secondary)' : 'var(--primary-fixed)'

  const updateValue = (e) => {
    const rect = e.currentTarget.getBoundingClientRect()
    let y = e.clientY - rect.top
    y = Math.max(0, Math.min(rect.height, y))
    const scalar = 1 - y / rect.height
    const mapped = scalar * 24 - 12
    onChange(mapped)
  }

  return (
    <div
      style={{
        height: '140px',
        position: 'relative',
        width: '32px',
        display: 'flex',
        justifyContent: 'center',
        cursor: 'grab',
        touchAction: 'none'
      }}
      onPointerDown={(e) => {
        e.currentTarget.setPointerCapture(e.pointerId)
        updateValue(e)
      }}
      onPointerMove={(e) => {
        if (e.buttons) updateValue(e)
      }}
    >
      <div
        style={{
          position: 'absolute',
          bottom: 0,
          top: 0,
          width: '6px',
          background: 'var(--surface-container-lowest)',
          borderRadius: '3px'
        }}
      />
      <div
        style={{
          position: 'absolute',
          bottom: 0,
          height: `${normalized * 100}%`,
          width: '4px',
          background: color,
          opacity: 0.3,
          borderRadius: '2px'
        }}
      />

      {/* Hardware Switch Style Thumb */}
      <div
        style={{
          position: 'absolute',
          bottom: `${normalized * 100}%`,
          height: '14px',
          width: '24px',
          background: 'linear-gradient(135deg, var(--surface-bright), var(--surface-dim))',
          border: '1px solid #000',
          borderRadius: '2px',
          transform: 'translateY(50%)',
          boxShadow: '0 2px 4px rgba(0,0,0,0.5), inset 0 1px 1px rgba(255,255,255,0.1)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center'
        }}
      >
        <div
          style={{ width: '100%', height: '2px', background: color, boxShadow: `0 0 6px ${color}` }}
        />
      </div>
    </div>
  )
}

export function EqualizerWindow() {
  const eqEnabled = usePlayerStore((s) => s.eqEnabled)
  const setEqEnabled = usePlayerStore((s) => s.setEqEnabled)
  const preamp = usePlayerStore((s) => s.preamp)
  const setPreamp = usePlayerStore((s) => s.setPreamp)
  const eqBands = usePlayerStore((s) => s.eqBands)
  const setEqBand = usePlayerStore((s) => s.setEqBand)

  return (
    <div
      className="metallic-surface"
      style={{
        width: '100vw',
        height: '100vh',
        boxSizing: 'border-box',
        borderRadius: '8px',
        padding: '16px',
        overflow: 'hidden',
        position: 'relative'
      }}
    >
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'flex-start',
          marginBottom: '20px',
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
            EQ
          </h1>
        </div>

        <div
          style={{ display: 'flex', alignItems: 'center', gap: '8px', WebkitAppRegion: 'no-drag' }}
        >
          <div style={{ position: 'relative' }}>
            <select
              onChange={(e) => {
                const p = presets[e.target.value]
                if (p) p.forEach((val, idx) => setEqBand(idx, val))
              }}
              style={{
                background: 'var(--surface-container-high)',
                color: 'var(--primary-fixed)',
                border: 'none',
                padding: '4px 24px 4px 8px',
                fontSize: '10px',
                borderRadius: '2px',
                fontFamily: 'var(--font-display)',
                appearance: 'none',
                cursor: 'pointer',
                outline: 'none'
              }}
            >
              <option value="CUSTOM">PRESET: CUSTOM</option>
              {Object.keys(presets).map((name) => (
                <option key={name} value={name}>
                  {name}
                </option>
              ))}
            </select>
            <ChevronDown
              size={10}
              style={{
                position: 'absolute',
                right: '6px',
                top: '50%',
                transform: 'translateY(-50%)',
                pointerEvents: 'none',
                opacity: 0.5
              }}
            />
          </div>

          <button
            onClick={() => setEqEnabled(!eqEnabled)}
            style={{
              background: eqEnabled ? 'var(--primary-container)' : 'var(--surface-container-high)',
              border: 'none',
              color: eqEnabled ? 'var(--on-primary-container)' : 'var(--on-surface-variant)',
              borderRadius: '2px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '4px 10px',
              fontFamily: 'var(--font-display)',
              fontSize: '10px',
              boxShadow: eqEnabled ? '0 0 16px rgba(0,255,157,0.3)' : 'none',
              cursor: 'pointer',
              transition: 'all 0.2s'
            }}
          >
            <Power size={10} style={{ marginRight: '6px' }} /> {eqEnabled ? 'ON' : 'OFF'}
          </button>
        </div>
      </div>

      {/* Compact Sliders Layout */}
      <div
        className="vintage-bay"
        style={{
          display: 'flex',
          gap: '8px',
          background: 'rgba(0,0,0,0.2)',
          padding: '12px 8px',
          borderRadius: '6px',
          opacity: eqEnabled ? 1 : 0.4,
          transition: 'opacity 0.3s',
          WebkitAppRegion: 'no-drag',
          boxShadow: 'inset 0 2px 8px rgba(0,0,0,0.4)',
          justifyContent: 'center'
        }}
      >
        {/* Preamp - Isolated Sidebar */}
        <div
          style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            borderRight: '1px solid rgba(255,255,255,0.05)',
            paddingRight: '8px'
          }}
        >
          <Slider value={preamp} onChange={(v) => eqEnabled && setPreamp(v)} isSecondary={true} />
          <div
            style={{
              fontSize: '8px',
              marginTop: '12px',
              color: 'var(--secondary)',
              letterSpacing: '1px',
              fontWeight: 600
            }}
          >
            GAIN
          </div>
        </div>

        {/* 10-Band EQ - Compact Grouping */}
        <div style={{ display: 'flex', gap: '4px' }}>
          {/* Lows */}
          <div style={{ display: 'flex', gap: '2px' }}>
            {bands.slice(0, 3).map((band, i) => (
              <div
                key={i}
                style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  width: '24px'
                }}
              >
                <Slider value={eqBands[i]} onChange={(v) => eqEnabled && setEqBand(i, v)} />
                <div
                  style={{
                    fontSize: '8px',
                    marginTop: '12px',
                    color: 'var(--on-surface-variant)',
                    fontFamily: 'var(--font-display)',
                    opacity: 0.6
                  }}
                >
                  {band}
                </div>
              </div>
            ))}
          </div>

          {/* Mids */}
          <div
            style={{
              display: 'flex',
              gap: '2px',
              borderLeft: '1px solid rgba(255,255,255,0.03)',
              borderRight: '1px solid rgba(255,255,255,0.03)',
              padding: '0 4px'
            }}
          >
            {bands.slice(3, 7).map((band, i) => {
              const idx = i + 3
              return (
                <div
                  key={idx}
                  style={{
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    width: '24px'
                  }}
                >
                  <Slider value={eqBands[idx]} onChange={(v) => eqEnabled && setEqBand(idx, v)} />
                  <div
                    style={{
                      fontSize: '8px',
                      marginTop: '12px',
                      color: 'var(--on-surface-variant)',
                      fontFamily: 'var(--font-display)',
                      opacity: 0.6
                    }}
                  >
                    {band}
                  </div>
                </div>
              )
            })}
          </div>

          {/* Highs */}
          <div style={{ display: 'flex', gap: '2px' }}>
            {bands.slice(7).map((band, i) => {
              const idx = i + 7
              return (
                <div
                  key={idx}
                  style={{
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    width: '24px'
                  }}
                >
                  <Slider value={eqBands[idx]} onChange={(v) => eqEnabled && setEqBand(idx, v)} />
                  <div
                    style={{
                      fontSize: '8px',
                      marginTop: '12px',
                      color: 'var(--on-surface-variant)',
                      fontFamily: 'var(--font-display)',
                      opacity: 0.6
                    }}
                  >
                    {band}
                  </div>
                </div>
              )
            })}
          </div>
        </div>
      </div>
    </div>
  )
}
