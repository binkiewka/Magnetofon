/* eslint-disable react/prop-types */
import '../assets/cassette.css'

function clamp01(value) {
  if (!Number.isFinite(value)) return 0
  return Math.max(0, Math.min(1, value))
}

export function CassetteDeck({ file, isPlaying, progress = 0, duration = 0 }) {
  const pct = clamp01(duration > 0 ? progress / duration : 0)
  const leftTape = 34 + pct * 34
  const rightTape = 68 - pct * 34
  const spinState = isPlaying ? 'running' : 'paused'

  return (
    <div className={`cassette-deck ${isPlaying ? 'is-playing' : 'is-paused'}`}>
      <div className="cassette-shell">
        <div className="cassette-screw screw-tl" />
        <div className="cassette-screw screw-tr" />
        <div className="cassette-screw screw-bl" />
        <div className="cassette-screw screw-br" />

        <div className="cassette-window">
          <div className="meter-stack meter-left" aria-hidden="true">
            {[0, 1, 2, 3, 4].map((i) => (
              <span key={i} className={`meter-led led-${i}`} />
            ))}
            <small>L</small>
          </div>
          <div className="meter-stack meter-right" aria-hidden="true">
            {[0, 1, 2, 3, 4].map((i) => (
              <span key={i} className={`meter-led led-${i}`} />
            ))}
            <small>R</small>
          </div>

          <div className="tape-band tape-top" />
          <div className="tape-band tape-bottom" />

          <div
            className="reel reel-left"
            style={{
              '--tape-fill': `${leftTape}%`,
              '--spin-state': spinState,
              '--spin-direction': 'reverse'
            }}
          >
            <div className="reel-hub" />
            <div className="reel-spokes" />
          </div>

          <div
            className="reel reel-right"
            style={{
              '--tape-fill': `${rightTape}%`,
              '--spin-state': spinState,
              '--spin-direction': 'reverse'
            }}
          >
            <div className="reel-hub" />
            <div className="reel-spokes" />
          </div>

          <div className="cassette-head">
            <div className="head-post" />
            <div className="head-core" />
            <div className="head-post" />
          </div>
        </div>

        <div className="cassette-footer">
          <span>MAXELL</span>
          <span>UR90</span>
          <span>{isPlaying ? 'PLAY' : file ? 'CUED' : 'EMPTY'}</span>
        </div>
      </div>
    </div>
  )
}
