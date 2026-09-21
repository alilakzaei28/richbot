#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
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
        printf("[INFO] Initializing High-Probability Daily Engine...\n");

        Candle raw_history[1000]; 
        memset(raw_history, 0, sizeof(raw_history));

        // Fetch 500 daily candles. Twelve Data uses "1day" for the Daily timeframe.
        // We need older data to prime the 200 EMA so the 1-year backtest starts accurately.
        int count = fetch_historical_data("EUR/USD", "1day", raw_history, 500);
        if (count < 250) {
            printf("[ERROR] Insufficient data. Trend strategy needs at least 250 candles.\n");
            return 1;
        }

        reverse_candles(raw_history, count);
        printf("[INFO] Chronologically aligned %d daily candles.\n", count);

        // --- USER CONFIGURABLE CAPITAL & STRATEGY SETTINGS ---
        Account acc = {
            .initial_balance = 10000.0,
            .current_balance = 10000.0,
            .max_risk_pct = 0.02,     
            .use_fixed_lot = 1,       
            .fixed_lot_size = 0.50    // Half a standard lot
        };

        StrategyParams params = {
            .trend_ema_period = 200,  // Institutional trend benchmark
            .rsi_period = 14,         
            .rsi_oversold = 40.0,     // Shallow dips
            .rsi_overbought = 60.0,   // Shallow rallies
            .sl_pips = 50.0,          // Widened for Daily timeframe volatility (ATR)
            .tp_pips = 75.0           // 1:1.5 Risk-to-Reward Ratio
        };
        // -----------------------------------------------------

        Trade trade_log[1000];
        int trade_count = 0;

        // 1 Year of Forex trading is roughly 252 days.
        // We set our start index to exactly 252 candles from the end, ensuring a 1-year test.
        int start_idx = count - 252;
        if (start_idx < params.trend_ema_period) {
            start_idx = params.trend_ema_period; // Safety fallback
        }

        printf("[INFO] Executing 1-Year Backtest from candle index %d to %d\n", start_idx, count - 1);

        for (int i = start_idx; i < count - 1; i++) {
            int signal = strategy_trend_pullback(raw_history, i, &params);
            if (signal == 0) continue;

            Trade t;
            memset(&t, 0, sizeof(Trade));
            strncpy(t.symbol, "EUR/USD", sizeof(t.symbol) - 1);
            t.type = signal;
            t.entry_price = raw_history[i].close;

            double sl_dist = params.sl_pips / 10000.0;
            double tp_dist = params.tp_pips / 10000.0;

            if (signal == 1) { 
                t.stop_loss = t.entry_price - sl_dist;
                t.take_profit = t.entry_price + tp_dist;
            } else { 
                t.stop_loss = t.entry_price + sl_dist;
                t.take_profit = t.entry_price - tp_dist;
            }

            t.lot_size = calculate_lot_size(&acc, t.entry_price, t.stop_loss);
            if (t.lot_size <= 0.0) continue;

            execute_realistic_backtest_trade(&acc, &t, raw_history, i);

            if (t.realized_pnl != 0.0) {
                trade_log[trade_count++] = t;
                
                // Jump the index forward by 2 days after a closed trade 
                // to prevent the bot from immediately re-entering the exact same pullback
                i += 2; 
            }
        }

        export_report(trade_log, trade_count, "reports/backtest_EMA_RSI_1D.csv");

        double total_pnl = acc.current_balance - acc.initial_balance;
        int wins = 0;
        for (int i = 0; i < trade_count; i++) {
            if (trade_log[i].realized_pnl > 0) wins++;
        }
        double win_rate = (trade_count > 0) ? ((double)wins / trade_count) * 100.0 : 0.0;

        printf("\n==========================================\n");
        printf("         1-YEAR DAILY PERFORMANCE         \n");
        printf("==========================================\n");
        printf("Initial Balance : $%.2f\n", acc.initial_balance);
        printf("Final Balance   : $%.2f\n", acc.current_balance);
        printf("Net Profit/Loss : $%.2f\n", total_pnl);
        printf("Total Trades    : %d\n", trade_count);
        printf("Win Rate        : %.2f%%\n", win_rate);
        printf("==========================================\n");
    } 
    else if (strcmp(argv[1], "--live") == 0) {
        printf("[INFO] Live forward engine initialized.\n");
        const char *mock_tick = "{\"event\":\"price\",\"symbol\":\"EUR/USD\",\"bid\":1.08502,\"ask\":1.08514}";
        LiveTick tick;
        memset(&tick, 0, sizeof(LiveTick));
        parse_live_tick(mock_tick, &tick);
    }

    return 0;
}