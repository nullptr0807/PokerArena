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
          background: 'rgba(0,0,0,0.9)',
          backdropFilter: 'blur(12px)',
          borderRadius: 16,
          padding: '20px 28px',
          zIndex: 20,
          minWidth: 320,
          textAlign: 'center',
        }}
      >
        <p style={{ fontSize: 15, fontWeight: 600, marginBottom: 12 }}>
          All-In Showdown — Run It Multiple Times?
        </p>

        {!results && (
          <div style={{ display: 'flex', gap: 8, justifyContent: 'center' }}>
            {[1, 2, 3].map((n) => (
              <button
                key={n}
                onClick={() => onRun(n)}
                style={{
                  padding: '8px 20px',
                  borderRadius: 10,
                  fontSize: 14,
                  fontWeight: 600,
                  background: n === 1 ? 'var(--accent)' : 'var(--bg-card)',
                  color: n === 1 ? '#fff' : 'var(--text-primary)',
                  border: '1px solid var(--border)',
                }}
              >
                {n === 1 ? 'Run Once' : `Run ${n}x`}
              </button>
            ))}
          </div>
        )}

        {results && (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 12 }}>
            {results.map((run, ri) => (
              <motion.div
                key={ri}
                initial={{ opacity: 0, x: -20 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: ri * 0.3 }}
                style={{
                  background: 'var(--bg-card)',
                  borderRadius: 12,
                  padding: '12px 16px',
                }}
              >
                <div style={{ fontSize: 12, color: 'var(--text-muted)', marginBottom: 6 }}>
                  Run {ri + 1}
                </div>
                <div style={{ display: 'flex', gap: 4, justifyContent: 'center', marginBottom: 8 }}>
                  {run.board.map((c, ci) => (
                    <Card key={ci} card={c} size="sm" />
                  ))}
                </div>
                {run.winners.map((w, wi) => (
                  <div key={wi} style={{ fontSize: 13, fontWeight: 500 }}>
                    {playerNames[w.player] ?? `Player ${w.player}`} wins {w.amount}
                    {w.hand_rank ? ` — ${w.hand_rank}` : ''}
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
