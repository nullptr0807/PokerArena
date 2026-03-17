import { type FC, useState } from 'react'
import type { GameConfig } from '../types'

interface SetupProps {
  onStart: (config: GameConfig) => void
}

export const Setup: FC<SetupProps> = ({ onStart }) => {
  const [numPlayers, setNumPlayers] = useState(2)
  const [difficulties, setDifficulties] = useState<string[]>(['normal'])
  const [debugMode, setDebugMode] = useState(false)

  const handleNumChange = (n: number) => {
    setNumPlayers(n)
    setDifficulties(Array(n - 1).fill('normal'))
  }

  const handleDiffChange = (idx: number, val: string) => {
    const next = [...difficulties]
    next[idx] = val
    setDifficulties(next)
  }

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '100%',
        gap: 32,
      }}
    >
      <h1 style={{ fontSize: 48, fontWeight: 700, letterSpacing: -1 }}>
        Poker Arena
      </h1>
      <p style={{ color: 'var(--text-secondary)', fontSize: 16 }}>
        No-Limit Texas Hold'em · You vs AI
      </p>

      {/* Player count */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
        <span style={{ fontSize: 14, color: 'var(--text-secondary)' }}>
          Players:
        </span>
        {[2, 3, 4, 5, 6].map((n) => (
          <button
            key={n}
            onClick={() => handleNumChange(n)}
            style={{
              width: 40,
              height: 40,
              borderRadius: 10,
              fontSize: 16,
              fontWeight: 600,
              background:
                numPlayers === n ? 'var(--accent)' : 'var(--bg-card)',
              color: numPlayers === n ? '#fff' : 'var(--text-primary)',
            }}
          >
            {n}
          </button>
        ))}
      </div>

      {/* AI difficulty settings */}
      <div
        style={{
          display: 'flex',
          flexDirection: 'column',
          gap: 8,
          alignItems: 'center',
        }}
      >
        {difficulties.map((diff, i) => (
          <div
            key={i}
            style={{ display: 'flex', alignItems: 'center', gap: 8 }}
          >
            <span
              style={{
                fontSize: 13,
                color: 'var(--text-secondary)',
                width: 60,
              }}
            >
              AI {i + 1}:
            </span>
            {['normal', 'medium', 'advanced'].map((d) => (
              <button
                key={d}
                onClick={() => handleDiffChange(i, d)}
                style={{
                  padding: '6px 14px',
                  borderRadius: 8,
                  fontSize: 13,
                  fontWeight: 500,
                  background: diff === d ? 'var(--accent)' : 'var(--bg-card)',
                  color: diff === d ? '#fff' : 'var(--text-secondary)',
                }}
              >
                {d === 'normal' ? '普通' : d === 'medium' ? '中等' : '高级'}
              </button>
            ))}
          </div>
        ))}
      </div>

      {/* Debug mode toggle */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
        <label
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: 8,
            cursor: 'pointer',
            fontSize: 13,
            color: debugMode ? '#f59e0b' : 'var(--text-secondary)',
          }}
        >
          <input
            type="checkbox"
            checked={debugMode}
            onChange={(e) => setDebugMode(e.target.checked)}
            style={{ accentColor: '#f59e0b' }}
          />
          🐛 Debug Mode
        </label>
      </div>

      {/* Start button */}
      <button
        onClick={() =>
          onStart({
            num_players: numPlayers,
            small_blind: 1,
            big_blind: 2,
            starting_chips: 400,
            ai_difficulties: difficulties,
            debug_mode: debugMode,
          })
        }
        style={{
          padding: '14px 48px',
          borderRadius: 14,
          fontSize: 18,
          fontWeight: 700,
          background: 'var(--accent)',
          color: '#fff',
          boxShadow: 'var(--shadow)',
          marginTop: 8,
        }}
      >
        开始游戏
      </button>
    </div>
  )
}
