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
    int trend_ema_period;  
    int rsi_period;        
    double rsi_oversold;   
    double rsi_overbought; 
    double sl_pips;
    double tp_pips;
} StrategyParams;

// Data & Parsing Functions
int fetch_historical_data(const char* symbol, const char* interval, Candle* out_buffer, int max_candles);
void parse_live_tick(const char* json_string, LiveTick* out_tick);

// Strategy & Execution Functions
int strategy_trend_pullback(Candle* prices, int current_idx, StrategyParams* params);
double calculate_lot_size(Account* acc, double entry, double stop_loss);
void execute_realistic_backtest_trade(Account* acc, Trade* trade, Candle* future_prices, int start_idx);
void export_report(Trade* trades, int count, const char* filename);

#endif