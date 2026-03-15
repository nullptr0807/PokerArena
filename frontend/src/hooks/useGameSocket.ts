import { useCallback, useEffect, useRef, useState } from 'react'
import type { ActionLogEntry, GameConfig, GameState, WSMessage } from '../types'

export function useGameSocket() {
  const wsRef = useRef<WebSocket | null>(null)
  const [connected, setConnected] = useState(false)
  const [gameState, setGameState] = useState<GameState | null>(null)
  const [lastAIAction, setLastAIAction] = useState<{
    player: number
    action: string
    amount: number
  } | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [runItResults, setRunItResults] = useState<Array<{ board: string[]; winners: Array<{ player: number; amount: number; hand_rank: string }> }> | null>(null)
  const [turnSecondsLeft, setTurnSecondsLeft] = useState<number | null>(null)
  const [aiThinking, setAiThinking] = useState<Record<number, string | null>>({})
  const [actionLog, setActionLog] = useState<ActionLogEntry[]>([])
  const timerRef = useRef<ReturnType<typeof setInterval> | null>(null)
  const deadlineRef = useRef<number>(0)

  const startCountdown = useCallback((seconds: number) => {
    if (timerRef.current) clearInterval(timerRef.current)
    deadlineRef.current = Date.now() + seconds * 1000
    setTurnSecondsLeft(seconds)
    timerRef.current = setInterval(() => {
      const left = Math.max(0, Math.ceil((deadlineRef.current - Date.now()) / 1000))
      setTurnSecondsLeft(left)
      if (left <= 0 && timerRef.current) {
        clearInterval(timerRef.current)
        timerRef.current = null
      }
    }, 250)
  }, [])

  const stopCountdown = useCallback(() => {
    if (timerRef.current) clearInterval(timerRef.current)
    timerRef.current = null
    setTurnSecondsLeft(null)
  }, [])

  const connect = useCallback(() => {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    const ws = new WebSocket(`${protocol}//${window.location.host}/ws/game`)
    wsRef.current = ws

    ws.onopen = () => setConnected(true)
    ws.onclose = () => {
      setConnected(false)
      setTimeout(connect, 2000)
    }
    ws.onerror = () => setError('Connection lost')

    ws.onmessage = (event) => {
      const msg: WSMessage = JSON.parse(event.data)
      switch (msg.type) {
        case 'state':
          setGameState(msg.data)
          setError(null)
          break
        case 'ai_action':
          setLastAIAction({
            player: msg.player,
            action: msg.action,
            amount: msg.amount,
          })
          break
        case 'ai_thinking':
          setAiThinking((prev) => ({ ...prev, [msg.player]: msg.stage }))
          break
        case 'action_log':
          setActionLog(msg.data)
          break
        case 'run_it_results':
          setRunItResults(msg.data)
          break
        case 'turn_timer':
          startCountdown(msg.seconds)
          break
        case 'timeout_fold':
          // Timer expired, server auto-folded
          stopCountdown()
          break
        case 'error':
          setError(msg.message)
          break
        case 'game_created':
          setError(null)
          break
      }
    }
  }, [startCountdown, stopCountdown])

  useEffect(() => {
    connect()
    return () => {
      wsRef.current?.close()
      stopCountdown()
    }
  }, [connect, stopCountdown])

  const send = useCallback((data: Record<string, unknown>) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(data))
    }
  }, [])

  const createGame = useCallback(
    (config: GameConfig) => {
      send({ type: 'create_game', ...config })
    },
    [send],
  )

  const startHand = useCallback(() => {
    setRunItResults(null)
    setActionLog([])
    setAiThinking({})
    stopCountdown()
    send({ type: 'start_hand' })
  }, [send, stopCountdown])

  const act = useCallback(
    (action: string, amount = 0) => {
      stopCountdown()
      send({ type: 'action', action, amount })
    },
    [send, stopCountdown],
  )

  const runItMultiple = useCallback(
    (times: number) => {
      send({ type: 'run_it_multiple', times })
    },
    [send],
  )

  return {
    connected,
    gameState,
    lastAIAction,
    error,
    runItResults,
    turnSecondsLeft,
    aiThinking,
    actionLog,
    createGame,
    startHand,
    act,
    runItMultiple,
  }
}
