#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bot.h"

int strategy_bollinger_bands(Candle* prices, int current_idx, StrategyParams* params) {
    if (current_idx < params->bb_period) return 0;

    double sum = 0.0;
    for (int i = 0; i < params->bb_period; i++) {
        sum += prices[current_idx - i].close;
    }
    double sma = sum / params->bb_period;

    double variance_sum = 0.0;
    for (int i = 0; i < params->bb_period; i++) {
        variance_sum += pow(prices[current_idx - i].close - sma, 2.0);
    }
    double std_dev = sqrt(variance_sum / params->bb_period);

    double upper_band = sma + (params->bb_std_dev * std_dev);
    double lower_band = sma - (params->bb_std_dev * std_dev);

    // Mean Reversion Signals
    if (prices[current_idx].close < lower_band) return 1;  // Oversold -> BUY
    if (prices[current_idx].close > upper_band) return -1; // Overbought -> SELL

    return 0;
}

void execute_realistic_backtest_trade(Account* acc, Trade* trade, Candle* prices, int start_idx) {
    int outcome = 0; // 1 for TP, -1 for SL

    // Prices array is ordered Chronological: index 0 (oldest) to count-1 (newest).
    // The forward path runs forward from start_idx + 1 onwards.
    for (int i = start_idx + 1; prices[i].close > 0.0001; i++) {
        if (trade->type == 1) { // BUY Trade
            if (prices[i].low <= trade->stop_loss) {
                outcome = -1;
                break;
            }
            if (prices[i].high >= trade->take_profit) {
                outcome = 1;
                break;
            }
        } else { // SELL Trade
            if (prices[i].high >= trade->stop_loss) {
                outcome = -1;
                break;
            }
            if (prices[i].low <= trade->take_profit) {
                outcome = 1;
                break;
            }
        }
    }

    if (outcome != 0) {
        double pip_change = 0.0;
        if (outcome == 1) {
            pip_change = fabs(trade->take_profit - trade->entry_price) * 10000.0;
            trade->realized_pnl = pip_change * 10.0 * trade->lot_size;
        } else {
            pip_change = fabs(trade->entry_price - trade->stop_loss) * 10000.0;
            trade->realized_pnl = -(pip_change * 10.0 * trade->lot_size);
        }
        acc->current_balance += trade->realized_pnl;
    } else {
        trade->realized_pnl = 0.0; // Open trade at end of data series
    }
}

void export_report(Trade* trades, int count, const char* filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("[ERROR] Cannot write report to file: %s\n", filename);
        return;
    }

    fprintf(f, "TradeID,Symbol,Type,Entry,StopLoss,TakeProfit,LotSize,RealizedPnL\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%d,%s,%s,%.5f,%.5f,%.5f,%.2f,%.2f\n",
                i + 1,
                trades[i].symbol,
                (trades[i].type == 1) ? "BUY" : "SELL",
                trades[i].entry_price,
                trades[i].stop_loss,
                trades[i].take_profit,
                trades[i].lot_size,
                trades[i].realized_pnl);
    }
    fclose(f);
    printf("[INFO] Report successfully written to %s\n", filename);
}