import { type FC, useEffect, useRef } from 'react'
import type { DebugEvent } from '../types'

const STAGE_COLORS: Record<string, string> = {
  blueprint_lookup: '#60a5fa',
  subgame_solve: '#fbbf24',
  heuristic: '#a78bfa',
}

const STAGE_LABELS: Record<string, string> = {
  blueprint_lookup: '📋 蓝图查表',
  subgame_solve: '🧮 子博弈求解',
  heuristic: '🎲 启发式',
}

function formatCards(cards: number[] | string[] | undefined): string {
  if (!cards || cards.length === 0) return '??'
  if (typeof cards[0] === 'string') return (cards as string[]).join(' ')
  const ranks = '23456789TJQKA'
  const suits = ['♠', '♥', '♦', '♣']
  return (cards as number[]).map(c => {
    const r = Math.floor(c / 4)
    const s = c % 4
    return `${ranks[r] ?? '?'}${suits[s] ?? '?'}`
  }).join(' ')
}

interface DebugPanelProps {
  events: DebugEvent[]
  visible: boolean
  onClose: () => void
}

export const DebugPanel: FC<DebugPanelProps> = ({ events, visible, onClose }) => {
  const scrollRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight
    }
  }, [events])

  if (!visible) return null

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      right: 0,
      width: 380,
      height: '100vh',
      background: 'rgba(9, 9, 11, 0.92)',
      backdropFilter: 'blur(24px)',
      borderLeft: '1px solid var(--border)',
      zIndex: 50,
      display: 'flex',
      flexDirection: 'column',
      fontFamily: 'var(--font-mono)',
      fontSize: 12,
    }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '14px 18px',
        borderBottom: '1px solid var(--border)',
      }}>
        <span style={{ fontWeight: 700, fontSize: 13, color: '#fbbf24' }}>
          Debug
        </span>
        <button
          onClick={onClose}
          style={{
            background: 'var(--glass)',
            border: '1px solid var(--border)',
            color: 'var(--text-muted)',
            cursor: 'pointer',
            fontSize: 12,
            borderRadius: 6,
            width: 24,
            height: 24,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
          }}
        >
          ✕
        </button>
      </div>

      <div ref={scrollRef} style={{ flex: 1, overflowY: 'auto', padding: '10px 14px' }}>
        {events.length === 0 && (
          <div style={{ color: 'var(--text-muted)', textAlign: 'center', marginTop: 40, fontSize: 12 }}>
            等待 AI 决策...
          </div>
        )}
        {events.map((evt, i) => (
          <DebugEventRow key={i} event={evt} />
        ))}
      </div>
    </div>
  )
}

const DebugEventRow: FC<{ event: DebugEvent }> = ({ event }) => {
  if (event.type === 'hand_start') {
    return (
      <div style={{
        marginBottom: 10,
        padding: '10px 12px',
        background: 'var(--glass)',
        borderRadius: 10,
        border: '1px solid var(--border)',
      }}>
        <div style={{ color: '#4ade80', fontWeight: 700, marginBottom: 4, fontSize: 12 }}>
          Hand #{event.hand_number} · Button: 座位{event.button}
        </div>
        {event.players?.map((p) => (
          <div key={p.index} style={{ color: 'var(--text-secondary)', marginLeft: 8, fontSize: 11 }}>
            {p.name}: {formatCards(p.hole_cards)} · {p.chips} · [{p.difficulty}]
          </div>
        ))}
      </div>
    )
  }

  if (event.type === 'ai_start') {
    return (
      <div style={{
        marginTop: 8,
        marginBottom: 2,
        color: 'var(--text-muted)',
        borderTop: '1px solid var(--border)',
        paddingTop: 6,
        fontSize: 11,
      }}>
        ▶ {event.name} · {event.street} · Pot: {event.pot}
      </div>
    )
  }

  if (event.type === 'ai_stage') {
    const color = STAGE_COLORS[event.stage ?? ''] ?? '#666'
    const label = STAGE_LABELS[event.stage ?? ''] ?? event.stage
    return (
      <div style={{ color, marginLeft: 12, marginBottom: 2, fontSize: 11 }}>
        {label}
        {event.action_history && event.action_history.length > 0 && (
          <span style={{ color: 'var(--text-muted)', marginLeft: 8 }}>
            {event.action_history.join(' → ')}
          </span>
        )}
      </div>
    )
  }

  if (event.type === 'ai_decision') {
    const color = STAGE_COLORS[event.stage ?? ''] ?? '#666'
    return (
      <div style={{
        marginLeft: 12,
        marginBottom: 6,
        padding: '6px 10px',
        background: 'var(--glass)',
        borderRadius: 8,
        borderLeft: `2px solid ${color}`,
      }}>
        <div style={{ color: 'var(--text-primary)', fontWeight: 600, fontSize: 12 }}>
          {event.name}: <span style={{ color: '#fbbf24' }}>{event.action}</span>
          {event.amount ? ` (${event.amount})` : ''}
        </div>
        <div style={{ color: 'var(--text-muted)', marginTop: 2, fontSize: 10 }}>
          {formatCards(event.hole_cards)} · {event.compute_ms}ms ({event.total_ms}ms)
        </div>
      </div>
    )
  }

  return (
    <div style={{ color: 'var(--text-muted)', marginBottom: 2, fontSize: 10 }}>
      {JSON.stringify(event)}
    </div>
  )
}
