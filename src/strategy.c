#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bot.h"

// Helper: Calculate Exponential Moving Average
double calculate_ema(Candle* prices, int current_idx, int period) {
    double multiplier = 2.0 / (period + 1.0);
    double ema = prices[current_idx - period].close; 
    
    for (int i = current_idx - period + 1; i <= current_idx; i++) {
        ema = (prices[i].close - ema) * multiplier + ema;
    }
    return ema;
}

// Helper: Calculate Wilder's Smoothed Relative Strength Index (RSI)
double calculate_rsi(Candle* prices, int current_idx, int period) {
    int lookback = period * 2; 
    if (current_idx < lookback) return 50.0; 
    
    double avg_gain = 0.0, avg_loss = 0.0;
    int start = current_idx - lookback;
    
    // Initial SMA for the first block
    for (int i = start + 1; i <= start + period; i++) {
        double change = prices[i].close - prices[i-1].close;
        if (change > 0) avg_gain += change;
        else avg_loss -= change;
    }
    avg_gain /= period;
    avg_loss /= period;
    
    // Wilder's Smoothing for the remaining candles
    for (int i = start + period + 1; i <= current_idx; i++) {
        double change = prices[i].close - prices[i-1].close;
        double gain = (change > 0) ? change : 0.0;
        double loss = (change < 0) ? -change : 0.0;
        
        avg_gain = ((avg_gain * (period - 1)) + gain) / period;
        avg_loss = ((avg_loss * (period - 1)) + loss) / period;
    }
    
    if (avg_loss == 0.0) return 100.0;
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

// The Core Strategy Engine
int strategy_trend_pullback(Candle* prices, int current_idx, StrategyParams* params) {
    // Ensure we have enough historical data to prime the EMA and RSI
    if (current_idx < (params->trend_ema_period + 10)) return 0;

    double ema_trend = calculate_ema(prices, current_idx, params->trend_ema_period);
    double rsi = calculate_rsi(prices, current_idx, params->rsi_period);

    // Rule 1: Only Buy in an Uptrend (Price > EMA)
    if (prices[current_idx].close > ema_trend) {
        // Entry: RSI indicates a temporary dip (oversold)
        if (rsi < params->rsi_oversold) {
            return 1; // BUY SIGNAL
        }
    }
    // Rule 2: Only Sell in a Downtrend (Price < EMA)
    else if (prices[current_idx].close < ema_trend) {
        // Entry: RSI indicates a temporary rally (overbought)
        if (rsi > params->rsi_overbought) {
            return -1; // SELL SIGNAL
        }
    }

    return 0;
}

void execute_realistic_backtest_trade(Account* acc, Trade* trade, Candle* prices, int start_idx) {
    int outcome = 0; 

    for (int i = start_idx + 1; prices[i].close > 0.0001; i++) {
        if (trade->type == 1) { 
            if (prices[i].low <= trade->stop_loss) { outcome = -1; break; }
            if (prices[i].high >= trade->take_profit) { outcome = 1; break; }
        } else { 
            if (prices[i].high >= trade->stop_loss) { outcome = -1; break; }
            if (prices[i].low <= trade->take_profit) { outcome = 1; break; }
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
        trade->realized_pnl = 0.0; 
    }
}

void export_report(Trade* trades, int count, const char* filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "TradeID,Symbol,Type,Entry,StopLoss,TakeProfit,LotSize,RealizedPnL\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%d,%s,%s,%.5f,%.5f,%.5f,%.2f,%.2f\n",
                i + 1, trades[i].symbol, (trades[i].type == 1) ? "BUY" : "SELL",
                trades[i].entry_price, trades[i].stop_loss, trades[i].take_profit,
                trades[i].lot_size, trades[i].realized_pnl);
    }
    fclose(f);
}