#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>
#include "bot.h"
#include "csv_exporter.h"

#define DATA_SIZE 15000 

static void reverse_candles(Candle* arr, int count) {
    for (int i = 0; i < count / 2; i++) {
        Candle temp = arr[i];
        arr[i] = arr[count - 1 - i];
        arr[count - 1 - i] = temp;
    }
}

int compare_results(const void *a, const void *b) {
    SimulationResult *resA = (SimulationResult *)a;
    SimulationResult *resB = (SimulationResult *)b;
    if (resB->net_pnl > resA->net_pnl) return 1;
    if (resB->net_pnl < resA->net_pnl) return -1;
    return 0;
}

// Added out_trades and out_trade_count pointers for the final verification pass
SimulationResult run_backtest(int lookback, double atr_mult, double rr_ratio, Candle* data, int data_size, Trade* out_trades, int* out_trade_count) {
    Account acc = {
        .initial_balance = 10000.0,
        .current_balance = 10000.0,
        .max_risk_pct = 0.01,
        .use_fixed_lot = 0,
        .fixed_lot_size = 0.0
    };

    StrategyParams params = {
        .lookback_period = lookback,    
        .atr_period = 14,         
        .atr_multiplier = atr_mult,
        .risk_reward_ratio = rr_ratio,
        .session_start_hour = 8,
        .session_end_hour = 17,
        .spread_slippage_pips = 1.5
    };
    
    StrategyState state = {0, 0.0, 0};
    
    int wins = 0;
    int total_trades = 0;
    double peak_balance = acc.initial_balance;
    double max_drawdown = 0.0;

    for (int i = params.lookback_period; i < data_size - 1; i++) {
        double calculated_sl = 0.0;
        int signal = strategy_fakeout_reversal(data, i, &params, &state, &calculated_sl);
        
        if (signal == 0) continue;

        Trade t;
        memset(&t, 0, sizeof(Trade));
        t.type = signal;
        strncpy(t.symbol, "EUR/USD", sizeof(t.symbol) - 1);
        strcpy(t.entry_time, data[i].timestamp);
        t.entry_price = data[i+1].open; 
        t.stop_loss = calculated_sl;

        double risk_distance = fabs(t.entry_price - t.stop_loss);
        if (signal == 1) {
            t.take_profit = t.entry_price + (risk_distance * params.risk_reward_ratio);
        } else {
            t.take_profit = t.entry_price - (risk_distance * params.risk_reward_ratio);
        }

        t.lot_size = calculate_lot_size(&acc, t.entry_price, t.stop_loss);
        if (t.lot_size < 0.01) continue;

        int close_idx = execute_realistic_backtest_trade(&acc, &t, data, i + 1, &params);

        if (t.realized_pnl != 0.0) {
            
            // Only save trade data to memory if pointers were provided
            if (out_trades != NULL && out_trade_count != NULL) {
                out_trades[*out_trade_count] = t;
            }

            total_trades++;
            if (out_trade_count != NULL) (*out_trade_count)++;
            if (t.realized_pnl > 0) wins++;
            
            if (acc.current_balance > peak_balance) {
                peak_balance = acc.current_balance;
            }
            double current_drawdown = ((peak_balance - acc.current_balance) / peak_balance) * 100.0;
            if (current_drawdown > max_drawdown) {
                max_drawdown = current_drawdown;
            }

            i = close_idx; 
        }
    }

    SimulationResult res;
    res.lookback = lookback;
    res.atr_mult = atr_mult;
    res.rr_ratio = rr_ratio;
    res.net_pnl = acc.current_balance - acc.initial_balance;
    res.win_rate = (total_trades > 0) ? ((double)wins / total_trades) * 100.0 : 0.0;
    res.total_trades = total_trades;
    res.max_drawdown = max_drawdown;

    return res;
}

int main(int argc, char *argv[]) {
    printf("[INFO] Initializing OpenMP Grid Search Optimizer...\n");

    Candle* raw_history = malloc(DATA_SIZE * sizeof(Candle));
    if (!raw_history) return 1;
    memset(raw_history, 0, DATA_SIZE * sizeof(Candle));

    int count = fetch_historical_data("EUR/USD", "15min", raw_history, DATA_SIZE);
    if (count < 500) {
        printf("[ERROR] Insufficient data.\n");
        free(raw_history);
        return 1;
    }
    reverse_candles(raw_history, count);

    int num_lookback = 9;  // 20 to 100
    int num_atr = 11;      // 1.0 to 3.0
    int num_rr = 5;        // 1.0 to 3.0
    int total_permutations = num_lookback * num_atr * num_rr;

    SimulationResult* results = malloc(total_permutations * sizeof(SimulationResult));

    printf("[INFO] Grid Search Started. Sweeping %d permutations...\n", total_permutations);
    double start_time = omp_get_wtime();

    #pragma omp parallel for collapse(3) schedule(dynamic)
    for (int l = 0; l < num_lookback; l++) {
        for (int a = 0; a < num_atr; a++) {
            for (int r = 0; r < num_rr; r++) {
                int current_lookback = 20 + (l * 10);
                double current_atr = 1.0 + (a * 0.2);
                double current_rr = 1.0 + (r * 0.5);

                // Pass NULL for the array during the fast parallel search to save memory
                SimulationResult res = run_backtest(current_lookback, current_atr, current_rr, raw_history, count, NULL, NULL);
                
                int flat_idx = l * (num_atr * num_rr) + a * num_rr + r;
                results[flat_idx] = res;
            }
        }
    }

    double end_time = omp_get_wtime();
    printf("[INFO] Matrix completed in %.2f seconds.\n\n", end_time - start_time);

    qsort(results, total_permutations, sizeof(SimulationResult), compare_results);

    printf("===============================================================================\n");
    printf(" TOP 1 OPTIMIZED PARAMETER SET\n");
    printf("===============================================================================\n");
    printf("Lookback: %d | ATR Mult: %.1f | R:R Ratio: %.1f | Win Rate: %.2f%% | Net PnL: $%.2f\n",
            results[0].lookback, results[0].atr_mult, results[0].rr_ratio, results[0].win_rate, results[0].net_pnl);
    printf("===============================================================================\n\n");

    // FINAL VERIFICATION RUN
    printf("[INFO] Running final verification sequence with optimal parameters...\n");
    Trade* verification_trades = malloc(5000 * sizeof(Trade));
    int verification_count = 0;

    run_backtest(results[0].lookback, results[0].atr_mult, results[0].rr_ratio, raw_history, count, verification_trades, &verification_count);
    
    export_trades_to_csv("reports/best_strategy_trades.csv", verification_trades, verification_count);

    free(verification_trades);
    free(results);
    free(raw_history);
    return 0;
}