import { useState } from 'react'
import { useGameSocket } from './hooks/useGameSocket'
import { Setup } from './components/Setup'
import { Table } from './components/Table'
import { StatsOverlay } from './components/StatsOverlay'
import { RunItMultiple } from './components/RunItMultiple'
import { DebugPanel } from './components/DebugPanel'
import type { GameConfig } from './types'

export default function App() {
  const {
    connected, gameState, error, runItResults, turnSecondsLeft,
    aiThinking, actionLog, debugEvents,
    createGame, startHand, act, runItMultiple,
  } = useGameSocket()
  const [gameStarted, setGameStarted] = useState(false)
  const [showStats, setShowStats] = useState(false)
  const [debugMode, setDebugMode] = useState(false)
  const [showDebug, setShowDebug] = useState(false)

  const handleCreateGame = (config: GameConfig) => {
    createGame(config)
    setGameStarted(true)
    setDebugMode(config.debug_mode ?? false)
    setShowDebug(config.debug_mode ?? false)
    setTimeout(() => startHand(), 300)
  }

  if (!connected) {
    return (
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        height: '100%',
        flexDirection: 'column',
        gap: 16,
      }}>
        <div style={{
          width: 36,
          height: 36,
          border: '2px solid var(--accent)',
          borderTopColor: 'transparent',
          borderRadius: '50%',
          animation: 'spin 0.8s linear infinite',
        }} />
        <span style={{ color: 'var(--text-muted)', fontSize: 14, letterSpacing: '-0.01em' }}>
          连接服务器中...
        </span>
      </div>
    )
  }

  if (error) {
    return (
      <div style={{
        position: 'fixed',
        top: 16,
        right: 16,
        background: 'rgba(244, 63, 94, 0.9)',
        backdropFilter: 'blur(8px)',
        color: '#fff',
        padding: '10px 20px',
        borderRadius: 12,
        fontSize: 13,
        fontWeight: 600,
        zIndex: 100,
        border: '1px solid rgba(244,63,94,0.3)',
      }}>
        {error}
      </div>
    )
  }

  if (!gameStarted || !gameState) {
    return <Setup onStart={handleCreateGame} />
  }

  const isAllInShowdown = gameState.all_in_showdown

  return (
    <>
      <Table state={gameState} onAction={act} onNewHand={startHand} onQuit={() => setGameStarted(false)} turnSecondsLeft={turnSecondsLeft} aiThinking={aiThinking} actionLog={actionLog} />

      {/* Top-right buttons */}
      <div style={{
        position: 'fixed',
        top: 12,
        right: 16,
        display: 'flex',
        gap: 8,
        zIndex: 10,
      }}>
        {debugMode && (
          <button
            onClick={() => setShowDebug((v) => !v)}
            style={{
              padding: '6px 14px',
              borderRadius: 10,
              background: showDebug ? 'rgba(251,191,36,0.15)' : 'var(--glass)',
              color: showDebug ? '#fbbf24' : 'var(--text-muted)',
              fontSize: 12,
              fontWeight: 600,
              border: '1px solid var(--border)',
              backdropFilter: 'blur(8px)',
            }}
          >
            Debug
          </button>
        )}
        <button
          onClick={() => setShowStats(true)}
          style={{
            padding: '6px 14px',
            borderRadius: 10,
            background: 'var(--glass)',
            color: 'var(--text-muted)',
            fontSize: 12,
            fontWeight: 600,
            border: '1px solid var(--border)',
            backdropFilter: 'blur(8px)',
          }}
        >
          统计
        </button>
      </div>

      <DebugPanel
        events={debugEvents}
        visible={showDebug && debugMode}
        onClose={() => setShowDebug(false)}
      />

      <RunItMultiple
        visible={isAllInShowdown}
        onRun={runItMultiple}
        results={runItResults}
        playerNames={gameState.players.map((p) => p.name)}
      />

      <StatsOverlay
        visible={showStats}
        players={gameState.players}
        handNumber={gameState.hand_number}
        onClose={() => setShowStats(false)}
      />
    </>
  )
}
