import { type FC, useState } from 'react'
import { motion } from 'framer-motion'
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
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      justifyContent: 'center',
      height: '100%',
      gap: 0,
    }}>
      {/* Main card */}
      <motion.div
        initial={{ opacity: 0, y: 20 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ duration: 0.6, ease: [0.22, 1, 0.36, 1] }}
        style={{
          background: 'rgba(18, 18, 22, 0.7)',
          backdropFilter: 'blur(40px)',
          borderRadius: 28,
          border: '1px solid var(--border-light)',
          padding: '48px 56px',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          gap: 28,
          boxShadow: '0 24px 80px rgba(0,0,0,0.5)',
          maxWidth: 480,
          width: '90%',
        }}
      >
        {/* Logo */}
        <div style={{ textAlign: 'center', marginBottom: 4 }}>
          <h1 style={{
            fontSize: 40,
            fontWeight: 800,
            letterSpacing: '-0.03em',
            background: 'linear-gradient(135deg, #f0f0f5 0%, #8b8b9e 100%)',
            WebkitBackgroundClip: 'text',
            WebkitTextFillColor: 'transparent',
          }}>
            Poker Arena
          </h1>
          <p style={{
            color: 'var(--text-muted)',
            fontSize: 14,
            marginTop: 6,
            letterSpacing: '0.02em',
          }}>
            No-Limit Texas Hold'em · 人机对战
          </p>
        </div>

        {/* Player count */}
        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 10 }}>
          <span style={{ fontSize: 12, color: 'var(--text-muted)', textTransform: 'uppercase', letterSpacing: '0.08em', fontWeight: 600 }}>
            玩家人数
          </span>
          <div style={{ display: 'flex', gap: 6 }}>
            {[2, 3, 4, 5, 6].map((n) => (
              <motion.button
                key={n}
                whileHover={{ scale: 1.08 }}
                whileTap={{ scale: 0.95 }}
                onClick={() => handleNumChange(n)}
                style={{
                  width: 44,
                  height: 44,
                  borderRadius: 12,
                  fontSize: 16,
                  fontWeight: 700,
                  background: numPlayers === n
                    ? 'linear-gradient(135deg, var(--accent), #059669)'
                    : 'var(--glass)',
                  color: numPlayers === n ? '#000' : 'var(--text-secondary)',
                  border: numPlayers === n
                    ? 'none'
                    : '1px solid var(--border)',
                  boxShadow: numPlayers === n ? '0 4px 16px rgba(52,211,153,0.3)' : 'none',
                }}
              >
                {n}
              </motion.button>
            ))}
          </div>
        </div>

        {/* AI difficulty */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: 8, alignItems: 'center', width: '100%' }}>
          <span style={{ fontSize: 12, color: 'var(--text-muted)', textTransform: 'uppercase', letterSpacing: '0.08em', fontWeight: 600 }}>
            AI 难度
          </span>
          {difficulties.map((diff, i) => (
            <div key={i} style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
              <span style={{ fontSize: 12, color: 'var(--text-secondary)', width: 48, textAlign: 'right' }}>
                AI {i + 1}
              </span>
              <div style={{
                display: 'flex',
                gap: 2,
                background: 'var(--glass)',
                borderRadius: 10,
                padding: 2,
                border: '1px solid var(--border)',
              }}>
                {[
                  { key: 'normal', label: '普通' },
                  { key: 'medium', label: '中等' },
                  { key: 'advanced', label: '高级' },
                ].map((d) => (
                  <button
                    key={d.key}
                    onClick={() => handleDiffChange(i, d.key)}
                    style={{
                      padding: '5px 14px',
                      borderRadius: 8,
                      fontSize: 12,
                      fontWeight: 600,
                      background: diff === d.key ? 'rgba(52,211,153,0.2)' : 'transparent',
                      color: diff === d.key ? 'var(--accent)' : 'var(--text-muted)',
                      transition: 'all 0.2s ease',
                    }}
                  >
                    {d.label}
                  </button>
                ))}
              </div>
            </div>
          ))}
        </div>

        {/* Debug toggle */}
        <label style={{
          display: 'flex',
          alignItems: 'center',
          gap: 8,
          cursor: 'pointer',
          fontSize: 12,
          color: debugMode ? 'var(--warning)' : 'var(--text-muted)',
          transition: 'color 0.2s',
        }}>
          <input
            type="checkbox"
            checked={debugMode}
            onChange={(e) => setDebugMode(e.target.checked)}
            style={{ accentColor: '#fbbf24', width: 14, height: 14 }}
          />
          Debug Mode
        </label>

        {/* Start button */}
        <motion.button
          whileHover={{ scale: 1.03 }}
          whileTap={{ scale: 0.97 }}
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
            width: '100%',
            padding: '14px 0',
            borderRadius: 14,
            fontSize: 17,
            fontWeight: 700,
            background: 'linear-gradient(135deg, var(--accent), #059669)',
            color: '#000',
            boxShadow: '0 4px 24px rgba(52,211,153,0.3)',
            letterSpacing: '-0.01em',
          }}
        >
          开始游戏
        </motion.button>
      </motion.div>
    </div>
  )
}
