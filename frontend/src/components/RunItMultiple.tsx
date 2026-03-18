import type { FC } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { Card } from './Card'

interface RunItProps {
  visible: boolean
  onRun: (times: number) => void
  results: Array<{
    board: string[]
    winners: Array<{ player: number; amount: number; hand_rank: string }>
  }> | null
  playerNames: string[]
}

export const RunItMultiple: FC<RunItProps> = ({
  visible,
  onRun,
  results,
  playerNames,
}) => {
  if (!visible) return null

  return (
    <AnimatePresence>
      <motion.div
        initial={{ opacity: 0, y: 20 }}
        animate={{ opacity: 1, y: 0 }}
        exit={{ opacity: 0, y: 20 }}
        style={{
          position: 'absolute',
          bottom: 80,
          left: '50%',
          transform: 'translateX(-50%)',
          background: 'rgba(9, 9, 11, 0.9)',
          backdropFilter: 'blur(24px)',
          borderRadius: 20,
          padding: '24px 32px',
          zIndex: 20,
          minWidth: 340,
          textAlign: 'center',
          border: '1px solid var(--border-light)',
          boxShadow: '0 16px 48px rgba(0,0,0,0.5)',
        }}
      >
        <p style={{ fontSize: 15, fontWeight: 700, marginBottom: 14, letterSpacing: '-0.01em' }}>
          All-In Showdown
        </p>
        <p style={{ fontSize: 12, color: 'var(--text-muted)', marginBottom: 16 }}>
          选择发牌次数
        </p>

        {!results && (
          <div style={{ display: 'flex', gap: 8, justifyContent: 'center' }}>
            {[1, 2, 3].map((n) => (
              <motion.button
                key={n}
                whileHover={{ scale: 1.05 }}
                whileTap={{ scale: 0.95 }}
                onClick={() => onRun(n)}
                style={{
                  padding: '8px 22px',
                  borderRadius: 12,
                  fontSize: 13,
                  fontWeight: 700,
                  background: n === 1
                    ? 'linear-gradient(135deg, var(--accent), #059669)'
                    : 'var(--glass)',
                  color: n === 1 ? '#000' : 'var(--text-primary)',
                  border: n === 1 ? 'none' : '1px solid var(--border)',
                }}
              >
                {n === 1 ? '发一次' : `发 ${n} 次`}
              </motion.button>
            ))}
          </div>
        )}

        {results && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
            {results.map((run, ri) => (
              <motion.div
                key={ri}
                initial={{ opacity: 0, x: -16 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: ri * 0.25 }}
                style={{
                  background: 'var(--glass)',
                  borderRadius: 14,
                  padding: '12px 16px',
                  border: '1px solid var(--border)',
                }}
              >
                <div style={{ fontSize: 11, color: 'var(--text-muted)', marginBottom: 6, fontWeight: 600 }}>
                  第 {ri + 1} 次
                </div>
                <div style={{ display: 'flex', gap: 4, justifyContent: 'center', marginBottom: 8 }}>
                  {run.board.map((c, ci) => (
                    <Card key={ci} card={c} size="sm" />
                  ))}
                </div>
                {run.winners.map((w, wi) => (
                  <div key={wi} style={{ fontSize: 13, fontWeight: 600 }}>
                    <span style={{ color: 'var(--accent)' }}>{playerNames[w.player] ?? `Player ${w.player}`}</span>
                    <span style={{ color: 'var(--text-muted)' }}> 赢得 </span>
                    <span style={{ color: '#fbbf24', fontFamily: 'var(--font-mono)' }}>{w.amount}</span>
                    {w.hand_rank && <span style={{ color: 'var(--text-secondary)', fontSize: 11 }}> ({w.hand_rank})</span>}
                  </div>
                ))}
              </motion.div>
            ))}
          </div>
        )}
      </motion.div>
    </AnimatePresence>
  )
}
