import { type FC, useEffect, useState, useRef, useCallback } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import type { ActionLogEntry, GameState } from '../types'
import { Card } from './Card'
import { PlayerSeat } from './PlayerSeat'

/** Minimal Markdown → HTML for analysis display */
function renderMarkdown(md: string): string {
  return md
    // Tables: convert | row | row | to <table>
    .replace(/^(\|.+\|)\n(\|[-| :]+\|)\n((?:\|.+\|\n?)*)/gm, (_m, header: string, _sep: string, body: string) => {
      const thCells = header.split('|').filter(Boolean).map((c: string) => `<th style="padding:6px 10px;text-align:left;border-bottom:1px solid rgba(255,255,255,0.1);font-size:12px;color:rgba(255,255,255,0.5)">${c.trim()}</th>`).join('')
      const rows = body.trim().split('\n').map((row: string) => {
        const cells = row.split('|').filter(Boolean).map((c: string) => `<td style="padding:5px 10px;font-size:12px">${c.trim()}</td>`).join('')
        return `<tr style="border-bottom:1px solid rgba(255,255,255,0.04)">${cells}</tr>`
      }).join('')
      return `<table style="width:100%;border-collapse:collapse;margin:12px 0"><thead><tr>${thCells}</tr></thead><tbody>${rows}</tbody></table>`
    })
    // Headers
    .replace(/^### (.+)$/gm, '<h4 style="color:#e2e8f0;font-size:14px;font-weight:700;margin:16px 0 6px">$1</h4>')
    .replace(/^## (.+)$/gm, '<h3 style="color:#f1f5f9;font-size:15px;font-weight:700;margin:20px 0 8px">$1</h3>')
    // Bold
    .replace(/\*\*(.+?)\*\*/g, '<strong style="color:#e2e8f0">$1</strong>')
    // Horizontal rules
    .replace(/^---$/gm, '<hr style="border:none;border-top:1px solid rgba(255,255,255,0.08);margin:14px 0">')
    // List items
    .replace(/^- (.+)$/gm, '<div style="padding:2px 0 2px 16px;position:relative"><span style="position:absolute;left:4px;color:rgba(255,255,255,0.3)">•</span>$1</div>')
    // Line breaks
    .replace(/\n\n/g, '<div style="height:8px"></div>')
    .replace(/\n/g, '<br>')
}
import { TurnTimer } from './TurnTimer'
import { ActionPanel } from './ActionPanel'
import { ActionLine } from './ActionLine'

const SEAT_POSITIONS: Record<number, { top: string; left: string }[]> = {
  2: [
    { top: '78%', left: '50%' },
    { top: '8%', left: '50%' },
  ],
  3: [
    { top: '78%', left: '50%' },
    { top: '12%', left: '25%' },
    { top: '12%', left: '75%' },
  ],
  4: [
    { top: '78%', left: '50%' },
    { top: '42%', left: '8%' },
    { top: '8%', left: '50%' },
    { top: '42%', left: '92%' },
  ],
  5: [
    { top: '78%', left: '50%' },
    { top: '58%', left: '6%' },
    { top: '8%', left: '25%' },
    { top: '8%', left: '75%' },
    { top: '58%', left: '94%' },
  ],
  6: [
    { top: '78%', left: '50%' },
    { top: '58%', left: '6%' },
    { top: '8%', left: '20%' },
    { top: '8%', left: '50%' },
    { top: '8%', left: '80%' },
    { top: '58%', left: '94%' },
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
  const [paused, setPaused] = useState(false)
  const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null)
  const [pendingResult, setPendingResult] = useState<typeof state.result | null>(null)
  const [analysisText, setAnalysisText] = useState<string | null>(null)
  const [analysisLoading, setAnalysisLoading] = useState(false)

  // Single effect: capture result AND handle auto-advance
  useEffect(() => {
    if (timerRef.current) { clearTimeout(timerRef.current); timerRef.current = null }

    // If state has a result, capture it and start 5s countdown
    if (state.result) {
      setPendingResult(state.result)
      if (!paused) {
        timerRef.current = setTimeout(() => {
          setPendingResult(null)
          onNewHand()
        }, 5000)
      }
      return () => { if (timerRef.current) clearTimeout(timerRef.current) }
    }

    // Waiting with no result — but only advance if we're not showing a pending result
    if (isWaiting && !pendingResult && !paused) {
      timerRef.current = setTimeout(onNewHand, 800)
      return () => { if (timerRef.current) clearTimeout(timerRef.current) }
    }
  }, [state.result, isWaiting, pendingResult, paused, onNewHand])

  // Handle pause toggle — restart timer if unpausing with pending result
  useEffect(() => {
    if (!paused && pendingResult && !state.result) {
      timerRef.current = setTimeout(() => {
        setPendingResult(null)
        onNewHand()
      }, 5000)
      return () => { if (timerRef.current) clearTimeout(timerRef.current) }
    }
  }, [paused])

  const skipResult = useCallback(() => {
    if (timerRef.current) clearTimeout(timerRef.current)
    setPendingResult(null)
    setAnalysisText(null)
    setAnalysisLoading(false)
    onNewHand()
  }, [onNewHand])

  const requestAnalysis = useCallback(async () => {
    // Pause timer
    setPaused(true)
    setAnalysisLoading(true)
    if (timerRef.current) { clearTimeout(timerRef.current); timerRef.current = null }

    try {
      const body = {
        players: state.players.map(p => ({
          name: p.name,
          is_human: p.is_human,
          hole_cards: p.hole_cards,
          chips: p.chips,
        })),
        board: state.board,
        action_log: actionLog,
        result: pendingResult,
        pot: state.pot,
        blinds: { small: 1, big: 2 },
      }
      const res = await fetch('/api/analyze-hand', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      })
      const data = await res.json()
      setAnalysisText(data.analysis || data.error || '分析失败')
    } catch (e) {
      setAnalysisText('请求分析失败，请重试')
    } finally {
      setAnalysisLoading(false)
    }
  }, [state, actionLog, pendingResult])

  const isHumanTurn =
    !isWaiting &&
    state.street !== 'SHOWDOWN' &&
    state.players[state.current_player]?.is_human

  const STREET_LABELS: Record<string, string> = {
    PREFLOP: '翻前', FLOP: '翻牌', TURN: '转牌', RIVER: '河牌',
    SHOWDOWN: '摊牌', WAITING: '等待中',
  }

  return (
    <div style={{
      position: 'relative',
      width: '100%',
      height: '100%',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
    }}>
      {/* Header bar — glass */}
      <div style={{
        position: 'absolute',
        top: 0,
        left: 0,
        right: 0,
        height: 56,
        display: 'flex',
        alignItems: 'center',
        padding: '0 24px',
        gap: 16,
        background: 'rgba(9,9,11,0.7)',
        backdropFilter: 'blur(20px)',
        borderBottom: '1px solid var(--border)',
        zIndex: 5,
      }}>
        <span style={{ fontSize: 18, fontWeight: 700, letterSpacing: '-0.02em' }}>
          Poker Arena
        </span>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: 8,
          background: 'var(--glass)',
          borderRadius: 8,
          padding: '4px 12px',
          border: '1px solid var(--border)',
        }}>
          <span style={{ fontSize: 12, color: 'var(--text-muted)' }}>
            Hand #{state.hand_number}
          </span>
          <span style={{
            fontSize: 11,
            color: 'var(--accent)',
            fontWeight: 600,
            background: 'rgba(52,211,153,0.1)',
            borderRadius: 6,
            padding: '1px 8px',
          }}>
            {STREET_LABELS[state.street] ?? state.street}
          </span>
        </div>
        <div style={{ flex: 1 }} />
        <button
          onClick={() => setPaused(p => !p)}
          style={{
            padding: '6px 16px',
            borderRadius: 8,
            background: paused ? 'rgba(251,191,36,0.15)' : 'var(--glass)',
            color: paused ? '#fbbf24' : 'var(--text-muted)',
            fontSize: 13,
            fontWeight: 600,
            border: `1px solid ${paused ? 'rgba(251,191,36,0.3)' : 'var(--border)'}`,
          }}
        >
          {paused ? '▶ 继续' : '⏸ 暂停'}
        </button>
        <button
          onClick={onQuit}
          style={{
            padding: '6px 16px',
            borderRadius: 8,
            background: 'var(--glass)',
            color: 'var(--text-muted)',
            fontSize: 13,
            fontWeight: 500,
            border: '1px solid var(--border)',
          }}
        >
          退出
        </button>
      </div>

      {/* Main content */}
      <div style={{ display: 'flex', alignItems: 'center', gap: 20, width: '100%', justifyContent: 'center' }}>
        {/* Poker table */}
        <div style={{
          position: 'relative',
          width: '68%',
          maxWidth: 820,
          height: '62vh',
          borderRadius: '50%',
          background: `
            radial-gradient(ellipse at 50% 35%, #1a6b3a 0%, #145a2e 30%, #0e4422 55%, #082e16 80%, #051e0e 100%)
          `,
          border: '12px solid #111118',
          boxShadow: `
            inset 0 2px 80px rgba(0,0,0,0.5),
            inset 0 0 30px rgba(0,0,0,0.3),
            0 0 0 3px rgba(255,255,255,0.04),
            0 0 0 14px #0c0c12,
            0 0 100px rgba(0,0,0,0.7),
            0 30px 80px rgba(0,0,0,0.6)
          `,
          overflow: 'visible',
        }}>
          {/* Table inner rim */}
          <div style={{
            position: 'absolute',
            inset: 6,
            borderRadius: '50%',
            border: '1px solid rgba(255,255,255,0.06)',
            pointerEvents: 'none',
          }} />
          {/* Felt texture overlay */}
          <div style={{
            position: 'absolute',
            inset: 0,
            borderRadius: '50%',
            background: 'repeating-conic-gradient(rgba(255,255,255,0.003) 0% 25%, transparent 0% 50%)',
            pointerEvents: 'none',
          }} />

          {/* Community cards + Pot — centered together */}
          <div style={{
            position: 'absolute',
            top: '50%',
            left: '50%',
            transform: 'translate(-50%, -50%)',
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            gap: 12,
          }}>
            {/* Community cards */}
            {state.board.length > 0 && (
              <div style={{ display: 'flex', gap: 12, alignItems: 'center' }}>
                {state.board.map((c, i) => (
                  <motion.div
                    key={`${c}-${i}`}
                    initial={{ y: -30, opacity: 0, scale: 0.85 }}
                    animate={{ y: 0, opacity: 1, scale: 1 }}
                    transition={{ duration: 0.45, delay: i * 0.12, ease: [0.34, 1.56, 0.64, 1] }}
                  >
                    <Card card={c} size="lg" />
                  </motion.div>
                ))}
              </div>
            )}

            {/* Pot */}
            {state.pot > 0 && (
              <motion.div
                initial={{ opacity: 0, scale: 0.9 }}
                animate={{ opacity: 1, scale: 1 }}
                style={{
                  background: 'linear-gradient(135deg, rgba(251,191,36,0.15), rgba(245,158,11,0.1))',
                  backdropFilter: 'blur(10px)',
                  borderRadius: 28,
                  padding: '8px 28px',
                  border: '1.5px solid rgba(251,191,36,0.35)',
                  display: 'flex',
                  alignItems: 'center',
                  gap: 8,
                  boxShadow: '0 0 24px rgba(251,191,36,0.15), 0 4px 12px rgba(0,0,0,0.3)',
                }}
              >
                <span style={{
                  fontSize: 11,
                  fontWeight: 700,
                  color: 'rgba(251,191,36,0.7)',
                  textTransform: 'uppercase',
                  letterSpacing: '0.1em',
                }}>POT</span>
                <span style={{
                  fontSize: 22,
                  fontWeight: 800,
                  fontFamily: 'var(--font-mono)',
                  color: '#fbbf24',
                  letterSpacing: '-0.02em',
                  textShadow: '0 0 12px rgba(251,191,36,0.3)',
                }}>
                  {state.pot.toLocaleString()}
                </span>
              </motion.div>
            )}
          </div>

          {/* Player seats */}
          {state.players.map((p, i) => (
            <div
              key={i}
              style={{
                position: 'absolute',
                top: positions[i].top,
                left: positions[i].left,
                transform: 'translate(-50%, -50%)',
                zIndex: 2,
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

      {/* Result overlay — stays 5 seconds */}
      <AnimatePresence>
        {pendingResult && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            transition={{ duration: 0.4 }}
            style={{
              position: 'absolute',
              inset: 0,
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              background: 'rgba(0,0,0,0.55)',
              backdropFilter: 'blur(8px)',
              zIndex: 10,
            }}
          >
            <motion.div
              initial={{ scale: 0.9, y: 20 }}
              animate={{ scale: 1, y: 0 }}
              exit={{ scale: 0.9 }}
              transition={{ type: 'spring', damping: 20 }}
              style={{
                background: 'rgba(18,18,22,0.9)',
                backdropFilter: 'blur(20px)',
                borderRadius: 20,
                padding: '32px 48px',
                textAlign: 'center',
                border: '1px solid var(--border-light)',
                boxShadow: 'var(--shadow-lg)',
                maxWidth: analysisText ? 640 : 400,
                maxHeight: '80vh',
                overflowY: 'auto',
              }}
            >
              {pendingResult.winners.map((w, i) => (
                <div key={i} style={{ fontSize: 20, fontWeight: 700, marginBottom: 6, letterSpacing: '-0.01em' }}>
                  <span style={{ color: 'var(--accent)' }}>{state.players[w.player]?.name}</span>
                  <span style={{ color: 'var(--text-muted)', margin: '0 10px' }}>赢得</span>
                  <span style={{ color: '#fbbf24', fontFamily: 'var(--font-mono)', fontSize: 22 }}>{w.amount}</span>
                  {w.hand_rank && <span style={{ color: 'var(--text-secondary)', fontSize: 14, marginLeft: 10 }}>({w.hand_rank})</span>}
                </div>
              ))}

              {/* Countdown bar — hidden when analyzing */}
              {!analysisText && !analysisLoading && (
                <motion.div
                  initial={{ scaleX: 1 }}
                  animate={{ scaleX: paused ? undefined : 0 }}
                  transition={{ duration: paused ? 0 : 5, ease: 'linear' }}
                  style={{
                    marginTop: 20,
                    height: 3,
                    borderRadius: 2,
                    background: paused ? '#fbbf24' : 'var(--accent)',
                    transformOrigin: 'left',
                    opacity: 0.6,
                  }}
                />
              )}
              {paused && !analysisText && !analysisLoading && (
                <span style={{ fontSize: 11, color: '#fbbf24', marginTop: 8, fontWeight: 600 }}>
                  已暂停
                </span>
              )}

              {/* Analysis section */}
              {analysisLoading && (
                <div style={{ marginTop: 20, color: 'var(--text-muted)', fontSize: 14 }}>
                  <motion.span
                    animate={{ opacity: [0.4, 1, 0.4] }}
                    transition={{ duration: 1.5, repeat: Infinity }}
                  >
                    🤔 AI 正在分析对局...
                  </motion.span>
                </div>
              )}

              {analysisText && (
                <div
                  style={{
                    marginTop: 20,
                    textAlign: 'left',
                    fontSize: 13,
                    lineHeight: 1.7,
                    color: 'var(--text-secondary)',
                    background: 'rgba(255,255,255,0.03)',
                    borderRadius: 12,
                    padding: '16px 20px',
                    border: '1px solid var(--border)',
                  }}
                  dangerouslySetInnerHTML={{ __html: renderMarkdown(analysisText) }}
                />
              )}

              {/* Buttons row */}
              <div style={{ marginTop: 16, display: 'flex', gap: 10, justifyContent: 'center' }}>
                {!analysisText && !analysisLoading && (
                  <button
                    onClick={requestAnalysis}
                    style={{
                      padding: '10px 24px',
                      borderRadius: 12,
                      background: 'linear-gradient(135deg, rgba(99,102,241,0.8), rgba(139,92,246,0.8))',
                      color: '#fff',
                      fontSize: 14,
                      fontWeight: 700,
                      border: 'none',
                      boxShadow: '0 2px 12px rgba(99,102,241,0.3)',
                    }}
                  >
                    🧠 AI 分析对局
                  </button>
                )}
                <button
                  onClick={skipResult}
                  style={{
                    padding: '10px 28px',
                    borderRadius: 12,
                    background: 'rgba(255,255,255,0.08)',
                    color: 'var(--text-secondary)',
                    fontSize: 13,
                    fontWeight: 600,
                    border: '1px solid var(--border)',
                  }}
                >
                  下一把 →
                </button>
              </div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Action panel + timer */}
      {isHumanTurn && (
        <div style={{ position: 'absolute', bottom: 20, zIndex: 5, display: 'flex', alignItems: 'center', gap: 12 }}>
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

      {/* Waiting — deal button */}
      {isWaiting && !state.result && (
        <div style={{ position: 'absolute', bottom: 28 }}>
          <motion.button
            whileHover={{ scale: 1.04 }}
            whileTap={{ scale: 0.96 }}
            onClick={onNewHand}
            style={{
              padding: '14px 40px',
              borderRadius: 16,
              background: 'linear-gradient(135deg, var(--accent), #059669)',
              color: '#000',
              fontSize: 17,
              fontWeight: 700,
              boxShadow: '0 4px 24px rgba(52,211,153,0.3)',
              letterSpacing: '-0.01em',
            }}
          >
            发牌
          </motion.button>
        </div>
      )}
    </div>
  )
}
