#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cJSON.h"
#include "bot.h"

// Define your API key
#define TD_API_KEY "e500ec9435e042c49129af10ae81c0de"

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

// Fetches REST data for backtesting (e.g., EUR/USD or XAU/USD)
int fetch_historical_data(const char* symbol, const char* interval, Candle* out_buffer, int max_candles) {
    CURL *curl = curl_easy_init();
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    char url[256];
    snprintf(url, sizeof(url), "https://api.twelvedata.com/time_series?symbol=%s&interval=%s&outputsize=%d&apikey=%s", 
             symbol, interval, max_candles, TD_API_KEY);
    
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_perform(curl);
        
        cJSON *json = cJSON_Parse(chunk.memory);
        cJSON *values = cJSON_GetObjectItem(json, "values");
        
        int count = 0;
        cJSON *item;
        cJSON_ArrayForEach(item, values) {
            if (count >= max_candles) break;
            strcpy(out_buffer[count].timestamp, cJSON_GetObjectItem(item, "datetime")->valuestring);
            out_buffer[count].open = atof(cJSON_GetObjectItem(item, "open")->valuestring);
            out_buffer[count].high = atof(cJSON_GetObjectItem(item, "high")->valuestring);
            out_buffer[count].low = atof(cJSON_GetObjectItem(item, "low")->valuestring);
            out_buffer[count].close = atof(cJSON_GetObjectItem(item, "close")->valuestring);
            count++;
        }
        
        cJSON_Delete(json);
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return count;
    }
    return 0;
}

// Parses raw websocket JSON payload into the LiveTick struct
void parse_live_tick(const char* json_string, LiveTick* out_tick) {
    cJSON *json = cJSON_Parse(json_string);
    if (!json) return;

    cJSON *event = cJSON_GetObjectItem(json, "event");
    if (event && strcmp(event->valuestring, "price") == 0) {
        strcpy(out_tick->symbol, cJSON_GetObjectItem(json, "symbol")->valuestring);
        out_tick->bid = cJSON_GetObjectItem(json, "bid")->valuedouble;
        out_tick->ask = cJSON_GetObjectItem(json, "ask")->valuedouble;
        printf("[Live Tick] %s | Bid: %.5f | Ask: %.5f\n", out_tick->symbol, out_tick->bid, out_tick->ask);
    }
    cJSON_Delete(json);
}