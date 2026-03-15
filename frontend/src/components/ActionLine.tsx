import { type FC, useEffect, useRef } from 'react'
import { motion } from 'framer-motion'
import type { ActionLogEntry } from '../types'

const ACTION_COLORS: Record<string, string> = {
  fold: '#6b7280',
  check: '#e5e7eb',
  call: '#10b981',
  raise: '#f59e0b',
  all_in: '#ef4444',
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

  // Group by street
  const groups: { street: string; board?: string[]; actions: ActionLogEntry[] }[] = []
  for (const entry of actionLog) {
    if (entry.type === 'street') {
      groups.push({ street: entry.street!, board: entry.board, actions: [] })
    } else if (groups.length > 0) {
      groups[groups.length - 1].actions.push(entry)
    }
  }

  return (
    <div
      style={{
        width: 200,
        height: '60vh',
        background: 'rgba(0, 0, 0, 0.6)',
        borderRadius: 12,
        border: '1px solid rgba(255, 255, 255, 0.1)',
        backdropFilter: 'blur(8px)',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}
    >
      <div
        style={{
          padding: '10px 14px',
          fontSize: 13,
          fontWeight: 700,
          color: 'var(--text-primary)',
          borderBottom: '1px solid rgba(255, 255, 255, 0.08)',
        }}
      >
        📋 行动记录
      </div>
      <div
        ref={scrollRef}
        style={{
          flex: 1,
          overflowY: 'auto',
          padding: '8px 12px',
        }}
      >
        {groups.map((group, gi) => (
          <div key={gi} style={{ marginBottom: 8 }}>
            {/* Street header */}
            <div
              style={{
                fontSize: 11,
                fontWeight: 700,
                color: '#94a3b8',
                textTransform: 'uppercase',
                marginBottom: 4,
                display: 'flex',
                alignItems: 'center',
                gap: 6,
              }}
            >
              <span>{STREET_LABELS[group.street] ?? group.street}</span>
              {group.board && group.board.length > 0 && (
                <span style={{ fontSize: 10, color: '#64748b', fontWeight: 400 }}>
                  [{group.board.join(' ')}]
                </span>
              )}
            </div>
            {/* Actions */}
            {group.actions.map((a, ai) => (
              <motion.div
                key={ai}
                initial={{ opacity: 0, x: -10 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ duration: 0.2 }}
                style={{
                  fontSize: 12,
                  padding: '2px 0',
                  display: 'flex',
                  alignItems: 'center',
                  gap: 6,
                }}
              >
                <span style={{ color: 'var(--text-muted)', minWidth: 40 }}>
                  {a.name}
                </span>
                <span
                  style={{
                    color: ACTION_COLORS[a.action ?? ''] ?? '#e5e7eb',
                    fontWeight: 600,
                  }}
                >
                  {ACTION_LABELS[a.action ?? ''] ?? a.action}
                </span>
                {a.amount != null && a.amount > 0 && (
                  <span
                    style={{
                      fontSize: 11,
                      color: '#94a3b8',
                      fontFamily: 'var(--font-mono)',
                    }}
                  >
                    {a.amount}
                  </span>
                )}
              </motion.div>
            ))}
            {group.actions.length === 0 && (
              <div style={{ fontSize: 11, color: '#475569', fontStyle: 'italic' }}>
                ...
              </div>
            )}
          </div>
        ))}
      </div>
    </div>
  )
}
