/* eslint-disable react/prop-types */
import { useEffect, useRef } from 'react'
import { engine } from '../audio/audioEngine'
import { usePlayerStore } from '../store/playerStore'

/**
 * High-DPI Iconic McIntosh Blue Glass Dual Power Output VU Meter
 * Renders legendary McIntosh cyan-blue backlit glass dials,
 * dual Watts / Decibel arcs, crimson needles, and glass face reflections.
 */
export function VuMeter({ isPlaying }) {
  const containerRef = useRef(null)
  const canvasRef = useRef(null)
  const animFrameRef = useRef(null)

  const needleRef = useRef({
    leftPos: 0,
    leftVel: 0,
    rightPos: 0,
    rightVel: 0,
    peakLeft: 0,
    peakRight: 0,
    peakHoldL: 0,
    peakHoldR: 0
  })

  useEffect(() => {
    const canvas = canvasRef.current
    const container = containerRef.current
    if (!canvas || !container) return
    const ctx = canvas.getContext('2d')

    let running = true

    const updateCanvasSize = () => {
      const dpr = window.devicePixelRatio || 1
      const rect = container.getBoundingClientRect()
      const displayWidth = Math.max(100, Math.floor(rect.width))
      const displayHeight = Math.max(40, Math.floor(rect.height))

      if (canvas.width !== displayWidth * dpr || canvas.height !== displayHeight * dpr) {
        canvas.width = displayWidth * dpr
        canvas.height = displayHeight * dpr
      }
    }

    updateCanvasSize()

    const resizeObserver = new ResizeObserver(() => {
      updateCanvasSize()
    })
    resizeObserver.observe(container)

    const render = () => {
      if (!running) return

      const dpr = window.devicePixelRatio || 1
      const rect = container.getBoundingClientRect()
      const displayWidth = Math.floor(rect.width)
      const displayHeight = Math.floor(rect.height)

      ctx.save()
      ctx.scale(dpr, dpr)
      ctx.clearRect(0, 0, displayWidth, displayHeight)

      const volume = usePlayerStore.getState().volume
      let levelL = 0
      let levelR = 0

      if (isPlaying && volume > 0 && engine.hasFreshNativeAudio()) {
        const { tL, tR } = engine.getTimeDomainData()
        let sumL = 0
        let sumR = 0
        const len = tL.length
        for (let i = 0; i < len; i++) {
          const l = (tL[i] - 128) / 128
          const r = (tR[i] - 128) / 128
          sumL += l * l
          sumR += r * r
        }
        levelL = Math.sqrt(sumL / len) * 2.5 * volume
        levelR = Math.sqrt(sumR / len) * 2.5 * volume
      } else if (isPlaying && volume > 0) {
        const time = Date.now() / 140
        levelL = Math.max(0, (0.35 + 0.3 * Math.sin(time) + 0.15 * Math.cos(time * 2.1)) * volume)
        levelR = Math.max(
          0,
          (0.35 + 0.3 * Math.cos(time * 1.2) + 0.15 * Math.sin(time * 2.5)) * volume
        )
      }

      const n = needleRef.current
      const targetL = Math.min(1.05, Math.max(0, levelL))
      const targetR = Math.min(1.05, Math.max(0, levelR))

      n.leftVel += (targetL - n.leftPos) * 0.2
      n.leftVel *= 0.72
      n.leftPos += n.leftVel

      n.rightVel += (targetR - n.rightPos) * 0.2
      n.rightVel *= 0.72
      n.rightPos += n.rightVel

      if (n.leftPos > n.peakLeft) {
        n.peakLeft = n.leftPos
        n.peakHoldL = 18
      } else if (n.peakHoldL > 0) {
        n.peakHoldL--
      } else {
        n.peakLeft = Math.max(0, n.peakLeft - 0.025)
      }

      if (n.rightPos > n.peakRight) {
        n.peakRight = n.rightPos
        n.peakHoldR = 18
      } else if (n.peakHoldR > 0) {
        n.peakHoldR--
      } else {
        n.peakRight = Math.max(0, n.peakRight - 0.025)
      }

      const meterWidth = (displayWidth - 14) / 2
      drawMcIntoshMeter(
        ctx,
        2,
        2,
        meterWidth,
        displayHeight - 4,
        n.leftPos,
        n.peakLeft > 0.88,
        'LEFT - WATTS'
      )
      drawMcIntoshMeter(
        ctx,
        meterWidth + 12,
        2,
        meterWidth,
        displayHeight - 4,
        n.rightPos,
        n.peakRight > 0.88,
        'RIGHT - WATTS'
      )

      ctx.restore()
      animFrameRef.current = requestAnimationFrame(render)
    }

    render()

    return () => {
      running = false
      resizeObserver.disconnect()
      if (animFrameRef.current) cancelAnimationFrame(animFrameRef.current)
    }
  }, [isPlaying])

  return (
    <div className="hifi-vu-container" ref={containerRef}>
      <canvas ref={canvasRef} className="hifi-vu-canvas" />
    </div>
  )
}

/** Draw authentic McIntosh cyan-blue backlit glass power output meter with strict bounds clipping */
function drawMcIntoshMeter(ctx, x, y, w, h, level, isPeak, channelLabel) {
  ctx.save()
  ctx.translate(x, y)

  // Enforce Strict Rectangular Clipping so Needles Never Bleed Outside Meter Bounds
  ctx.beginPath()
  ctx.rect(0, 0, w, h)
  ctx.clip()

  // Dark Smoked Frame Background
  ctx.fillStyle = '#03080e'
  ctx.fillRect(0, 0, w, h)
  ctx.strokeStyle = '#003355'
  ctx.lineWidth = 1
  ctx.strokeRect(0, 0, w, h)

  // Iconic McIntosh Blue Backlit Dial Face Gradient
  const dialGrad = ctx.createRadialGradient(w / 2, h * 0.45, 10, w / 2, h / 2, w * 0.65)
  dialGrad.addColorStop(0, '#00d2ff')
  dialGrad.addColorStop(0.35, '#0088e0')
  dialGrad.addColorStop(0.75, '#004488')
  dialGrad.addColorStop(1, '#002040')

  ctx.fillStyle = dialGrad
  ctx.fillRect(1, 1, w - 2, h - 2)

  // Inner Subtle Lighting Highlight
  ctx.fillStyle = 'rgba(0, 210, 255, 0.06)'
  ctx.fillRect(2, 2, w - 4, h - 4)

  const pivotX = w / 2
  const pivotY = h * 1.02
  const radius = h * 0.68

  const startAngle = -Math.PI * 0.74
  const endAngle = -Math.PI * 0.26
  const zeroDbAngle = startAngle + (endAngle - startAngle) * 0.75

  // Primary Scale Arc (Watts)
  ctx.beginPath()
  ctx.arc(pivotX, pivotY, radius, startAngle, zeroDbAngle)
  ctx.strokeStyle = '#ffffff'
  ctx.lineWidth = 1.8
  ctx.stroke()

  // Overload Red Arc
  ctx.beginPath()
  ctx.arc(pivotX, pivotY, radius, zeroDbAngle, endAngle)
  ctx.strokeStyle = '#ff1744'
  ctx.lineWidth = 2.5
  ctx.stroke()

  // Logarithmic Watts & dB Ticks
  const wattTicks = [
    { watt: '.01', val: 0.0 },
    { watt: '.1', val: 0.2 },
    { watt: '1', val: 0.38 },
    { watt: '10', val: 0.56 },
    { watt: '100', val: 0.75 },
    { watt: '200', val: 1.0 }
  ]

  ctx.textAlign = 'center'
  ctx.textBaseline = 'middle'
  ctx.font = '700 8.5px "Space Grotesk", sans-serif'

  wattTicks.forEach(({ watt, val }) => {
    const angle = startAngle + (endAngle - startAngle) * val
    const isRed = val >= 0.75
    const innerR = radius - 3
    const outerR = radius + (isRed ? 4 : 2)
    const textR = radius + 10

    const x1 = pivotX + Math.cos(angle) * innerR
    const y1 = pivotY + Math.sin(angle) * innerR
    const x2 = pivotX + Math.cos(angle) * outerR
    const y2 = pivotY + Math.sin(angle) * outerR

    ctx.beginPath()
    ctx.moveTo(x1, y1)
    ctx.lineTo(x2, y2)
    ctx.strokeStyle = isRed ? '#ff1744' : '#ffffff'
    ctx.lineWidth = isRed ? 2.2 : 1.2
    ctx.stroke()

    const tx = pivotX + Math.cos(angle) * textR
    const ty = pivotY + Math.sin(angle) * textR
    ctx.fillStyle = isRed ? '#ff1744' : '#ffffff'
    ctx.shadowColor = isRed ? '#ff1744' : '#00d2ff'
    ctx.shadowBlur = 3
    ctx.fillText(watt, tx, ty)
    ctx.shadowBlur = 0
  })

  // McIntosh Brand & Unit Branding Text
  ctx.fillStyle = '#ffffff'
  ctx.shadowColor = '#00d2ff'
  ctx.shadowBlur = 5
  ctx.font = '900 10.5px "Space Grotesk", sans-serif'
  ctx.fillText('POWER OUTPUT', pivotX, h * 0.76)
  ctx.shadowBlur = 0

  ctx.fillStyle = 'rgba(255, 255, 255, 0.75)'
  ctx.font = '700 7.5px sans-serif'
  ctx.fillText(channelLabel, pivotX, h * 0.88)

  // Crimson Red Needle
  const currentAngle = startAngle + (endAngle - startAngle) * Math.min(1.02, level)
  const needleLen = radius + 2
  const nx = pivotX + Math.cos(currentAngle) * needleLen
  const ny = pivotY + Math.sin(currentAngle) * needleLen

  // Needle Shadow
  ctx.beginPath()
  ctx.moveTo(pivotX + 1.5, pivotY + 1.5)
  ctx.lineTo(nx + 1.5, ny + 1.5)
  ctx.strokeStyle = 'rgba(0, 0, 0, 0.35)'
  ctx.lineWidth = 2
  ctx.stroke()

  // Primary Needle Stem
  ctx.beginPath()
  ctx.moveTo(pivotX, pivotY)
  ctx.lineTo(nx, ny)
  ctx.strokeStyle = '#ff1744'
  ctx.lineWidth = 2
  ctx.shadowColor = '#ff1744'
  ctx.shadowBlur = 4
  ctx.stroke()
  ctx.shadowBlur = 0

  // Pivot Cap
  ctx.beginPath()
  ctx.arc(pivotX, pivotY, 6, 0, Math.PI * 2)
  ctx.fillStyle = '#0a1016'
  ctx.fill()
  ctx.strokeStyle = '#00a8ff'
  ctx.lineWidth = 1.5
  ctx.stroke()

  // Peak Warning LED
  const ledX = w - 10
  const ledY = 10
  ctx.beginPath()
  ctx.arc(ledX, ledY, 3, 0, Math.PI * 2)
  if (isPeak) {
    ctx.fillStyle = '#ff1744'
    ctx.fill()
    ctx.shadowColor = '#ff1744'
    ctx.shadowBlur = 8
    ctx.stroke()
    ctx.shadowBlur = 0
  } else {
    ctx.fillStyle = '#1a0406'
    ctx.fill()
    ctx.strokeStyle = '#4a0c10'
    ctx.lineWidth = 1
    ctx.stroke()
  }

  // Smoked Glass Faceplate Refraction Glare Gradient
  const glassGrad = ctx.createLinearGradient(0, 0, w, h * 0.6)
  glassGrad.addColorStop(0, 'rgba(255, 255, 255, 0.25)')
  glassGrad.addColorStop(0.35, 'rgba(255, 255, 255, 0.05)')
  glassGrad.addColorStop(0.36, 'rgba(255, 255, 255, 0.0)')
  ctx.fillStyle = glassGrad
  ctx.fillRect(1, 1, w - 2, h - 2)

  ctx.restore()
}
