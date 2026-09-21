#ifndef CSV_EXPORTER_H
#define CSV_EXPORTER_H

#include "bot.h"

void export_trades_to_csv(const char* filepath, Trade* trades, int trade_count);

#endif