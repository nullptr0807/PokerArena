import { type FC, useState, useEffect } from 'react'
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

const POT_PRESETS = [
  { label: '¼', mult: 0.25 },
  { label: '⅓', mult: 0.33 },
  { label: '½', mult: 0.5 },
  { label: '¾', mult: 0.75 },
  { label: '1x', mult: 1.0 },
  { label: '1.2x', mult: 1.2 },
  { label: '1.5x', mult: 1.5 },
  { label: '2x', mult: 2.0 },
]

const actionBtn = (bg: string, color = '#fff'): React.CSSProperties => ({
  padding: '10px 22px',
  borderRadius: 12,
  fontSize: 14,
  fontWeight: 700,
  color,
  background: bg,
  border: 'none',
  minWidth: 76,
  letterSpacing: '-0.01em',
  backdropFilter: 'blur(8px)',
  boxShadow: '0 2px 8px rgba(0,0,0,0.2)',
})

const presetBtn = (active: boolean): React.CSSProperties => ({
  padding: '5px 10px',
  borderRadius: 8,
  fontSize: 12,
  fontWeight: 600,
  color: active ? '#000' : 'rgba(255,255,255,0.7)',
  background: active ? 'rgba(52,211,153,0.9)' : 'rgba(255,255,255,0.06)',
  border: active ? 'none' : '1px solid rgba(255,255,255,0.08)',
  minWidth: 38,
  letterSpacing: '-0.01em',
  transition: 'all 0.15s ease',
})

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
  const [activePreset, setActivePreset] = useState<string | null>(null)

  // Reset raise amount when minRaise changes
  useEffect(() => { setRaiseAmount(minRaise); setActivePreset(null) }, [minRaise])

  const clampRaise = (n: number) => Math.max(minRaise, Math.min(maxRaise, Math.round(n)))

  // Simple pot sizing: ½ = pot×0.5, 1x = pot, 2x = pot×2
  // Raise-to = currentBet + pot × multiplier
  const potSizeRaise = (mult: number) => Math.round(currentBet + pot * mult)

  const selectPreset = (label: string, mult: number) => {
    const raw = potSizeRaise(mult)
    const amt = Math.max(minRaise, Math.min(maxRaise, raw))
    setRaiseAmount(amt)
    setActivePreset(label)
  }

  const canRaise = validActions.includes('raise')

  return (
    <motion.div
      initial={{ y: 24, opacity: 0 }}
      animate={{ y: 0, opacity: 1 }}
      transition={{ duration: 0.3, ease: 'easeOut' }}
      style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: 10,
        padding: '14px 20px',
        background: 'rgba(18, 18, 22, 0.85)',
        backdropFilter: 'blur(24px)',
        borderRadius: 18,
        border: '1px solid var(--border)',
        boxShadow: '0 8px 32px rgba(0,0,0,0.4)',
      }}
    >
      {/* Pot sizing presets — only when raise is available */}
      {canRaise && (
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: 4,
          flexWrap: 'wrap',
          justifyContent: 'center',
        }}>
          {POT_PRESETS.map(({ label, mult }) => {
            const raw = potSizeRaise(mult)
            const belowMin = raw < minRaise
            const aboveMax = raw > maxRaise
            // Hide if above max (use ALL IN instead)
            if (aboveMax) return null
            return (
              <motion.button
                key={label}
                whileHover={{ scale: 1.06 }}
                whileTap={{ scale: 0.94 }}
                onClick={() => selectPreset(label, mult)}
                title={belowMin ? `最低加注 ${minRaise}，已自动调整` : `Raise to ${raw}`}
                style={{
                  ...presetBtn(activePreset === label),
                  opacity: belowMin ? 0.4 : 1,
                  textDecoration: belowMin ? 'line-through' : 'none',
                }}
              >
                {label}
              </motion.button>
            )
          })}
          <motion.button
            whileHover={{ scale: 1.06 }}
            whileTap={{ scale: 0.94 }}
            onClick={() => { setRaiseAmount(maxRaise); setActivePreset('ALL') }}
            style={{
              ...presetBtn(activePreset === 'ALL'),
              background: activePreset === 'ALL'
                ? 'linear-gradient(135deg, #fbbf24, #f59e0b)'
                : 'rgba(251,191,36,0.1)',
              color: activePreset === 'ALL' ? '#000' : '#fbbf24',
              border: activePreset === 'ALL' ? 'none' : '1px solid rgba(251,191,36,0.2)',
              fontWeight: 700,
            }}
          >
            ALL IN
          </motion.button>
        </div>
      )}

      {/* Main action row */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        gap: 10,
      }}>
        {validActions.includes('fold') && (
          <motion.button
            whileHover={{ scale: 1.04 }}
            whileTap={{ scale: 0.95 }}
            onClick={() => onAction('fold')}
            style={actionBtn('rgba(244,63,94,0.85)')}
          >
            弃牌
          </motion.button>
        )}

        {validActions.includes('check') && (
          <motion.button
            whileHover={{ scale: 1.04 }}
            whileTap={{ scale: 0.95 }}
            onClick={() => onAction('check')}
            style={actionBtn('rgba(75, 85, 99, 0.8)')}
          >
            过牌
          </motion.button>
        )}

        {validActions.includes('call') && (
          <motion.button
            whileHover={{ scale: 1.04 }}
            whileTap={{ scale: 0.95 }}
            onClick={() => onAction('call')}
            style={actionBtn('rgba(37, 99, 235, 0.85)')}
          >
            跟注 {toCall}
          </motion.button>
        )}

        {canRaise && (
          <>
            {/* Slider */}
            <div style={{
              position: 'relative',
              width: 120,
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              gap: 2,
            }}>
              <span style={{
                fontSize: 11,
                color: raiseAmount === minRaise && activePreset ? 'rgba(251,191,36,0.8)' : 'rgba(255,255,255,0.4)',
                fontFamily: 'var(--font-mono)',
              }}>
                {raiseAmount === minRaise && activePreset
                  ? `最低 ${minRaise}`
                  : raiseAmount}
              </span>
              <input
                type="range"
                min={minRaise}
                max={maxRaise}
                value={raiseAmount}
                onChange={(e) => { setRaiseAmount(Number(e.target.value)); setActivePreset(null) }}
                style={{
                  width: '100%',
                  appearance: 'none',
                  WebkitAppearance: 'none',
                  background: `linear-gradient(to right, rgba(52,211,153,0.6) ${((raiseAmount - minRaise) / (maxRaise - minRaise)) * 100}%, rgba(255,255,255,0.08) 0%)`,
                  height: 4,
                  borderRadius: 2,
                  cursor: 'pointer',
                  outline: 'none',
                }}
              />
            </div>

            {/* Raise button */}
            <motion.button
              whileHover={{ scale: 1.04 }}
              whileTap={{ scale: 0.95 }}
              onClick={() => onAction('raise', raiseAmount)}
              style={actionBtn('rgba(52, 211, 153, 0.85)', '#000')}
            >
              加注 {raiseAmount}
            </motion.button>
          </>
        )}

        {validActions.includes('all_in') && !canRaise && (
          <motion.button
            whileHover={{ scale: 1.04 }}
            whileTap={{ scale: 0.95 }}
            onClick={() => onAction('all_in')}
            style={actionBtn('linear-gradient(135deg, #fbbf24, #f59e0b)', '#000')}
          >
            {!validActions.includes('call') && !validActions.includes('check')
              ? `跟注全下 ${myChips}`
              : `全下 ${myChips}`}
          </motion.button>
        )}
      </div>
    </motion.div>
  )
}
