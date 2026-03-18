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
  const opacity = player.folded ? 0.35 : 1
  const initials = player.name.slice(0, 2).toUpperCase()
  const avatarBg = player.is_human
    ? 'linear-gradient(135deg, #34d399, #059669)'
    : 'linear-gradient(135deg, #60a5fa, #3b82f6)'

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: 6,
        opacity,
        transition: 'all 0.3s ease',
        minWidth: 110,
      }}
    >
      {/* Avatar circle */}
      <div style={{
        position: 'relative',
      }}>
        <div style={{
          width: 48,
          height: 48,
          borderRadius: '50%',
          background: avatarBg,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          fontSize: 16,
          fontWeight: 700,
          color: '#fff',
          boxShadow: isCurrentPlayer
            ? '0 0 0 3px var(--accent), 0 0 20px rgba(52,211,153,0.4)'
            : '0 4px 12px rgba(0,0,0,0.4)',
          transition: 'box-shadow 0.3s ease',
        }}>
          {initials}
        </div>
        {/* Dealer button */}
        {isButton && (
          <div style={{
            position: 'absolute',
            bottom: -2,
            right: -2,
            width: 18,
            height: 18,
            borderRadius: '50%',
            background: '#fbbf24',
            color: '#000',
            fontSize: 9,
            fontWeight: 800,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            boxShadow: '0 2px 6px rgba(0,0,0,0.3)',
            border: '2px solid var(--bg-primary)',
          }}>
            D
          </div>
        )}
      </div>

      {/* Name row */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 5 }}>
        <span style={{
          fontSize: 13,
          fontWeight: 600,
          color: player.is_human ? 'var(--accent)' : 'var(--text-primary)',
          letterSpacing: '-0.01em',
        }}>
          {player.name}
        </span>
        {player.all_in && (
          <span style={{
            fontSize: 9,
            background: 'var(--danger)',
            color: '#fff',
            borderRadius: 6,
            padding: '1px 6px',
            fontWeight: 700,
            textTransform: 'uppercase',
            letterSpacing: '0.05em',
          }}>
            ALL IN
          </span>
        )}
      </div>

      {/* Hole cards */}
      <div style={{ display: 'flex', gap: 5 }}>
        {player.hole_cards
          ? player.hole_cards.map((c, i) => <Card key={i} card={c} size="md" />)
          : player.folded
            ? null
            : [0, 1].map((i) => <Card key={i} card={null} size="sm" />)}
      </div>

      {/* Chips - glass pill */}
      <div style={{
        background: 'rgba(0,0,0,0.6)',
        backdropFilter: 'blur(8px)',
        borderRadius: 20,
        padding: '4px 14px',
        border: '1px solid rgba(255,255,255,0.1)',
      }}>
        <span style={{
          fontSize: 15,
          fontWeight: 700,
          fontFamily: 'var(--font-mono)',
          color: '#fff',
          letterSpacing: '-0.02em',
        }}>
          💰 {player.chips.toLocaleString()}
        </span>
      </div>

      {/* Current street bet */}
      {player.bet_this_street > 0 && (
        <div style={{
          background: 'rgba(245,158,11,0.15)',
          borderRadius: 14,
          padding: '3px 12px',
          border: '1px solid rgba(245,158,11,0.3)',
        }}>
          <span style={{
            fontSize: 13,
            color: '#fbbf24',
            fontFamily: 'var(--font-mono)',
            fontWeight: 700,
          }}>
            ⬆ {player.bet_this_street}
          </span>
        </div>
      )}

      {/* Stats */}
      <div style={{
        display: 'flex',
        gap: 8,
        fontSize: 11,
        color: 'rgba(255,255,255,0.55)',
        fontWeight: 500,
        letterSpacing: '0.01em',
      }}>
        <span style={{
          background: 'rgba(255,255,255,0.06)',
          borderRadius: 6,
          padding: '1px 6px',
        }}>VPIP {player.stats.vpip}%</span>
        <span style={{
          background: 'rgba(255,255,255,0.06)',
          borderRadius: 6,
          padding: '1px 6px',
          color: player.stats.bb_per_hand >= 0 ? 'rgba(74,222,128,0.7)' : 'rgba(248,113,113,0.7)',
        }}>{player.stats.bb_per_hand > 0 ? '+' : ''}{player.stats.bb_per_hand} bb/100</span>
      </div>

      {/* AI Thinking */}
      <AnimatePresence>
        {thinkingStage && (
          <motion.div
            initial={{ opacity: 0, y: 4 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -4 }}
            transition={{ duration: 0.2 }}
            style={{
              fontSize: 10,
              color: '#60a5fa',
              fontWeight: 500,
              display: 'flex',
              alignItems: 'center',
              gap: 4,
              background: 'rgba(96, 165, 250, 0.1)',
              borderRadius: 8,
              padding: '2px 8px',
            }}
          >
            <motion.span
              animate={{ opacity: [0.4, 1, 0.4] }}
              transition={{ duration: 1.5, repeat: Infinity, ease: 'easeInOut' }}
              style={{ display: 'inline-block' }}
            >
              ◉
            </motion.span>
            <span>{THINKING_LABELS[thinkingStage] ?? thinkingStage}</span>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  )
}
