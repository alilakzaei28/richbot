#include <stdio.h>
#include <math.h>
#include "bot.h"

// Dynamic lot sizing based on Stop Loss distance
double calculate_lot_size(Account* acc, double entry, double stop_loss) {
    double risk_capital = acc->current_balance * acc->max_risk_pct;
    double pip_distance = fabs(entry - stop_loss) * 10000; // Multiplier for standard pairs
    
    if (pip_distance == 0) return 0.0;
    
    // $10 per pip for 1 standard lot
    double lot_size = risk_capital / (pip_distance * 10.0);
    return lot_size;
}