/* eslint-disable react/prop-types */
import { useRef, useState } from 'react'

/**
 * Tactile Knurled Metallic Knob Component
 * Supports vertical pointer dragging, mouse wheel scrolling, double-click reset,
 * radial indicator notch, and outer tick marks with labels.
 */
export function Knob({
  value = 0,
  min = 0,
  max = 1,
  step = 0.01,
  onChange,
  onReset,
  size = 60,
  label = 'VOLUME',
  displayValue,
  ticks = true
}) {
  const [isDragging, setIsDragging] = useState(false)
  const startYRef = useRef(0)
  const startValRef = useRef(0)

  // Map value to angle (-135 deg [7 o'clock] to +135 deg [5 o'clock])
  const normValue = Math.max(0, Math.min(1, (value - min) / (max - min)))
  const angle = -135 + normValue * 270

  const handlePointerDown = (e) => {
    e.currentTarget.setPointerCapture(e.pointerId)
    setIsDragging(true)
    startYRef.current = e.clientY
    startValRef.current = value
  }

  const handlePointerMove = (e) => {
    if (!isDragging && !(e.buttons & 1)) return
    const dy = startYRef.current - e.clientY // drag up increases
    const range = max - min
    const sensitivity = range / 160 // 160px drag for full 0 to 1 sweep
    let newVal = startValRef.current + dy * sensitivity
    if (step) {
      newVal = Math.round(newVal / step) * step
    }
    newVal = Math.max(min, Math.min(max, newVal))
    if (onChange) onChange(newVal)
  }

  const handlePointerUp = (e) => {
    setIsDragging(false)
    try {
      e.currentTarget.releasePointerCapture(e.pointerId)
    } catch {
      // ignore
    }
  }

  const handleWheel = (e) => {
    e.preventDefault()
    const delta = e.deltaY < 0 ? 0.04 : -0.04
    let newVal = value + delta
    if (step) {
      newVal = Math.round(newVal / step) * step
    }
    newVal = Math.max(min, Math.min(max, newVal))
    if (onChange) onChange(newVal)
  }

  const handleDoubleClick = () => {
    if (onReset) onReset()
    else if (onChange) onChange((max + min) / 2)
  }

  const radiusOffset = size / 2 + 8

  return (
    <div className="hifi-knob-wrapper" onWheel={handleWheel}>
      <div
        className={`hifi-knob-dial ${isDragging ? 'is-dragging' : ''}`}
        style={{ width: `${size}px`, height: `${size}px` }}
        onPointerDown={handlePointerDown}
        onPointerMove={handlePointerMove}
        onPointerUp={handlePointerUp}
        onDoubleClick={handleDoubleClick}
        title={`${label}: ${displayValue || value}`}
      >
        {/* Outer Knurled Rim */}
        <div className="knob-knurling" />

        {/* Brushed Metal Cap Face */}
        <div className="knob-cap">
          <div className="knob-notch" style={{ transform: `rotate(${angle}deg)` }} />
        </div>

        {/* Specular Highlight Overlay */}
        <div className="knob-shine" />
      </div>

      {ticks && (
        <div
          className="knob-ticks-ring"
          style={{ width: `${size + 16}px`, height: `${size + 16}px` }}
        >
          {Array.from({ length: 11 }).map((_, i) => {
            const tickAngle = -135 + (i / 10) * 270
            const active = i / 10 <= normValue
            return (
              <span
                key={i}
                className={`knob-tick ${active ? 'active' : ''}`}
                style={{
                  transform: `rotate(${tickAngle}deg)`,
                  transformOrigin: `50% ${radiusOffset}px`
                }}
              />
            )
          })}
        </div>
      )}

      {label && <span className="knob-label">{label}</span>}
      {displayValue !== undefined && <span className="knob-readout">{displayValue}</span>}
    </div>
  )
}
