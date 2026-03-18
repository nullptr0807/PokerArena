import type { FC } from 'react'
import { motion } from 'framer-motion'

const SUIT_MAP: Record<string, { symbol: string; color: string }> = {
  s: { symbol: '♠', color: '#1a1a2e' },
  h: { symbol: '♥', color: '#dc2626' },
  d: { symbol: '♦', color: '#2563eb' },
  c: { symbol: '♣', color: '#15803d' },
}

const RANK_DISPLAY: Record<string, string> = {
  T: '10', J: 'J', Q: 'Q', K: 'K', A: 'A',
}

interface CardProps {
  card: string | null
  size?: 'sm' | 'md' | 'lg'
}

const SIZES = {
  sm: { width: 56, height: 80, rank: 20, suit: 18, corner: 11, cornerSuit: 10, radius: 8 },
  md: { width: 72, height: 102, rank: 26, suit: 24, corner: 14, cornerSuit: 12, radius: 10 },
  lg: { width: 90, height: 128, rank: 34, suit: 30, corner: 18, cornerSuit: 16, radius: 12 },
}

export const Card: FC<CardProps> = ({ card, size = 'md' }) => {
  const d = SIZES[size]

  // Card back
  if (!card) {
    return (
      <motion.div
        initial={{ rotateY: 180, opacity: 0 }}
        animate={{ rotateY: 0, opacity: 1 }}
        transition={{ duration: 0.4, ease: 'easeOut' }}
        style={{
          width: d.width,
          height: d.height,
          borderRadius: d.radius,
          background: 'linear-gradient(135deg, #1e3a5f, #0f2440)',
          border: '2px solid rgba(96, 165, 250, 0.25)',
          boxShadow: '0 4px 16px rgba(0,0,0,0.5)',
          position: 'relative',
          overflow: 'hidden',
        }}
      >
        <div style={{
          position: 'absolute',
          inset: 4,
          borderRadius: d.radius - 3,
          border: '1px solid rgba(96, 165, 250, 0.15)',
          background: `
            repeating-linear-gradient(45deg, transparent, transparent 3px, rgba(96,165,250,0.04) 3px, rgba(96,165,250,0.04) 4px),
            repeating-linear-gradient(-45deg, transparent, transparent 3px, rgba(96,165,250,0.04) 3px, rgba(96,165,250,0.04) 4px)
          `,
        }} />
      </motion.div>
    )
  }

  const rankChar = card[0]
  const rank = RANK_DISPLAY[rankChar] ?? rankChar
  const suit = SUIT_MAP[card[1]] ?? { symbol: '?', color: '#888' }
  const isRed = card[1] === 'h' || card[1] === 'd'

  return (
    <motion.div
      initial={{ rotateY: -90, opacity: 0 }}
      animate={{ rotateY: 0, opacity: 1 }}
      transition={{ duration: 0.4, ease: 'easeOut' }}
      style={{
        width: d.width,
        height: d.height,
        borderRadius: d.radius,
        background: 'linear-gradient(175deg, #fffffe 0%, #f5f5f0 60%, #eeeee8 100%)',
        border: '1px solid rgba(0,0,0,0.12)',
        boxShadow: '0 4px 16px rgba(0,0,0,0.35), 0 1px 2px rgba(0,0,0,0.2)',
        position: 'relative',
        overflow: 'hidden',
        color: isRed ? '#dc2626' : '#1a1a2e',
        fontFamily: "'Georgia', 'Times New Roman', serif",
      }}
    >
      {/* Top-left corner */}
      <div style={{
        position: 'absolute',
        top: 4,
        left: 5,
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        lineHeight: 1.1,
      }}>
        <span style={{
          fontSize: d.corner,
          fontWeight: 700,
        }}>{rank}</span>
        <span style={{ fontSize: d.cornerSuit }}>{suit.symbol}</span>
      </div>

      {/* Center suit — large */}
      <div style={{
        position: 'absolute',
        top: '50%',
        left: '50%',
        transform: 'translate(-50%, -50%)',
        fontSize: d.suit * 1.5,
        lineHeight: 1,
        opacity: 0.85,
      }}>
        {suit.symbol}
      </div>

      {/* Bottom-right corner */}
      <div style={{
        position: 'absolute',
        bottom: 4,
        right: 5,
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        lineHeight: 1.1,
        transform: 'rotate(180deg)',
      }}>
        <span style={{
          fontSize: d.corner,
          fontWeight: 700,
        }}>{rank}</span>
        <span style={{ fontSize: d.cornerSuit }}>{suit.symbol}</span>
      </div>
    </motion.div>
  )
}
