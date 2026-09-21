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

// Upgraded Strategy Parameters
typedef struct {
    int lookback_period;       
    int atr_period;            
    int max_pullback_candles;  // Changed to 6
    double risk_reward_ratio;  
    int ema_period;            // 200 EMA Filter
    int session_start_hour;    // 12 (12:00 PM GMT)
    int session_end_hour;      // 17 (17:00 PM GMT)
    double spread_slippage_pips; // Fixed cost deduction (e.g. 1.5)
} StrategyParams;

typedef struct {
    int active_breakout;       
    double broken_level;       
    int candles_since_breakout; 
} StrategyState;

// Core Functions
int fetch_historical_data(const char* symbol, const char* interval, Candle* out_buffer, int target_candles);
void parse_live_tick(const char* json_string, LiveTick* out_tick);

int strategy_breakout_pullback(Candle* prices, int current_idx, StrategyParams* params, StrategyState* state, double* out_sl);
double calculate_lot_size(Account* acc, double entry, double stop_loss);
int execute_realistic_backtest_trade(Account* acc, Trade* trade, Candle* prices, int start_idx, StrategyParams* params);
void export_report(Trade* trades, int count, const char* filename);

#endif