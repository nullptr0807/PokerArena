import { type FC, useEffect } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import type { ActionLogEntry, GameState } from '../types'
import { Card } from './Card'
import { PlayerSeat } from './PlayerSeat'
import { TurnTimer } from './TurnTimer'
import { ActionPanel } from './ActionPanel'
import { ActionLine } from './ActionLine'

// Position players around an elliptical table
const SEAT_POSITIONS: Record<number, { top: string; left: string }[]> = {
  2: [
    { top: '75%', left: '50%' },   // player 0 (human) — bottom
    { top: '10%', left: '50%' },   // player 1 — top
  ],
  3: [
    { top: '75%', left: '50%' },
    { top: '15%', left: '25%' },
    { top: '15%', left: '75%' },
  ],
  4: [
    { top: '75%', left: '50%' },
    { top: '40%', left: '10%' },
    { top: '10%', left: '50%' },
    { top: '40%', left: '90%' },
  ],
  5: [
    { top: '75%', left: '50%' },
    { top: '55%', left: '8%' },
    { top: '10%', left: '25%' },
    { top: '10%', left: '75%' },
    { top: '55%', left: '92%' },
  ],
  6: [
    { top: '75%', left: '50%' },
    { top: '55%', left: '8%' },
    { top: '10%', left: '20%' },
    { top: '10%', left: '50%' },
    { top: '10%', left: '80%' },
    { top: '55%', left: '92%' },
  ],
}

export interface TableProps {
  state: GameState
  onAction: (action: string, amount?: number) => void
  onNewHand: () => void
  onQuit: () => void
  turnSecondsLeft: number | null
  aiThinking: Record<number, string | null>
  actionLog: ActionLogEntry[]
}

export const Table: FC<TableProps> = ({ state, onAction, onNewHand, onQuit, turnSecondsLeft, aiThinking, actionLog }) => {
  const positions = SEAT_POSITIONS[state.players.length] ?? SEAT_POSITIONS[2]
  const isWaiting = state.street === 'WAITING'

  // Auto-start next hand after result or when waiting
  useEffect(() => {
    if (state.result || (isWaiting && !state.result)) {
      const delay = state.result ? 2500 : 500
      const timer = setTimeout(onNewHand, delay)
      return () => clearTimeout(timer)
    }
  }, [state.result, isWaiting, onNewHand])
  const isHumanTurn =
    !isWaiting &&
    state.street !== 'SHOWDOWN' &&
    state.players[state.current_player]?.is_human

  return (
    <div
      style={{
        position: 'relative',
        width: '100%',
        height: '100%',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
      }}
    >
      {/* Header */}
      <div
        style={{
          position: 'absolute',
          top: 16,
          left: 24,
          display: 'flex',
          alignItems: 'center',
          gap: 16,
        }}
      >
        <span style={{ fontSize: 20, fontWeight: 700 }}>Poker Arena</span>
        <span style={{ fontSize: 13, color: 'var(--text-muted)' }}>
          Hand #{state.hand_number} · {state.street}
        </span>
        <button
          onClick={onQuit}
          style={{
            marginLeft: 'auto',
            padding: '4px 14px',
            borderRadius: 8,
            background: 'rgba(255,255,255,0.08)',
            color: 'var(--text-muted)',
            fontSize: 13,
            fontWeight: 500,
            border: '1px solid rgba(255,255,255,0.1)',
            cursor: 'pointer',
          }}
        >
          ✕ 退出
        </button>
      </div>

      {/* Main content area with table + action line */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 16, width: '100%', justifyContent: 'center' }}>
        {/* Felt table (ellipse) */}
        <div
          style={{
            position: 'relative',
            width: '70%',
            maxWidth: 800,
            height: '60vh',
            borderRadius: '50%',
            background: `radial-gradient(ellipse, var(--bg-table-felt) 0%, var(--bg-table) 100%)`,
            border: '8px solid #0f4c22',
            boxShadow: 'var(--shadow-lg)',
          }}
        >
        {/* Community cards */}
        <div
          style={{
            position: 'absolute',
            top: '42%',
            left: '50%',
            transform: 'translate(-50%, -50%)',
            display: 'flex',
            gap: 8,
            alignItems: 'center',
          }}
        >
          {state.board.map((c, i) => (
            <motion.div
              key={`${c}-${i}`}
              initial={{ y: -40, opacity: 0, scale: 0.8 }}
              animate={{ y: 0, opacity: 1, scale: 1 }}
              transition={{ duration: 0.4, delay: i * 0.1, ease: 'easeOut' }}
            >
              <Card card={c} size="md" />
            </motion.div>
          ))}
        </div>

        {/* Pot */}
        {state.pot > 0 && (
          <div
            style={{
              position: 'absolute',
              top: '56%',
              left: '50%',
              transform: 'translate(-50%, 0)',
              background: 'rgba(0,0,0,0.5)',
              borderRadius: 20,
              padding: '4px 16px',
              fontSize: 16,
              fontWeight: 700,
              fontFamily: 'var(--font-mono)',
              color: '#fbbf24',
            }}
          >
            Pot: {state.pot}
          </div>
        )}

        {/* Player seats */}
        {state.players.map((p, i) => (
          <div
            key={i}
            style={{
              position: 'absolute',
              top: positions[i].top,
              left: positions[i].left,
              transform: 'translate(-50%, -50%)',
            }}
          >
            <PlayerSeat
              player={p}
              isCurrentPlayer={state.current_player === i && !isWaiting}
              isButton={state.button === i}
              thinkingStage={aiThinking[i]}
            />
          </div>
        ))}
        </div>

        {/* Action Line sidebar */}
        <ActionLine actionLog={actionLog} />
      </div>

      {/* Result display */}
      <AnimatePresence>
        {state.result && (
          <motion.div
            initial={{ opacity: 0, scale: 0.9 }}
            animate={{ opacity: 1, scale: 1 }}
            exit={{ opacity: 0, scale: 0.9 }}
            transition={{ duration: 0.3 }}
            style={{
              position: 'absolute',
              top: '50%',
              left: '50%',
              transform: 'translate(-50%, -50%)',
              background: 'rgba(0,0,0,0.85)',
              borderRadius: 16,
              padding: '20px 32px',
              textAlign: 'center',
              zIndex: 10,
              backdropFilter: 'blur(8px)',
            }}
          >
            {state.result.winners.map((w, i) => (
              <div key={i} style={{ fontSize: 18, fontWeight: 600, marginBottom: 4 }}>
                {state.players[w.player]?.name} wins {w.amount}
                {w.hand_rank ? ` — ${w.hand_rank}` : ''}
              </div>
            ))}
            <button
              onClick={onNewHand}
              style={{
                marginTop: 12,
                padding: '8px 24px',
                borderRadius: 10,
                background: 'var(--accent)',
                color: '#fff',
                fontSize: 15,
                fontWeight: 600,
              }}
            >
              Next Hand
            </button>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Action panel + timer */}
      {isHumanTurn && (
        <div style={{ position: 'absolute', bottom: 24, zIndex: 5, display: 'flex', alignItems: 'center', gap: 16 }}>
          <TurnTimer secondsLeft={turnSecondsLeft} />
          <ActionPanel
            validActions={state.valid_actions}
            currentBet={state.current_bet}
            myBet={state.players[0]?.bet_this_street ?? 0}
            myChips={state.players[0]?.chips ?? 0}
            minRaiseTo={state.min_raise_to ?? (state.current_bet + 2)}
            pot={state.pot}
            onAction={onAction}
          />
        </div>
      )}

      {/* Waiting state — start hand button */}
      {isWaiting && !state.result && (
        <div style={{ position: 'absolute', bottom: 24 }}>
          <button
            onClick={onNewHand}
            style={{
              padding: '12px 32px',
              borderRadius: 12,
              background: 'var(--accent)',
              color: '#fff',
              fontSize: 16,
              fontWeight: 600,
              boxShadow: 'var(--shadow)',
            }}
          >
            Deal
          </button>
        </div>
      )}
    </div>
  )
}
