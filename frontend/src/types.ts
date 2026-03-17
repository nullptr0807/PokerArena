/** Types matching the backend game state. */

export interface PlayerState {
  index: number
  name: string
  chips: number
  bet_this_street: number
  bet_this_hand: number
  folded: boolean
  all_in: boolean
  is_human: boolean
  hole_cards: string[] | null
  stats: {
    hands_played: number
    vpip: number
    bb_per_hand: number
    total_profit_bb: number
  }
}

export interface PotInfo {
  amount: number
  eligible: number[]
}

export interface WinnerInfo {
  player: number
  amount: number
  hand_rank: string
}

export interface HandResult {
  winners: WinnerInfo[]
  board: string[]
  pots: PotInfo[]
}

export interface ActionLogEntry {
  player?: number
  name?: string
  action?: string
  amount?: number
  street?: string
  timestamp?: number
  type?: 'street'
  board?: string[]
}

export interface GameState {
  hand_number: number
  street: 'WAITING' | 'PREFLOP' | 'FLOP' | 'TURN' | 'RIVER' | 'SHOWDOWN'
  board: string[]
  current_player: number
  current_bet: number
  min_raise_to: number
  pot: number
  players: PlayerState[]
  valid_actions: string[]
  button: number
  all_in_showdown: boolean
  result?: HandResult
  action_log?: ActionLogEntry[]
}

export interface GameConfig {
  num_players: number
  small_blind: number
  big_blind: number
  starting_chips: number
  ai_difficulties: string[]
  debug_mode?: boolean
}

export interface DebugEvent {
  type: string
  timestamp?: number
  player?: number
  name?: string
  difficulty?: string
  stage?: string
  street?: string
  hole_cards?: number[] | string[]
  action?: string
  amount?: number
  compute_ms?: number
  total_ms?: number
  pot?: number
  current_bet?: number
  action_history?: string[]
  valid_actions?: string[]
  position?: string
  hand_number?: number
  button?: number
  players?: Array<{
    index: number
    name: string
    chips: number
    hole_cards: number[] | string[]
    difficulty: string
  }>
}

export type WSMessage =
  | { type: 'game_created'; data: Omit<GameConfig, 'ai_difficulties'> }
  | { type: 'state'; data: GameState }
  | { type: 'ai_action'; player: number; action: string; amount: number }
  | { type: 'run_it_results'; data: Array<{ board: string[]; winners: Array<{ player: number; amount: number; hand_rank: string }> }>; times: number }
  | { type: 'turn_timer'; deadline: number; seconds: number }
  | { type: 'timeout_fold'; player: number }
  | { type: 'ai_thinking'; player: number; stage: string | null }
  | { type: 'action_log'; data: ActionLogEntry[] }
  | { type: 'debug_event'; data: DebugEvent }
  | { type: 'error'; message: string }
  | { type: 'pong' }
