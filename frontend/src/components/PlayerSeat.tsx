import type { FC } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import type { PlayerState } from '../types'
import { Card } from './Card'

const THINKING_LABELS: Record<string, string> = {
  analyzing: '分析中...',
  computing: '计算中...',
  'solving subgame': '求解子博弈...',
  deciding: '决策中...',
}

interface PlayerSeatProps {
  player: PlayerState
  isCurrentPlayer: boolean
  isButton: boolean
  thinkingStage?: string | null
}

export const PlayerSeat: FC<PlayerSeatProps> = ({
  player,
  isCurrentPlayer,
  isButton,
  thinkingStage,
}) => {
  const opacity = player.folded ? 0.4 : 1

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: 6,
        opacity,
        transition: 'all 0.3s ease',
        padding: 12,
        borderRadius: 16,
        background: isCurrentPlayer
          ? 'rgba(16, 185, 129, 0.15)'
          : 'rgba(255, 255, 255, 0.05)',
        border: isCurrentPlayer
          ? '2px solid var(--accent)'
          : '2px solid transparent',
        minWidth: 120,
      }}
    >
      {/* Name & Button */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
        <span
          style={{
            fontSize: 14,
            fontWeight: 600,
            color: player.is_human ? 'var(--accent)' : 'var(--text-primary)',
          }}
        >
          {player.name}
        </span>
        {isButton && (
          <span
            style={{
              fontSize: 10,
              background: '#f59e0b',
              color: '#000',
              borderRadius: 10,
              padding: '1px 6px',
              fontWeight: 700,
            }}
          >
            D
          </span>
        )}
        {player.all_in && (
          <span
            style={{
              fontSize: 10,
              background: 'var(--danger)',
              color: '#fff',
              borderRadius: 10,
              padding: '1px 6px',
              fontWeight: 700,
            }}
          >
            ALL IN
          </span>
        )}
      </div>

      {/* Hole cards */}
      <div style={{ display: 'flex', gap: 4 }}>
        {player.hole_cards
          ? player.hole_cards.map((c, i) => <Card key={i} card={c} size="sm" />)
          : player.folded
            ? null
            : [0, 1].map((i) => <Card key={i} card={null} size="sm" />)}
      </div>

      {/* Chips */}
      <span
        style={{
          fontSize: 16,
          fontWeight: 700,
          fontFamily: 'var(--font-mono)',
          color: 'var(--text-primary)',
        }}
      >
        {player.chips.toLocaleString()}
      </span>

      {/* Current street bet */}
      {player.bet_this_street > 0 && (
        <span
          style={{
            fontSize: 12,
            color: 'var(--warning)',
            fontFamily: 'var(--font-mono)',
          }}
        >
          Bet: {player.bet_this_street}
        </span>
      )}

      {/* Stats */}
      <div
        style={{
          display: 'flex',
          gap: 8,
          fontSize: 10,
          color: 'var(--text-muted)',
        }}
      >
        <span>VPIP {player.stats.vpip}%</span>
        <span>{player.stats.bb_per_hand > 0 ? '+' : ''}{player.stats.bb_per_hand} bb/h</span>
      </div>

      {/* AI Thinking Indicator */}
      <AnimatePresence>
        {thinkingStage && (
          <motion.div
            initial={{ opacity: 0, y: 4 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -4 }}
            transition={{ duration: 0.2 }}
            style={{
              fontSize: 11,
              color: '#60a5fa',
              fontWeight: 500,
              display: 'flex',
              alignItems: 'center',
              gap: 4,
            }}
          >
            <motion.span
              animate={{ opacity: [0.4, 1, 0.4] }}
              transition={{ duration: 1.5, repeat: Infinity, ease: 'easeInOut' }}
              style={{ display: 'inline-block' }}
            >
              🧠
            </motion.span>
            <span>{THINKING_LABELS[thinkingStage] ?? thinkingStage}</span>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  )
}
