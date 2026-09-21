#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bot.h"

// Calculate Simple Average True Range (ATR)
double calculate_atr(Candle* prices, int current_idx, int period) {
    if (current_idx < period) return 0.0010; 
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

// Calculate Exponential Moving Average (EMA)
double calculate_ema(Candle* prices, int current_idx, int period) {
    if (current_idx < period) return prices[current_idx].close;
    double multiplier = 2.0 / (period + 1.0);
    double ema = prices[current_idx - period].close; 
    
    for (int i = current_idx - period + 1; i <= current_idx; i++) {
        ema = (prices[i].close - ema) * multiplier + ema;
    }
    return ema;
}

// Helper to extract hour from timestamp (YYYY-MM-DD HH:MM:SS)
int extract_hour(const char* timestamp) {
    int hour = 0;
    sscanf(timestamp, "%*d-%*d-%*d %d:%*d:%*d", &hour);
    return hour;
}

// Breakout and Pullback Strategy Engine
int strategy_breakout_pullback(Candle* prices, int current_idx, StrategyParams* params, StrategyState* state, double* out_sl) {
    if (current_idx < params->lookback_period + 1 || current_idx < params->ema_period) return 0;

    double hh = prices[current_idx - params->lookback_period].high;
    double ll = prices[current_idx - params->lookback_period].low;
    
    for (int i = current_idx - params->lookback_period; i < current_idx; i++) {
        if (prices[i].high > hh) hh = prices[i].high;
        if (prices[i].low < ll) ll = prices[i].low;
    }

    double current_ema = calculate_ema(prices, current_idx, params->ema_period);

    // 1. Process active breakouts (Tighter Window: 6 candles)
    if (state->active_breakout != 0) {
        state->candles_since_breakout++;
        
        if (state->candles_since_breakout > params->max_pullback_candles) {
            state->active_breakout = 0; // Exceeded tight window, cancel setup
        } else {
            // Check for Long Pullback Entry
            if (state->active_breakout == 1) {
                // Must remain above 200 EMA during pullback
                if (prices[current_idx].close > current_ema && 
                    prices[current_idx].low <= state->broken_level && 
                    prices[current_idx].close > state->broken_level) {
                    
                    double atr = calculate_atr(prices, current_idx, params->atr_period);
                    *out_sl = prices[current_idx].low - atr; 
                    state->active_breakout = 0; 
                    return 1;
                }
            } 
            // Check for Short Pullback Entry
            else if (state->active_breakout == -1) {
                // Must remain below 200 EMA during pullback
                if (prices[current_idx].close < current_ema && 
                    prices[current_idx].high >= state->broken_level && 
                    prices[current_idx].close < state->broken_level) {
                    
                    double atr = calculate_atr(prices, current_idx, params->atr_period);
                    *out_sl = prices[current_idx].high + atr; 
                    state->active_breakout = 0; 
                    return -1;
                }
            }
        }
    }

    // 2. Scan for NEW Breakouts (Session & Macro Trend Filters applied)
    if (state->active_breakout == 0) {
        int current_hour = extract_hour(prices[current_idx].timestamp);
        
        // ONLY validate breakouts during London/NY Overlap
        if (current_hour >= params->session_start_hour && current_hour < params->session_end_hour) {
            
            // LONG Breakout: Must break Resistance AND be Above 200 EMA
            if (prices[current_idx].close > hh && prices[current_idx].close > current_ema) {
                state->active_breakout = 1;
                state->broken_level = hh;
                state->candles_since_breakout = 0;
            } 
            // SHORT Breakout: Must break Support AND be Below 200 EMA
            else if (prices[current_idx].close < ll && prices[current_idx].close < current_ema) {
                state->active_breakout = -1;
                state->broken_level = ll;
                state->candles_since_breakout = 0;
            }
        }
    }

    return 0; 
}

// Simulates the trade and subtracts Spread/Slippage costs
int execute_realistic_backtest_trade(Account* acc, Trade* trade, Candle* prices, int start_idx, StrategyParams* params) {
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
            
        double gross_pnl = pip_change * 10.0 * trade->lot_size;
        
        // Apply Spread and Slippage Penalty (e.g., 1.5 pips cost per round-trip)
        double spread_slippage_cost = params->spread_slippage_pips * 10.0 * trade->lot_size;

        if (outcome == 1) {
            trade->realized_pnl = gross_pnl - spread_slippage_cost; // Profit is reduced
        } else {
            trade->realized_pnl = -(gross_pnl + spread_slippage_cost); // Loss is increased
        }
        
        acc->current_balance += trade->realized_pnl;
    } else {
        trade->realized_pnl = 0.0; 
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