#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bot.h"

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

int extract_hour(const char* timestamp) {
    int hour = 0;
    sscanf(timestamp, "%*d-%*d-%*d %d:%*d:%*d", &hour);
    return hour;
}

int strategy_fakeout_reversal(Candle* prices, int current_idx, StrategyParams* params, StrategyState* state, double* out_sl) {
    if (current_idx < params->lookback_period + 1) return 0;

    double hh = prices[current_idx - params->lookback_period].high;
    double ll = prices[current_idx - params->lookback_period].low;
    
    for (int i = current_idx - params->lookback_period; i < current_idx; i++) {
        if (prices[i].high > hh) hh = prices[i].high;
        if (prices[i].low < ll) ll = prices[i].low;
    }

    int current_hour = extract_hour(prices[current_idx].timestamp);
    
    if (current_hour >= params->session_start_hour && current_hour < params->session_end_hour) {
        
        // SHORT Entry
        if (prices[current_idx].high > hh && prices[current_idx].close < hh) {
            double atr = calculate_atr(prices, current_idx, params->atr_period);
            *out_sl = prices[current_idx].high + (params->atr_multiplier * atr); 
            state->active_breakout = 0; 
            return -1; 
        }
        
        // LONG Entry
        if (prices[current_idx].low < ll && prices[current_idx].close > ll) {
            double atr = calculate_atr(prices, current_idx, params->atr_period);
            *out_sl = prices[current_idx].low - (params->atr_multiplier * atr); 
            state->active_breakout = 0;
            return 1; 
        }
    }

    return 0; 
}

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

    // Assign the exact exit coordinate for the CSV
    if (outcome == 1) {
        trade->exit_price = trade->take_profit;
    } else if (outcome == -1) {
        trade->exit_price = trade->stop_loss;
    } else {
        trade->exit_price = 0.0;
    }

    if (outcome != 0) {
        double pip_change = (outcome == 1) 
            ? fabs(trade->take_profit - trade->entry_price) * 10000.0
            : fabs(trade->entry_price - trade->stop_loss) * 10000.0;
            
        double gross_pnl = pip_change * 10.0 * trade->lot_size;
        double spread_slippage_cost = params->spread_slippage_pips * 10.0 * trade->lot_size;

        if (outcome == 1) {
            trade->realized_pnl = gross_pnl - spread_slippage_cost; 
        } else {
            trade->realized_pnl = -(gross_pnl + spread_slippage_cost); 
        }
        
        acc->current_balance += trade->realized_pnl;
    } else {
        trade->realized_pnl = 0.0; 
    }
    
    return close_idx;
}