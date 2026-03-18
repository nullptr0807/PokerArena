import { type FC, useEffect, useRef } from 'react'
import { motion } from 'framer-motion'
import type { ActionLogEntry } from '../types'

const ACTION_COLORS: Record<string, string> = {
  fold: '#6b7280',
  check: '#d1d5db',
  call: '#34d399',
  raise: '#fbbf24',
  all_in: '#f43f5e',
}

const STREET_LABELS: Record<string, string> = {
  PREFLOP: '翻前',
  FLOP: '翻牌',
  TURN: '转牌',
  RIVER: '河牌',
}

const ACTION_LABELS: Record<string, string> = {
  fold: '弃牌',
  check: '过牌',
  call: '跟注',
  raise: '加注',
  all_in: '全下',
}

interface ActionLineProps {
  actionLog: ActionLogEntry[]
}

export const ActionLine: FC<ActionLineProps> = ({ actionLog }) => {
  const scrollRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight
    }
  }, [actionLog])

  if (actionLog.length === 0) return null

  const groups: { street: string; board?: string[]; actions: ActionLogEntry[] }[] = []
  for (const entry of actionLog) {
    if (entry.type === 'street') {
      groups.push({ street: entry.street!, board: entry.board, actions: [] })
    } else if (groups.length > 0) {
      groups[groups.length - 1].actions.push(entry)
    }
  }

  return (
    <div style={{
      width: 190,
      height: '58vh',
      background: 'rgba(9, 9, 11, 0.6)',
      backdropFilter: 'blur(20px)',
      borderRadius: 16,
      border: '1px solid var(--border)',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      <div style={{
        padding: '12px 16px',
        fontSize: 12,
        fontWeight: 700,
        color: 'var(--text-secondary)',
        borderBottom: '1px solid var(--border)',
        letterSpacing: '0.04em',
        textTransform: 'uppercase',
      }}>
        行动记录
      </div>
      <div ref={scrollRef} style={{ flex: 1, overflowY: 'auto', padding: '8px 14px' }}>
        {groups.map((group, gi) => (
          <div key={gi} style={{ marginBottom: 10 }}>
            <div style={{
              fontSize: 10,
              fontWeight: 700,
              color: 'var(--text-muted)',
              textTransform: 'uppercase',
              marginBottom: 4,
              display: 'flex',
              alignItems: 'center',
              gap: 6,
              letterSpacing: '0.06em',
            }}>
              <span>{STREET_LABELS[group.street] ?? group.street}</span>
              {group.board && group.board.length > 0 && (
                <span style={{ fontSize: 9, color: 'var(--text-muted)', fontWeight: 400, opacity: 0.6 }}>
                  {group.board.join(' ')}
                </span>
              )}
            </div>
            {group.actions.map((a, ai) => (
              <motion.div
                key={ai}
                initial={{ opacity: 0, x: -8 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ duration: 0.15 }}
                style={{
                  fontSize: 11,
                  padding: '3px 0',
                  display: 'flex',
                  alignItems: 'center',
                  gap: 6,
                }}
              >
                <span style={{ color: 'var(--text-muted)', minWidth: 36, fontSize: 10 }}>
                  {a.name}
                </span>
                <span style={{
                  color: ACTION_COLORS[a.action ?? ''] ?? '#d1d5db',
                  fontWeight: 600,
                }}>
                  {ACTION_LABELS[a.action ?? ''] ?? a.action}
                </span>
                {a.amount != null && a.amount > 0 && (
                  <span style={{
                    fontSize: 10,
                    color: 'var(--text-muted)',
                    fontFamily: 'var(--font-mono)',
                  }}>
                    {a.amount}
                  </span>
                )}
              </motion.div>
            ))}
            {group.actions.length === 0 && (
              <div style={{ fontSize: 10, color: 'var(--text-muted)', opacity: 0.4 }}>···</div>
            )}
          </div>
        ))}
      </div>
    </div>
  )
}
