#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bot.h"

#define DATA_SIZE 15000 

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
        printf("[INFO] Initializing 15-Minute Fakeout & Reversal Engine...\n");

        Candle* raw_history = malloc(DATA_SIZE * sizeof(Candle));
        if (!raw_history) {
            printf("[ERROR] Memory allocation failed.\n");
            return 1;
        }
        memset(raw_history, 0, DATA_SIZE * sizeof(Candle));

        int count = fetch_historical_data("EUR/USD", "15min", raw_history, DATA_SIZE);
        if (count < 500) {
            printf("[ERROR] Insufficient data. Fetched: %d\n", count);
            free(raw_history);
            return 1;
        }

        reverse_candles(raw_history, count);
        printf("[INFO] Chronologically aligned %d candles.\n", count);

        // --- ACCOUNT & FAKEOUT STRATEGY SPECIFICATIONS ---
        Account acc = {
            .initial_balance = 10000.0,
            .current_balance = 10000.0,
            .max_risk_pct = 0.01,     // 1% Risk per trade
            .use_fixed_lot = 0,       
            .fixed_lot_size = 0.0     
        };

        StrategyParams params = {
            .lookback_period = 50,    
            .atr_period = 14,         
            .risk_reward_ratio = 2.0,      // Fixed 1:2 R:R
            .session_start_hour = 8,       // 08:00 AM GMT
            .session_end_hour = 17,        // 17:00 PM GMT
            .spread_slippage_pips = 1.5    // Execution spread penalty
        };
        
        StrategyState state = {0, 0.0, 0};
        // -------------------------------------------------

        Trade trade_log[5000];
        int trade_count = 0;

        for (int i = params.lookback_period; i < count - 1; i++) {
            double calculated_sl = 0.0;
            
            int signal = strategy_fakeout_reversal(raw_history, i, &params, &state, &calculated_sl);
            
            if (signal == 0) continue;

            Trade t;
            memset(&t, 0, sizeof(Trade));
            strncpy(t.symbol, "EUR/USD", sizeof(t.symbol) - 1);
            strcpy(t.entry_time, raw_history[i].timestamp);
            t.type = signal;
            
            // Entry is exactly at the Open of the subsequent candle
            t.entry_price = raw_history[i+1].open; 
            t.stop_loss = calculated_sl;

            double risk_distance = fabs(t.entry_price - t.stop_loss);
            if (signal == 1) {
                t.take_profit = t.entry_price + (risk_distance * params.risk_reward_ratio);
            } else {
                t.take_profit = t.entry_price - (risk_distance * params.risk_reward_ratio);
            }

            t.lot_size = calculate_lot_size(&acc, t.entry_price, t.stop_loss);
            if (t.lot_size < 0.01) continue;

            int close_idx = execute_realistic_backtest_trade(&acc, &t, raw_history, i + 1, &params);

            if (t.realized_pnl != 0.0) {
                trade_log[trade_count++] = t;
                i = close_idx; 
            }
        }

        export_report(trade_log, trade_count, "reports/backtest_15m_Fakeout.csv");

        double total_pnl = acc.current_balance - acc.initial_balance;
        int wins = 0;
        for (int i = 0; i < trade_count; i++) {
            if (trade_log[i].realized_pnl > 0) wins++;
        }
        double win_rate = (trade_count > 0) ? ((double)wins / trade_count) * 100.0 : 0.0;

        printf("\n==========================================\n");
        printf("       FAKEOUT STRATEGY SUMMARY           \n");
        printf("==========================================\n");
        printf("Initial Balance : $%.2f\n", acc.initial_balance);
        printf("Final Balance   : $%.2f\n", acc.current_balance);
        printf("Net Profit/Loss : $%.2f (Inc. Spread)\n", total_pnl);
        printf("Total Trades    : %d\n", trade_count);
        printf("Win Rate        : %.2f%%\n", win_rate);
        printf("==========================================\n");
        
        free(raw_history);
    } 

    return 0;
}