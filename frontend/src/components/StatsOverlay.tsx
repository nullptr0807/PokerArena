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
        background: 'rgba(0,0,0,0.7)',
        backdropFilter: 'blur(20px)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 50,
      }}
      onClick={onClose}
    >
      <motion.div
        initial={{ scale: 0.92, y: 20 }}
        animate={{ scale: 1, y: 0 }}
        transition={{ type: 'spring', damping: 22 }}
        style={{
          background: 'rgba(18, 18, 22, 0.9)',
          backdropFilter: 'blur(40px)',
          borderRadius: 24,
          padding: '32px 36px',
          minWidth: 500,
          maxWidth: '80vw',
          border: '1px solid var(--border-light)',
          boxShadow: '0 24px 80px rgba(0,0,0,0.6)',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        <h2 style={{ fontSize: 20, fontWeight: 700, marginBottom: 4, letterSpacing: '-0.02em' }}>
          统计数据
        </h2>
        <p style={{ fontSize: 12, color: 'var(--text-muted)', marginBottom: 20 }}>
          Hand #{handNumber}
        </p>

        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ borderBottom: '1px solid var(--border)' }}>
              {['玩家', '手数', '筹码', 'VPIP', 'BB/100', '盈亏'].map((h) => (
                <th key={h} style={{
                  padding: '8px 12px',
                  textAlign: 'left',
                  fontSize: 11,
                  color: 'var(--text-muted)',
                  fontWeight: 600,
                  textTransform: 'uppercase',
                  letterSpacing: '0.06em',
                }}>
                  {h}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {players.map((p) => (
              <tr key={p.index} style={{ borderBottom: '1px solid var(--border)' }}>
                <td style={{ padding: '10px 12px', fontWeight: 600, fontSize: 14 }}>
                  {p.name}
                  {p.is_human && (
                    <span style={{
                      color: 'var(--accent)',
                      marginLeft: 6,
                      fontSize: 10,
                      background: 'rgba(52,211,153,0.1)',
                      borderRadius: 4,
                      padding: '1px 6px',
                    }}>YOU</span>
                  )}
                </td>
                <td style={{ padding: '10px 12px', fontFamily: 'var(--font-mono)', fontSize: 13, color: 'var(--text-secondary)' }}>
                  {p.stats.hands_played}
                </td>
                <td style={{ padding: '10px 12px', fontFamily: 'var(--font-mono)', fontSize: 14 }}>
                  {p.chips.toLocaleString()}
                </td>
                <td style={{ padding: '10px 12px', fontSize: 13 }}>{p.stats.vpip}%</td>
                <td style={{
                  padding: '10px 12px',
                  fontFamily: 'var(--font-mono)',
                  fontSize: 13,
                  color: p.stats.bb_per_hand > 0 ? 'var(--accent)' : 'var(--danger)',
                }}>
                  {p.stats.bb_per_hand > 0 ? '+' : ''}{p.stats.bb_per_hand}
                </td>
                <td style={{
                  padding: '10px 12px',
                  fontFamily: 'var(--font-mono)',
                  fontSize: 13,
                  color: p.stats.total_profit_bb > 0 ? 'var(--accent)' : 'var(--danger)',
                }}>
                  {p.stats.total_profit_bb > 0 ? '+' : ''}{p.stats.total_profit_bb} BB
                </td>
              </tr>
            ))}
          </tbody>
        </table>

        <button
          onClick={onClose}
          style={{
            marginTop: 24,
            padding: '8px 24px',
            borderRadius: 10,
            background: 'var(--glass)',
            color: 'var(--text-secondary)',
            fontSize: 13,
            fontWeight: 600,
            border: '1px solid var(--border)',
          }}
        >
          关闭
        </button>
      </motion.div>
    </motion.div>
  )
}
