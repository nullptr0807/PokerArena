import type { FC } from 'react'
import { motion } from 'framer-motion'
import type { PlayerState } from '../types'

interface StatsOverlayProps {
  visible: boolean
  players: PlayerState[]
  handNumber: number
  onClose: () => void
}

export const StatsOverlay: FC<StatsOverlayProps> = ({
  visible,
  players,
  handNumber,
  onClose,
}) => {
  if (!visible) return null

  return (
    <motion.div
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
      style={{
        position: 'fixed',
        inset: 0,
        background: 'rgba(0,0,0,0.8)',
        backdropFilter: 'blur(8px)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 50,
      }}
      onClick={onClose}
    >
      <motion.div
        initial={{ scale: 0.9, y: 20 }}
        animate={{ scale: 1, y: 0 }}
        style={{
          background: 'var(--bg-secondary)',
          borderRadius: 20,
          padding: 32,
          minWidth: 500,
          maxWidth: '80vw',
          boxShadow: 'var(--shadow-lg)',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        <h2 style={{ fontSize: 22, fontWeight: 700, marginBottom: 4 }}>
          📊 Session Stats
        </h2>
        <p style={{ fontSize: 13, color: 'var(--text-muted)', marginBottom: 20 }}>
          Hand #{handNumber}
        </p>

        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ borderBottom: '1px solid var(--border)' }}>
              {['Player', 'Chips', 'VPIP', 'BB/Hand', 'Profit'].map((h) => (
                <th
                  key={h}
                  style={{
                    padding: '8px 12px',
                    textAlign: 'left',
                    fontSize: 12,
                    color: 'var(--text-muted)',
                    fontWeight: 600,
                  }}
                >
                  {h}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {players.map((p) => (
              <tr
                key={p.index}
                style={{ borderBottom: '1px solid var(--border)' }}
              >
                <td style={{ padding: '10px 12px', fontWeight: 600 }}>
                  {p.name}
                  {p.is_human && (
                    <span style={{ color: 'var(--accent)', marginLeft: 6, fontSize: 11 }}>YOU</span>
                  )}
                </td>
                <td style={{ padding: '10px 12px', fontFamily: 'var(--font-mono)' }}>
                  {p.chips.toLocaleString()}
                </td>
                <td style={{ padding: '10px 12px' }}>{p.stats.vpip}%</td>
                <td
                  style={{
                    padding: '10px 12px',
                    fontFamily: 'var(--font-mono)',
                    color: p.stats.bb_per_hand > 0 ? 'var(--accent)' : 'var(--danger)',
                  }}
                >
                  {p.stats.bb_per_hand > 0 ? '+' : ''}
                  {p.stats.bb_per_hand}
                </td>
                <td
                  style={{
                    padding: '10px 12px',
                    fontFamily: 'var(--font-mono)',
                    color: p.stats.total_profit_bb > 0 ? 'var(--accent)' : 'var(--danger)',
                  }}
                >
                  {p.stats.total_profit_bb > 0 ? '+' : ''}
                  {p.stats.total_profit_bb} BB
                </td>
              </tr>
            ))}
          </tbody>
        </table>

        <button
          onClick={onClose}
          style={{
            marginTop: 20,
            padding: '8px 24px',
            borderRadius: 10,
            background: 'var(--bg-card)',
            color: 'var(--text-primary)',
            fontSize: 14,
            fontWeight: 500,
          }}
        >
          Close
        </button>
      </motion.div>
    </motion.div>
  )
}
