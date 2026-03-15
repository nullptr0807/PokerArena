import type { FC } from 'react'
import { motion } from 'framer-motion'

interface TurnTimerProps {
  secondsLeft: number | null
  total?: number
}

export const TurnTimer: FC<TurnTimerProps> = ({ secondsLeft, total = 30 }) => {
  if (secondsLeft === null) return null

  const pct = Math.max(0, secondsLeft / total)
  const urgent = secondsLeft <= 5
  const color = urgent ? '#ef4444' : secondsLeft <= 10 ? '#f59e0b' : '#22c55e'

  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: 10,
        padding: '6px 16px',
        background: 'rgba(0,0,0,0.6)',
        borderRadius: 12,
        backdropFilter: 'blur(4px)',
      }}
    >
      {/* Circular progress */}
      <svg width={36} height={36} viewBox="0 0 36 36">
        <circle
          cx={18} cy={18} r={15}
          fill="none"
          stroke="rgba(255,255,255,0.1)"
          strokeWidth={3}
        />
        <motion.circle
          cx={18} cy={18} r={15}
          fill="none"
          stroke={color}
          strokeWidth={3}
          strokeLinecap="round"
          strokeDasharray={94.25}
          strokeDashoffset={94.25 * (1 - pct)}
          transform="rotate(-90 18 18)"
          animate={{ strokeDashoffset: 94.25 * (1 - pct) }}
          transition={{ duration: 0.3 }}
        />
      </svg>

      <motion.span
        style={{
          fontSize: 18,
          fontWeight: 700,
          fontFamily: 'var(--font-mono)',
          color,
          minWidth: 28,
          textAlign: 'center',
        }}
        animate={urgent ? { scale: [1, 1.15, 1] } : {}}
        transition={urgent ? { duration: 0.5, repeat: Infinity } : {}}
      >
        {secondsLeft}
      </motion.span>
    </div>
  )
}
