#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cJSON.h"
#include "bot.h"

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

int fetch_historical_data(const char* symbol, const char* interval, Candle* out_buffer, int max_candles) {
    const char* api_key = getenv("TWELVEDATA_API_KEY");
    if (api_key == NULL || strlen(api_key) == 0) {
        printf("[WARN] TWELVEDATA_API_KEY environment variable not set. Falling back to 'demo'.\n");
        api_key = "demo";
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        printf("[ERROR] Failed to initialize curl.\n");
        return 0;
    }

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    char url[512];
    snprintf(url, sizeof(url), 
             "https://api.twelvedata.com/time_series?symbol=%s&interval=%s&outputsize=%d&apikey=%s", 
             symbol, interval, max_candles, api_key);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        printf("[ERROR] curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return 0;
    }

    cJSON *json = cJSON_Parse(chunk.memory);
    if (!json) {
        printf("[ERROR] Failed to parse API JSON response.\n");
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return 0;
    }

    cJSON *status = cJSON_GetObjectItemCaseSensitive(json, "status");
    if (status && cJSON_IsString(status) && strcmp(status->valuestring, "error") == 0) {
        cJSON *message = cJSON_GetObjectItemCaseSensitive(json, "message");
        printf("[API ERROR] %s\n", message ? message->valuestring : "Unknown error");
        cJSON_Delete(json);
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return 0;
    }

    cJSON *values = cJSON_GetObjectItemCaseSensitive(json, "values");
    int count = 0;

    if (values != NULL && cJSON_IsArray(values)) {
        cJSON *item;
        cJSON_ArrayForEach(item, values) {
            if (count >= max_candles) break;

            cJSON *datetime = cJSON_GetObjectItemCaseSensitive(item, "datetime");
            cJSON *open_val = cJSON_GetObjectItemCaseSensitive(item, "open");
            cJSON *high_val = cJSON_GetObjectItemCaseSensitive(item, "high");
            cJSON *low_val = cJSON_GetObjectItemCaseSensitive(item, "low");
            cJSON *close_val = cJSON_GetObjectItemCaseSensitive(item, "close");
            cJSON *vol_val = cJSON_GetObjectItemCaseSensitive(item, "volume");

            if (cJSON_IsString(datetime)) {
                strncpy(out_buffer[count].timestamp, datetime->valuestring, sizeof(out_buffer[count].timestamp) - 1);
            }
            if (cJSON_IsString(open_val))  out_buffer[count].open = atof(open_val->valuestring);
            if (cJSON_IsString(high_val))  out_buffer[count].high = atof(high_val->valuestring);
            if (cJSON_IsString(low_val))   out_buffer[count].low = atof(low_val->valuestring);
            if (cJSON_IsString(close_val)) out_buffer[count].close = atof(close_val->valuestring);
            if (cJSON_IsString(vol_val))   out_buffer[count].volume = atof(vol_val->valuestring);

            count++;
        }
    }

    cJSON_Delete(json);
    curl_easy_cleanup(curl);
    free(chunk.memory);
    return count;
}

void parse_live_tick(const char* json_string, LiveTick* out_tick) {
    cJSON *json = cJSON_Parse(json_string);
    if (!json) return;

    cJSON *event = cJSON_GetObjectItemCaseSensitive(json, "event");
    if (event && cJSON_IsString(event) && strcmp(event->valuestring, "price") == 0) {
        cJSON *symbol = cJSON_GetObjectItemCaseSensitive(json, "symbol");
        cJSON *bid = cJSON_GetObjectItemCaseSensitive(json, "bid");
        cJSON *ask = cJSON_GetObjectItemCaseSensitive(json, "ask");

        if (cJSON_IsString(symbol)) {
            strncpy(out_tick->symbol, symbol->valuestring, sizeof(out_tick->symbol) - 1);
        }
        if (cJSON_IsNumber(bid)) out_tick->bid = bid->valuedouble;
        if (cJSON_IsNumber(ask)) out_tick->ask = ask->valuedouble;

        printf("[Live Tick] %s | Bid: %.5f | Ask: %.5f\n", 
               out_tick->symbol, out_tick->bid, out_tick->ask);
    }
    cJSON_Delete(json);
}