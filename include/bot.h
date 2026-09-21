#ifndef BOT_H
#define BOT_H

typedef struct {
    char timestamp[32];
    double open;
    double high;
    double low;
    double close;
    double volume;
} Candle;

typedef struct {
    char symbol[16];
    double bid;
    double ask;
} LiveTick;

typedef struct {
    char symbol[16];
    double entry_price;
    double stop_loss;
    double take_profit;
    double lot_size;
    int type; // 1 for BUY, -1 for SELL
    double realized_pnl;
    char entry_time[32];
    char exit_time[32];
    double exit_price; // Added to capture exact exit coordinate
} Trade;

typedef struct {
    double initial_balance;
    double current_balance;
    double max_risk_pct;     
    int use_fixed_lot;       
    double fixed_lot_size;   
} Account;

typedef struct {
    int lookback_period;       
    int atr_period;
    double atr_multiplier;     
    double risk_reward_ratio;  
    int session_start_hour;    
    int session_end_hour;      
    double spread_slippage_pips; 
} StrategyParams;

typedef struct {
    int active_breakout;       
    double broken_level;       
    int candles_since_breakout; 
} StrategyState;

typedef struct {
    int lookback;
    double atr_mult;
    double rr_ratio;
    double net_pnl;
    double win_rate;
    int total_trades;
    double max_drawdown;
} SimulationResult;

int fetch_historical_data(const char* symbol, const char* interval, Candle* out_buffer, int target_candles);
void parse_live_tick(const char* json_string, LiveTick* out_tick);
int strategy_fakeout_reversal(Candle* prices, int current_idx, StrategyParams* params, StrategyState* state, double* out_sl);
double calculate_lot_size(Account* acc, double entry, double stop_loss);
int execute_realistic_backtest_trade(Account* acc, Trade* trade, Candle* prices, int start_idx, StrategyParams* params);

#endif