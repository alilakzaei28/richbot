#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bot.h"

// Calculate Simple Average True Range (ATR)
double calculate_atr(Candle* prices, int current_idx, int period) {
    if (current_idx < period) return 0.0010; // Failsafe default
    
    double sum_tr = 0.0;
    for (int i = current_idx - period + 1; i <= current_idx; i++) {
        double hl = prices[i].high - prices[i].low;
        double hc = fabs(prices[i].high - prices[i-1].close);
        double lc = fabs(prices[i].low - prices[i-1].close);
        
        double tr = hl;
        if (hc > tr) tr = hc;
        if (lc > tr) tr = lc;
        
        sum_tr += tr;
    }
    return sum_tr / period;
}

// Breakout and Pullback Strategy Engine
int strategy_breakout_pullback(Candle* prices, int current_idx, StrategyParams* params, StrategyState* state, double* out_sl) {
    // Ensure sufficient history for 50-period lookback and 14-period ATR
    if (current_idx < params->lookback_period + 1) return 0;

    // 1. Calculate 50-period Resistance (Highest High) and Support (Lowest Low) EXCLUDING current candle
    double hh = prices[current_idx - params->lookback_period].high;
    double ll = prices[current_idx - params->lookback_period].low;
    
    for (int i = current_idx - params->lookback_period; i < current_idx; i++) {
        if (prices[i].high > hh) hh = prices[i].high;
        if (prices[i].low < ll) ll = prices[i].low;
    }

    // 2. Process active breakouts (Wait max 10 candles for a pullback)
    if (state->active_breakout != 0) {
        state->candles_since_breakout++;
        
        if (state->candles_since_breakout > params->max_pullback_candles) {
            state->active_breakout = 0; // Exceeded 10 candles, reset setup
        } else {
            // Check for Long Pullback Entry
            if (state->active_breakout == 1) {
                // Low touches/drops below Broken Resistance, BUT Close remains above
                if (prices[current_idx].low <= state->broken_level && prices[current_idx].close > state->broken_level) {
                    double atr = calculate_atr(prices, current_idx, params->atr_period);
                    *out_sl = prices[current_idx].low - atr; // Stop loss exactly 1 ATR below the pullback low
                    state->active_breakout = 0; // Reset state after triggering entry
                    return 1;
                }
            } 
            // Check for Short Pullback Entry
            else if (state->active_breakout == -1) {
                // High touches/goes above Broken Support, BUT Close remains below
                if (prices[current_idx].high >= state->broken_level && prices[current_idx].close < state->broken_level) {
                    double atr = calculate_atr(prices, current_idx, params->atr_period);
                    *out_sl = prices[current_idx].high + atr; // Stop loss exactly 1 ATR above the pullback high
                    state->active_breakout = 0; // Reset state after triggering entry
                    return -1;
                }
            }
        }
    }

    // 3. Scan for NEW Breakouts
    // If no active setup exists, see if the current candle closes outside the 50-period channels
    if (state->active_breakout == 0) {
        if (prices[current_idx].close > hh) {
            state->active_breakout = 1;
            state->broken_level = hh;
            state->candles_since_breakout = 0;
        } 
        else if (prices[current_idx].close < ll) {
            state->active_breakout = -1;
            state->broken_level = ll;
            state->candles_since_breakout = 0;
        }
    }

    return 0; // No entry on this candle
}

// Simulates the trade candle by candle until TP or SL is hit.
// Returns the index of the candle where the trade closed to strictly enforce 1 order at a time.
int execute_realistic_backtest_trade(Account* acc, Trade* trade, Candle* prices, int start_idx) {
    int outcome = 0; 
    int close_idx = start_idx;

    for (int i = start_idx + 1; prices[i].close > 0.0001; i++) {
        if (trade->type == 1) { // LONG
            if (prices[i].low <= trade->stop_loss) { outcome = -1; close_idx = i; break; }
            if (prices[i].high >= trade->take_profit) { outcome = 1; close_idx = i; break; }
        } else { // SHORT
            if (prices[i].high >= trade->stop_loss) { outcome = -1; close_idx = i; break; }
            if (prices[i].low <= trade->take_profit) { outcome = 1; close_idx = i; break; }
        }
    }

    strcpy(trade->exit_time, prices[close_idx].timestamp);

    if (outcome != 0) {
        double pip_change = (outcome == 1) 
            ? fabs(trade->take_profit - trade->entry_price) * 10000.0
            : fabs(trade->entry_price - trade->stop_loss) * 10000.0;
            
        trade->realized_pnl = pip_change * 10.0 * trade->lot_size * outcome;
        acc->current_balance += trade->realized_pnl;
    } else {
        trade->realized_pnl = 0.0; // Trade never closed by the end of dataset
    }
    
    return close_idx;
}

void export_report(Trade* trades, int count, const char* filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "TradeID,Symbol,EntryTime,ExitTime,Type,Entry,StopLoss,TakeProfit,LotSize,RealizedPnL\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%d,%s,%s,%s,%s,%.5f,%.5f,%.5f,%.2f,%.2f\n",
                i + 1, trades[i].symbol, trades[i].entry_time, trades[i].exit_time,
                (trades[i].type == 1) ? "BUY" : "SELL",
                trades[i].entry_price, trades[i].stop_loss, trades[i].take_profit,
                trades[i].lot_size, trades[i].realized_pnl);
    }
    fclose(f);
}