#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>
#include "bot.h"

#define DATA_SIZE 15000 

static void reverse_candles(Candle* arr, int count) {
    for (int i = 0; i < count / 2; i++) {
        Candle temp = arr[i];
        arr[i] = arr[count - 1 - i];
        arr[count - 1 - i] = temp;
    }
}

// QSort Comparator for descending Net PnL
int compare_results(const void *a, const void *b) {
    SimulationResult *resA = (SimulationResult *)a;
    SimulationResult *resB = (SimulationResult *)b;
    if (resB->net_pnl > resA->net_pnl) return 1;
    if (resB->net_pnl < resA->net_pnl) return -1;
    return 0;
}

// Standalone isolated backtest engine for OpenMP workers
SimulationResult run_backtest(int lookback, double atr_mult, double rr_ratio, Candle* data, int data_size) {
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
            total_trades++;
            if (t.realized_pnl > 0) wins++;
            
            // Track Drawdown
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
    printf("[INFO] Data loaded. Commencing parallel execution across CPU threads...\n\n");

    // Grid Dimensions
    int num_lookback = 9;  // 20 to 100
    int num_atr = 11;      // 1.0 to 3.0
    int num_rr = 5;        // 1.0 to 3.0
    int total_permutations = num_lookback * num_atr * num_rr;

    SimulationResult* results = malloc(total_permutations * sizeof(SimulationResult));

    double start_time = omp_get_wtime();

    // Multithreaded Matrix Execution
    #pragma omp parallel for collapse(3) schedule(dynamic)
    for (int l = 0; l < num_lookback; l++) {
        for (int a = 0; a < num_atr; a++) {
            for (int r = 0; r < num_rr; r++) {
                
                int current_lookback = 20 + (l * 10);
                double current_atr = 1.0 + (a * 0.2);
                double current_rr = 1.0 + (r * 0.5);

                SimulationResult res = run_backtest(current_lookback, current_atr, current_rr, raw_history, count);
                
                // Deterministic flat-array indexing avoids mutex locking
                int flat_idx = l * (num_atr * num_rr) + a * num_rr + r;
                results[flat_idx] = res;
            }
        }
    }

    double end_time = omp_get_wtime();
    printf("[INFO] Matrix calculated %d permutations in %.2f seconds.\n\n", total_permutations, end_time - start_time);

    // Sort and Print Top 10
    qsort(results, total_permutations, sizeof(SimulationResult), compare_results);

    printf("===============================================================================\n");
    printf(" TOP 10 OPTIMIZED PARAMETER SETS (By Net PnL)\n");
    printf("===============================================================================\n");
    printf("Rank | Lookback | ATR Mult | R:R Ratio | Win Rate | Trades | Max DD | Net PnL\n");
    printf("-------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < 10 && i < total_permutations; i++) {
        printf("#%-3d | %-8d | %-8.1f | %-9.1f | %-7.2f%% | %-6d | %-5.2f%% | $%-.2f\n",
               i + 1,
               results[i].lookback,
               results[i].atr_mult,
               results[i].rr_ratio,
               results[i].win_rate,
               results[i].total_trades,
               results[i].max_drawdown,
               results[i].net_pnl);
    }
    printf("===============================================================================\n");

    free(results);
    free(raw_history);
    return 0;
}