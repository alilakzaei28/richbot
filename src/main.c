#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bot.h"

static void reverse_candles(Candle* arr, int count) {
    for (int i = 0; i < count / 2; i++) {
        Candle temp = arr[i];
        arr[i] = arr[count - 1 - i];
        arr[count - 1 - i] = temp;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s [--backtest | --live]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--backtest") == 0) {
        printf("[INFO] Initializing 15-Minute Breakout & Pullback Engine...\n");

        Candle raw_history[2500]; 
        memset(raw_history, 0, sizeof(raw_history));

        // Request 15-minute data
        int count = fetch_historical_data("EUR/USD", "15min", raw_history, 2500);
        if (count < 100) {
            printf("[ERROR] Insufficient data for 50-period strategy.\n");
            return 1;
        }

        reverse_candles(raw_history, count);
        printf("[INFO] Chronologically aligned %d candles (15m timeframe).\n", count);

        // --- ACCOUNT & STRATEGY SPECIFICATIONS ---
        Account acc = {
            .initial_balance = 10000.0,
            .current_balance = 10000.0,
            .max_risk_pct = 0.01,     // Exactly 1% risk per trade
            .use_fixed_lot = 0,       // Disable fixed lot to enable dynamic position sizing
            .fixed_lot_size = 0.0     
        };

        StrategyParams params = {
            .lookback_period = 50,    
            .atr_period = 14,         
            .max_pullback_candles = 10, 
            .risk_reward_ratio = 2.0  // Fixed 1:2 R:R
        };
        
        StrategyState state = {0, 0.0, 0};
        // -----------------------------------------

        Trade trade_log[1000];
        int trade_count = 0;

        for (int i = params.lookback_period; i < count - 1; i++) {
            double calculated_sl = 0.0;
            int signal = strategy_breakout_pullback(raw_history, i, &params, &state, &calculated_sl);
            
            if (signal == 0) continue;

            Trade t;
            memset(&t, 0, sizeof(Trade));
            strncpy(t.symbol, "EUR/USD", sizeof(t.symbol) - 1);
            strcpy(t.entry_time, raw_history[i].timestamp);
            t.type = signal;
            t.entry_price = raw_history[i].close;
            t.stop_loss = calculated_sl;

            // Mathematical 1:2 Risk to Reward Calculation
            double risk_distance = fabs(t.entry_price - t.stop_loss);
            if (signal == 1) {
                t.take_profit = t.entry_price + (risk_distance * params.risk_reward_ratio);
            } else {
                t.take_profit = t.entry_price - (risk_distance * params.risk_reward_ratio);
            }

            // Position Sizing: Exactly 1% of account equity
            t.lot_size = calculate_lot_size(&acc, t.entry_price, t.stop_loss);
            if (t.lot_size < 0.01) continue;

            // Simulates trade and returns the index where the trade hit SL or TP
            int close_idx = execute_realistic_backtest_trade(&acc, &t, raw_history, i);

            if (t.realized_pnl != 0.0) {
                trade_log[trade_count++] = t;
                
                // Pyramiding strict rule: 1 order at a time.
                // Advance the loop directly to the candle where the trade closed.
                i = close_idx; 
                
                // Clear any residual strategy state to start scanning fresh
                state.active_breakout = 0; 
                state.candles_since_breakout = 0;
            }
        }

        export_report(trade_log, trade_count, "reports/backtest_15m_Breakout.csv");

        double total_pnl = acc.current_balance - acc.initial_balance;
        int wins = 0;
        for (int i = 0; i < trade_count; i++) {
            if (trade_log[i].realized_pnl > 0) wins++;
        }
        double win_rate = (trade_count > 0) ? ((double)wins / trade_count) * 100.0 : 0.0;

        printf("\n==========================================\n");
        printf("     15-MIN BREAKOUT & PULLBACK SUMMARY   \n");
        printf("==========================================\n");
        printf("Initial Balance : $%.2f\n", acc.initial_balance);
        printf("Final Balance   : $%.2f\n", acc.current_balance);
        printf("Net Profit/Loss : $%.2f\n", total_pnl);
        printf("Total Trades    : %d\n", trade_count);
        printf("Win Rate        : %.2f%%\n", win_rate);
        printf("==========================================\n");
    } 

    return 0;
}