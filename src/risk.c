#include <stdio.h>
#include <math.h>
#include "bot.h"

double calculate_lot_size(Account* acc, double entry, double stop_loss) {
    if (acc->use_fixed_lot == 1) {
        return acc->fixed_lot_size;
    }

    // 1% of total account equity
    double risk_capital = acc->current_balance * acc->max_risk_pct;
    double pip_distance = fabs(entry - stop_loss) * 10000.0;

    if (pip_distance <= 0.0001) return 0.0;

    // Based on standard $10 risk per pip per standard lot for major pairs
    double lot_size = risk_capital / (pip_distance * 10.0);

    // Floor rounding to 2 decimal places to match micro-lot broker standards
    lot_size = floor(lot_size * 100.0) / 100.0;

    return (lot_size < 0.01) ? 0.0 : lot_size;
}