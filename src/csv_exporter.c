#include <stdio.h>
#include "csv_exporter.h"

void export_trades_to_csv(const char* filepath, Trade* trades, int trade_count) {
    FILE *f = fopen(filepath, "w");
    if (!f) {
        printf("[ERROR] Cannot write to %s. Ensure the directory exists.\n", filepath);
        return;
    }

    // Write the requested headers
    fprintf(f, "Trade_ID,Type,Entry_Time,Entry_Price,Stop_Loss,Take_Profit,Exit_Time,Exit_Price,Net_PnL\n");
    
    for (int i = 0; i < trade_count; i++) {
        fprintf(f, "%d,%s,%s,%.5f,%.5f,%.5f,%s,%.5f,%.2f\n",
                i + 1,
                (trades[i].type == 1) ? "Long" : "Short",
                trades[i].entry_time,
                trades[i].entry_price,
                trades[i].stop_loss,
                trades[i].take_profit,
                trades[i].exit_time,
                trades[i].exit_price,
                trades[i].realized_pnl);
    }
    
    fclose(f);
    printf("[INFO] Successfully exported %d trades to %s\n", trade_count, filepath);
}