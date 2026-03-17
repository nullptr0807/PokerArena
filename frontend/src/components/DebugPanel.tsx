import { type FC, useEffect, useRef } from 'react'
import type { DebugEvent } from '../types'

const STAGE_COLORS: Record<string, string> = {
  blueprint_lookup: '#60a5fa',  // blue
  subgame_solve: '#f59e0b',     // amber
  heuristic: '#a78bfa',         // purple
}

const STAGE_LABELS: Record<string, string> = {
  blueprint_lookup: '📋 蓝图查表',
  subgame_solve: '🧮 子博弈求解',
  heuristic: '🎲 启发式',
}

function formatCards(cards: number[] | string[] | undefined): string {
  if (!cards || cards.length === 0) return '??'
  // If already strings, return as-is
  if (typeof cards[0] === 'string') return (cards as string[]).join(' ')
  // Convert card ints to readable format
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
    <div
      style={{
        position: 'fixed',
        top: 0,
        right: 0,
        width: 380,
        height: '100vh',
        background: 'rgba(0, 0, 0, 0.92)',
        borderLeft: '1px solid rgba(255,255,255,0.1)',
        zIndex: 50,
        display: 'flex',
        flexDirection: 'column',
        fontFamily: 'ui-monospace, "SF Mono", monospace',
        fontSize: 12,
      }}
    >
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '12px 16px',
          borderBottom: '1px solid rgba(255,255,255,0.1)',
        }}
      >
        <span style={{ fontWeight: 700, fontSize: 14, color: '#f59e0b' }}>
          🐛 Debug Mode
        </span>
        <button
          onClick={onClose}
          style={{
            background: 'none',
            border: 'none',
            color: 'rgba(255,255,255,0.5)',
            cursor: 'pointer',
            fontSize: 16,
          }}
        >
          ✕
        </button>
      </div>

      {/* Events */}
      <div
        ref={scrollRef}
        style={{
          flex: 1,
          overflowY: 'auto',
          padding: '8px 12px',
        }}
      >
        {events.length === 0 && (
          <div style={{ color: 'rgba(255,255,255,0.3)', textAlign: 'center', marginTop: 32 }}>
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
      <div style={{ marginBottom: 12, padding: '8px 10px', background: 'rgba(255,255,255,0.05)', borderRadius: 8 }}>
        <div style={{ color: '#4ade80', fontWeight: 700, marginBottom: 4 }}>
          🃏 Hand #{event.hand_number} · Button: 座位{event.button}
        </div>
        {event.players?.map((p) => (
          <div key={p.index} style={{ color: 'rgba(255,255,255,0.7)', marginLeft: 8 }}>
            {p.name}: {formatCards(p.hole_cards)} · {p.chips} chips · [{p.difficulty}]
          </div>
        ))}
      </div>
    )
  }

  if (event.type === 'ai_start') {
    return (
      <div style={{ marginTop: 8, marginBottom: 2, color: 'rgba(255,255,255,0.5)', borderTop: '1px solid rgba(255,255,255,0.05)', paddingTop: 6 }}>
        ▶ {event.name} 开始思考 · {event.street} · Pot: {event.pot}
      </div>
    )
  }

  if (event.type === 'ai_stage') {
    const color = STAGE_COLORS[event.stage ?? ''] ?? '#999'
    const label = STAGE_LABELS[event.stage ?? ''] ?? event.stage
    return (
      <div style={{ color, marginLeft: 12, marginBottom: 2 }}>
        {label}
        {event.action_history && event.action_history.length > 0 && (
          <span style={{ color: 'rgba(255,255,255,0.4)', marginLeft: 8 }}>
            行动线: {event.action_history.join(' → ')}
          </span>
        )}
      </div>
    )
  }

  if (event.type === 'ai_decision') {
    const color = STAGE_COLORS[event.stage ?? ''] ?? '#999'
    return (
      <div style={{
        marginLeft: 12,
        marginBottom: 6,
        padding: '6px 8px',
        background: 'rgba(255,255,255,0.04)',
        borderRadius: 6,
        borderLeft: `3px solid ${color}`,
      }}>
        <div style={{ color: '#e2e8f0', fontWeight: 600 }}>
          {event.name}: <span style={{ color: '#fbbf24' }}>{event.action}</span>
          {event.amount ? ` (${event.amount})` : ''}
        </div>
        <div style={{ color: 'rgba(255,255,255,0.45)', marginTop: 2 }}>
          手牌: {formatCards(event.hole_cards)} · 
          耗时: {event.compute_ms}ms (总 {event.total_ms}ms)
        </div>
      </div>
    )
  }

  // Fallback for unknown events
  return (
    <div style={{ color: 'rgba(255,255,255,0.3)', marginBottom: 2, fontSize: 11 }}>
      {JSON.stringify(event)}
    </div>
  )
}
