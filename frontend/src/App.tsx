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
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          height: '100%',
          flexDirection: 'column',
          gap: 12,
        }}
      >
        <div
          style={{
            width: 32,
            height: 32,
            border: '3px solid var(--accent)',
            borderTopColor: 'transparent',
            borderRadius: '50%',
            animation: 'spin 1s linear infinite',
          }}
        />
        <span style={{ color: 'var(--text-secondary)' }}>
          Connecting to server...
        </span>
        <style>{`@keyframes spin { to { transform: rotate(360deg) } }`}</style>
      </div>
    )
  }

  if (error) {
    return (
      <div
        style={{
          position: 'fixed',
          top: 16,
          right: 16,
          background: 'var(--danger)',
          color: '#fff',
          padding: '8px 16px',
          borderRadius: 8,
          fontSize: 13,
          zIndex: 100,
        }}
      >
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

      {/* Stats button */}
      <button
        onClick={() => setShowStats(true)}
        style={{
          position: 'fixed',
          top: 16,
          right: 16,
          padding: '6px 14px',
          borderRadius: 10,
          background: 'var(--bg-card)',
          color: 'var(--text-secondary)',
          fontSize: 13,
          fontWeight: 500,
          border: '1px solid var(--border)',
          zIndex: 10,
        }}
      >
        📊 Stats
      </button>

      {/* Debug toggle button */}
      {debugMode && (
        <button
          onClick={() => setShowDebug((v) => !v)}
          style={{
            position: 'fixed',
            top: 16,
            right: 100,
            padding: '6px 14px',
            borderRadius: 10,
            background: showDebug ? '#f59e0b' : 'var(--bg-card)',
            color: showDebug ? '#000' : '#f59e0b',
            fontSize: 13,
            fontWeight: 500,
            border: '1px solid rgba(245,158,11,0.3)',
            zIndex: 10,
          }}
        >
          🐛 Debug
        </button>
      )}

      {/* Debug Panel */}
      <DebugPanel
        events={debugEvents}
        visible={showDebug && debugMode}
        onClose={() => setShowDebug(false)}
      />

      {/* Run It Multiple Times */}
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
