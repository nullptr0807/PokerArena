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
  const color = urgent ? '#f43f5e' : secondsLeft <= 10 ? '#fbbf24' : '#34d399'

  return (
    <div style={{
      display: 'flex',
      alignItems: 'center',
      gap: 8,
      padding: '8px 16px',
      background: 'rgba(9,9,11,0.7)',
      backdropFilter: 'blur(16px)',
      borderRadius: 14,
      border: '1px solid var(--border)',
    }}>
      <svg width={32} height={32} viewBox="0 0 36 36">
        <circle
          cx={18} cy={18} r={14}
          fill="none"
          stroke="rgba(255,255,255,0.06)"
          strokeWidth={2.5}
        />
        <motion.circle
          cx={18} cy={18} r={14}
          fill="none"
          stroke={color}
          strokeWidth={2.5}
          strokeLinecap="round"
          strokeDasharray={87.96}
          strokeDashoffset={87.96 * (1 - pct)}
          transform="rotate(-90 18 18)"
          animate={{ strokeDashoffset: 87.96 * (1 - pct) }}
          transition={{ duration: 0.3 }}
        />
      </svg>

      <motion.span
        style={{
          fontSize: 16,
          fontWeight: 700,
          fontFamily: 'var(--font-mono)',
          color,
          minWidth: 24,
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
