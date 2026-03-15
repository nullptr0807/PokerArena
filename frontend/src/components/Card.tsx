import type { FC } from 'react'
import { motion } from 'framer-motion'

// Card display: converts "As" → "A♠", "Td" → "T♦", etc.
const SUIT_MAP: Record<string, { symbol: string; color: string }> = {
  s: { symbol: '♠', color: '#e0e0e0' },
  h: { symbol: '♥', color: '#ef4444' },
  d: { symbol: '♦', color: '#3b82f6' },
  c: { symbol: '♣', color: '#22c55e' },
}

interface CardProps {
  card: string | null  // e.g. "As", "Td", or null for face-down
  size?: 'sm' | 'md' | 'lg'
}

const SIZES = {
  sm: { width: 40, height: 56, fontSize: 14 },
  md: { width: 56, height: 80, fontSize: 18 },
  lg: { width: 72, height: 100, fontSize: 22 },
}

export const Card: FC<CardProps> = ({ card, size = 'md' }) => {
  const dim = SIZES[size]

  if (!card) {
    return (
      <motion.div
        initial={{ rotateY: 180, opacity: 0 }}
        animate={{ rotateY: 0, opacity: 1 }}
        transition={{ duration: 0.4, ease: 'easeOut' }}
        style={{
          width: dim.width,
          height: dim.height,
          borderRadius: 8,
          background: 'linear-gradient(135deg, #2563eb 0%, #1d4ed8 100%)',
          border: '2px solid #3b82f6',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          boxShadow: '0 2px 8px rgba(0,0,0,0.3)',
        }}
      >
        <span style={{ fontSize: dim.fontSize, opacity: 0.5 }}>🂠</span>
      </motion.div>
    )
  }

  const rank = card[0]
  const suit = SUIT_MAP[card[1]] ?? { symbol: '?', color: '#888' }

  return (
    <motion.div
      initial={{ rotateY: -90, opacity: 0 }}
      animate={{ rotateY: 0, opacity: 1 }}
      transition={{ duration: 0.5, ease: 'easeOut' }}
      style={{
        width: dim.width,
        height: dim.height,
        borderRadius: 8,
        background: '#fff',
        border: '1px solid #ddd',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        boxShadow: '0 2px 8px rgba(0,0,0,0.2)',
        color: suit.color,
        fontSize: dim.fontSize,
        fontWeight: 700,
        fontFamily: 'var(--font-mono)',
        gap: 0,
        lineHeight: 1.1,
      }}
    >
      <span>{rank}</span>
      <span style={{ fontSize: dim.fontSize * 0.9 }}>{suit.symbol}</span>
    </motion.div>
  )
}
