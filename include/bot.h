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
} Trade;

typedef struct {
    double initial_balance;
    double current_balance;
    double max_risk_pct;     
    int use_fixed_lot;       
    double fixed_lot_size;   
} Account;

// Strategy Parameters
typedef struct {
    int lookback_period;       // 50 periods for Support/Resistance
    int atr_period;            // 14 periods for Stop Loss buffer
    int max_pullback_candles;  // 10 candles wait window
    double risk_reward_ratio;  // 2.0 (1:2 R:R)
} StrategyParams;

// Strategy State Tracker (to remember breakouts across loop iterations)
typedef struct {
    int active_breakout;       // 1 (Long Breakout), -1 (Short Breakout), 0 (None)
    double broken_level;       // The exact Resistance or Support broken
    int candles_since_breakout; 
} StrategyState;

// Core Functions
int fetch_historical_data(const char* symbol, const char* interval, Candle* out_buffer, int max_candles);
void parse_live_tick(const char* json_string, LiveTick* out_tick);

int strategy_breakout_pullback(Candle* prices, int current_idx, StrategyParams* params, StrategyState* state, double* out_sl);
double calculate_lot_size(Account* acc, double entry, double stop_loss);
int execute_realistic_backtest_trade(Account* acc, Trade* trade, Candle* prices, int start_idx);
void export_report(Trade* trades, int count, const char* filename);

#endif