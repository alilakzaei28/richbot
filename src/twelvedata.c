#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <ctype.h>
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

// Replaces spaces with %20 for URL encoding
void url_encode_space(const char *src, char *dest) {
    while (*src) {
        if (*src == ' ') { strcpy(dest, "%20"); dest += 3; }
        else { *dest++ = *src; }
        src++;
    }
    *dest = '\0';
}

int fetch_historical_data(const char* symbol, const char* interval, Candle* out_buffer, int target_candles) {
    char cache_filename[128];
    char symbol_safe[16];
    
    // Create a safe filename (replace '/' with '_')
    strcpy(symbol_safe, symbol);
    for(int i=0; symbol_safe[i]; i++) { if(symbol_safe[i] == '/') symbol_safe[i] = '_'; }
    snprintf(cache_filename, sizeof(cache_filename), "cache_%s_%s.csv", symbol_safe, interval);

    // 1. Try Loading from Local Cache First
    FILE *cache_file = fopen(cache_filename, "r");
    if (cache_file) {
        printf("[INFO] Found local cache: %s. Loading data...\n", cache_filename);
        char line[256];
        int count = 0;
        fgets(line, sizeof(line), cache_file); // Skip header
        while (fgets(line, sizeof(line), cache_file) && count < target_candles) {
            sscanf(line, "%[^,],%lf,%lf,%lf,%lf,%lf", 
                   out_buffer[count].timestamp, &out_buffer[count].open, 
                   &out_buffer[count].high, &out_buffer[count].low, 
                   &out_buffer[count].close, &out_buffer[count].volume);
            count++;
        }
        fclose(cache_file);
        printf("[INFO] Loaded %d candles from cache.\n", count);
        return count;
    }

    // 2. Fetch from API with Pagination (Chunking)
    const char* api_key = getenv("TWELVEDATA_API_KEY");
    if (!api_key || strlen(api_key) == 0) api_key = "demo";

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    int total_fetched = 0;
    char end_date[64] = "";
    int req_size = 5000; // Max allowed per free tier request

    printf("[INFO] No cache found. Fetching %d candles via API chunks...\n", target_candles);

    while (total_fetched < target_candles) {
        struct MemoryStruct chunk;
        chunk.memory = malloc(1);
        chunk.size = 0;

        char url[512];
        if (strlen(end_date) == 0) {
            snprintf(url, sizeof(url), "https://api.twelvedata.com/time_series?symbol=%s&interval=%s&outputsize=%d&apikey=%s", 
                     symbol, interval, req_size, api_key);
        } else {
            char encoded_date[128];
            url_encode_space(end_date, encoded_date);
            snprintf(url, sizeof(url), "https://api.twelvedata.com/time_series?symbol=%s&interval=%s&outputsize=%d&apikey=%s&end_date=%s", 
                     symbol, interval, req_size, api_key, encoded_date);
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        
        if (curl_easy_perform(curl) != CURLE_OK) {
            free(chunk.memory);
            break;
        }

        cJSON *json = cJSON_Parse(chunk.memory);
        if (!json) { free(chunk.memory); break; }

        cJSON *values = cJSON_GetObjectItemCaseSensitive(json, "values");
        if (!values || !cJSON_IsArray(values)) {
            cJSON_Delete(json);
            free(chunk.memory);
            break; // No more data or API error limit reached
        }

        cJSON *item;
        int chunk_count = 0;
        char last_timestamp[64] = "";

        cJSON_ArrayForEach(item, values) {
            if (total_fetched >= target_candles) break;

            cJSON *dt = cJSON_GetObjectItemCaseSensitive(item, "datetime");
            if (cJSON_IsString(dt)) {
                strncpy(out_buffer[total_fetched].timestamp, dt->valuestring, 31);
                strcpy(last_timestamp, dt->valuestring);
            }
            out_buffer[total_fetched].open = atof(cJSON_GetObjectItemCaseSensitive(item, "open")->valuestring);
            out_buffer[total_fetched].high = atof(cJSON_GetObjectItemCaseSensitive(item, "high")->valuestring);
            out_buffer[total_fetched].low = atof(cJSON_GetObjectItemCaseSensitive(item, "low")->valuestring);
            out_buffer[total_fetched].close = atof(cJSON_GetObjectItemCaseSensitive(item, "close")->valuestring);
            out_buffer[total_fetched].volume = 0; // Optional volume parse

            total_fetched++;
            chunk_count++;
        }

        cJSON_Delete(json);
        free(chunk.memory);

        if (chunk_count == 0) break; 
        
        strcpy(end_date, last_timestamp); // Set next pagination anchor
        printf("[INFO] Fetched chunk: %d candles. Total: %d\n", chunk_count, total_fetched);
    }
    curl_easy_cleanup(curl);

    // 3. Save to Local Cache
    if (total_fetched > 0) {
        cache_file = fopen(cache_filename, "w");
        if (cache_file) {
            fprintf(cache_file, "Timestamp,Open,High,Low,Close,Volume\n");
            for (int i = 0; i < total_fetched; i++) {
                fprintf(cache_file, "%s,%.5f,%.5f,%.5f,%.5f,%.2f\n", 
                        out_buffer[i].timestamp, out_buffer[i].open, out_buffer[i].high, 
                        out_buffer[i].low, out_buffer[i].close, out_buffer[i].volume);
            }
            fclose(cache_file);
            printf("[INFO] Data cached to %s\n", cache_filename);
        }
    }
    return total_fetched;
}

void parse_live_tick(const char* json_string, LiveTick* out_tick) {
    // ... Unchanged ...
}