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
        printf("[INFO] Initializing backtest engine...\n");

        Candle raw_history[1000];
        memset(raw_history, 0, sizeof(raw_history));

        int count = fetch_historical_data("EUR/USD", "1h", raw_history, 1000);
        if (count < 60) {
            printf("[ERROR] Insufficient data fetched (%d candles). Exiting.\n", count);
            return 1;
        }

        // Twelve Data returns newest to oldest. Reverse to process chronologically.
        reverse_candles(raw_history, count);
        printf("[INFO] Chronologically aligned %d candles.\n", count);

        // --- USER CONFIGURABLE CAPITAL & STRATEGY SETTINGS ---
        Account acc = {
            .initial_balance = 10000.0,
            .current_balance = 10000.0,
            .max_risk_pct = 0.02,     // 2% per trade when use_fixed_lot = 0
            .use_fixed_lot = 1,       // 1 = Fixed Lot, 0 = Dynamic Risk
            .fixed_lot_size = 0.10    // 0.10 lots
        };

        StrategyParams params = {
            .bb_period = 20,
            .bb_std_dev = 2.0,
            .sl_pips = 25.0,
            .tp_pips = 50.0
        };
        // -----------------------------------------------------

        Trade trade_log[1000];
        int trade_count = 0;

        for (int i = params.bb_period; i < count - 1; i++) {
            int signal = strategy_bollinger_bands(raw_history, i, &params);
            if (signal == 0) continue;

            Trade t;
            memset(&t, 0, sizeof(Trade));
            strncpy(t.symbol, "EUR/USD", sizeof(t.symbol) - 1);
            t.type = signal;
            t.entry_price = raw_history[i].close;

            double sl_dist = params.sl_pips / 10000.0;
            double tp_dist = params.tp_pips / 10000.0;

            if (signal == 1) { // BUY
                t.stop_loss = t.entry_price - sl_dist;
                t.take_profit = t.entry_price + tp_dist;
            } else { // SELL
                t.stop_loss = t.entry_price + sl_dist;
                t.take_profit = t.entry_price - tp_dist;
            }

            t.lot_size = calculate_lot_size(&acc, t.entry_price, t.stop_loss);
            if (t.lot_size <= 0.0) continue;

            execute_realistic_backtest_trade(&acc, &t, raw_history, i);

            if (t.realized_pnl != 0.0) {
                trade_log[trade_count++] = t;
            }
        }

        export_report(trade_log, trade_count, "reports/backtest_results.csv");

        double total_pnl = acc.current_balance - acc.initial_balance;
        int wins = 0;
        for (int i = 0; i < trade_count; i++) {
            if (trade_log[i].realized_pnl > 0) wins++;
        }
        double win_rate = (trade_count > 0) ? ((double)wins / trade_count) * 100.0 : 0.0;

        printf("\n==========================================\n");
        printf("         BACKTEST PERFORMANCE SUMMARY     \n");
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