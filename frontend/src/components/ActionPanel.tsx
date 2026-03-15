import { type FC, useState } from 'react'
import { motion } from 'framer-motion'

interface ActionPanelProps {
  validActions: string[]
  currentBet: number
  myBet: number
  myChips: number
  minRaiseTo: number
  pot: number
  onAction: (action: string, amount?: number) => void
}

export const ActionPanel: FC<ActionPanelProps> = ({
  validActions,
  currentBet,
  myBet,
  myChips,
  minRaiseTo,
  pot,
  onAction,
}) => {
  const toCall = currentBet - myBet
  const minRaise = minRaiseTo
  const maxRaise = myChips + myBet
  const [raiseAmount, setRaiseAmount] = useState(minRaise)

  const btnStyle = (color: string, bg: string): React.CSSProperties => ({
    padding: '10px 24px',
    borderRadius: 10,
    fontSize: 15,
    fontWeight: 600,
    color,
    background: bg,
    border: 'none',
    minWidth: 80,
  })

  return (
    <motion.div
      initial={{ y: 30, opacity: 0 }}
      animate={{ y: 0, opacity: 1 }}
      transition={{ duration: 0.3, ease: 'easeOut' }}
      style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        gap: 12,
        padding: '16px 24px',
        background: 'var(--bg-secondary)',
        borderRadius: 16,
        boxShadow: 'var(--shadow)',
      }}
    >
      {validActions.includes('fold') && (
        <button
          onClick={() => onAction('fold')}
          style={btnStyle('#fff', 'var(--danger)')}
        >
          Fold
        </button>
      )}

      {validActions.includes('check') && (
        <button
          onClick={() => onAction('check')}
          style={btnStyle('#fff', '#374151')}
        >
          Check
        </button>
      )}

      {validActions.includes('call') && (
        <button
          onClick={() => onAction('call')}
          style={btnStyle('#fff', '#2563eb')}
        >
          Call {toCall}
        </button>
      )}

      {validActions.includes('raise') && (
        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
          <input
            type="range"
            min={minRaise}
            max={maxRaise}
            value={raiseAmount}
            onChange={(e) => setRaiseAmount(Number(e.target.value))}
            style={{ width: 120, accentColor: 'var(--accent)' }}
          />
          <button
            onClick={() => onAction('raise', raiseAmount)}
            style={btnStyle('#fff', 'var(--accent)')}
          >
            Raise {raiseAmount}
          </button>
          {/* Quick raise buttons */}
          <button
            onClick={() => {
              const amt = Math.min(Math.round(pot * 0.5) + currentBet, maxRaise)
              onAction('raise', amt)
            }}
            style={btnStyle('var(--text-primary)', 'var(--bg-card)')}
          >
            ½ Pot
          </button>
          <button
            onClick={() => {
              const amt = Math.min(pot + currentBet, maxRaise)
              onAction('raise', amt)
            }}
            style={btnStyle('var(--text-primary)', 'var(--bg-card)')}
          >
            Pot
          </button>
        </div>
      )}

      {validActions.includes('all_in') && (
        <button
          onClick={() => onAction('all_in')}
          style={btnStyle('#000', '#f59e0b')}
        >
          {!validActions.includes('call') && !validActions.includes('raise') && !validActions.includes('check')
            ? `Call All-in ${myChips}`
            : `All In ${myChips}`}
        </button>
      )}
    </motion.div>
  )
}
