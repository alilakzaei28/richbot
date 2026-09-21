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
    double max_risk_pct;     // e.g., 0.02 for 2%
    int use_fixed_lot;       // 1 = Fixed Lot, 0 = Dynamic Lot based on max_risk_pct
    double fixed_lot_size;   // User-defined lot size (e.g., 0.1)
} Account;

typedef struct {
    int bb_period;
    double bb_std_dev;
    double sl_pips;
    double tp_pips;
} StrategyParams;

// Data & Parsing Functions
int fetch_historical_data(const char* symbol, const char* interval, Candle* out_buffer, int max_candles);
void parse_live_tick(const char* json_string, LiveTick* out_tick);

// Strategy & Execution Functions
int strategy_bollinger_bands(Candle* prices, int current_idx, StrategyParams* params);
double calculate_lot_size(Account* acc, double entry, double stop_loss);
void execute_realistic_backtest_trade(Account* acc, Trade* trade, Candle* future_prices, int start_idx);
void export_report(Trade* trades, int count, const char* filename);

#endif