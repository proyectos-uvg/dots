/**
 * @file scores.cpp
 * @brief Lectura/escritura de scores.txt (sin ncurses).
 *
 * Formato por línea: nombre|puntaje
 * Líneas antiguas solo numéricas se cargan como "Jugador".
 */

#include "scores.h"
#include "game_state.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <vector>

static const char* SCORES_FILE = "scores.txt";
static const char* DEFAULT_NAME = "Jugador";

static pthread_mutex_t     scores_file_mutex = PTHREAD_MUTEX_INITIALIZER;
static std::vector<ScoreEntry> scores_cache;
static bool                scores_cache_valid = false;

static void sanitize_name(const char* in, char* out, size_t out_size)
{
    size_t j = 0;

    if (in == NULL || out == NULL || out_size == 0)
        return;

    while (*in != '\0' && j + 1 < out_size) {
        unsigned char c = (unsigned char)*in++;

        if (c == '|' || c == '\n' || c == '\r' || c == '\t')
            continue;

        if (isprint(c))
            out[j++] = (char)c;
    }

    while (j > 0 && out[j - 1] == ' ')
        j--;

    out[j] = '\0';

    if (j == 0) {
        strncpy(out, DEFAULT_NAME, out_size - 1);
        out[out_size - 1] = '\0';
    }
}

static bool parse_legacy_score(const char* line, int* score_out)
{
    char* end = NULL;
    long value;

    while (*line != '\0' && isspace((unsigned char)*line))
        line++;

    if (*line == '\0')
        return false;

    value = strtol(line, &end, 10);

    if (end == line)
        return false;

    while (*end != '\0' && isspace((unsigned char)*end))
        end++;

    if (*end != '\0')
        return false;

    if (value < 0 || value > 9999999L)
        return false;

    *score_out = (int)value;
    return true;
}

static bool parse_score_line(const char* line, ScoreEntry* out)
{
    char buffer[128];
    char* sep;
    int   score = 0;

    if (line == NULL || out == NULL)
        return false;

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        buffer[--len] = '\0';

    if (buffer[0] == '\0')
        return false;

    sep = strchr(buffer, '|');
    if (sep != NULL) {
        *sep = '\0';
        char* score_str = sep + 1;

        sanitize_name(buffer, out->name, sizeof(out->name));

        if (!parse_legacy_score(score_str, &score))
            return false;

        out->score = score;
        return true;
    }

    if (!parse_legacy_score(buffer, &score))
        return false;

    strncpy(out->name, DEFAULT_NAME, sizeof(out->name) - 1);
    out->name[sizeof(out->name) - 1] = '\0';
    out->score = score;
    return true;
}

static void sort_scores_desc(void)
{
    std::sort(scores_cache.begin(), scores_cache.end(),
              [](const ScoreEntry& a, const ScoreEntry& b) {
                  return a.score > b.score;
              });
}

static bool read_scores_from_disk(void)
{
    scores_cache.clear();

    FILE* file = fopen(SCORES_FILE, "r");
    if (file == NULL)
        return true;

    char line[128];

    while (fgets(line, sizeof(line), file) != NULL) {
        ScoreEntry entry;

        if (parse_score_line(line, &entry))
            scores_cache.push_back(entry);
    }

    if (ferror(file) != 0) {
        fclose(file);
        scores_cache.clear();
        return false;
    }

    fclose(file);
    sort_scores_desc();
    return true;
}

static bool write_scores_to_disk(void)
{
    FILE* file = fopen(SCORES_FILE, "w");
    if (file == NULL)
        return false;

    for (size_t i = 0; i < scores_cache.size(); i++) {
        if (fprintf(file, "%s|%d\n",
                    scores_cache[i].name,
                    scores_cache[i].score) < 0)
        {
            fclose(file);
            return false;
        }
    }

    if (fflush(file) != 0) {
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

static void refresh_session_high_score(void)
{
    if (scores_cache.empty())
        return;

    if (scores_cache.front().score > g_state.high_score)
        g_state.high_score = scores_cache.front().score;
}

void load_scores(void)
{
    pthread_mutex_lock(&scores_file_mutex);

    scores_cache_valid = read_scores_from_disk();
    refresh_session_high_score();

    pthread_mutex_unlock(&scores_file_mutex);
}

bool save_score(const char* name, int score)
{
    ScoreEntry entry;

    if (name == NULL || score < 0)
        return false;

    sanitize_name(name, entry.name, sizeof(entry.name));
    entry.score = score;

    pthread_mutex_lock(&scores_file_mutex);

    if (!scores_cache_valid) {
        if (!read_scores_from_disk()) {
            pthread_mutex_unlock(&scores_file_mutex);
            return false;
        }
        scores_cache_valid = true;
    }

    scores_cache.push_back(entry);
    sort_scores_desc();

    bool ok = write_scores_to_disk();

    if (ok)
        refresh_session_high_score();

    pthread_mutex_unlock(&scores_file_mutex);
    return ok;
}

int scores_display_count(void)
{
    int n = 0;

    pthread_mutex_lock(&scores_file_mutex);

    if (!scores_cache_valid)
        read_scores_from_disk();

    n = (int)scores_cache.size();
    if (n > SCORES_DISPLAY_COUNT)
        n = SCORES_DISPLAY_COUNT;

    pthread_mutex_unlock(&scores_file_mutex);
    return n;
}

bool scores_display_entry(int index, ScoreEntry* out)
{
    bool ok = false;

    if (out == NULL)
        return false;

    pthread_mutex_lock(&scores_file_mutex);

    if (!scores_cache_valid)
        read_scores_from_disk();

    if (index >= 0 && index < (int)scores_cache.size() &&
        index < SCORES_DISPLAY_COUNT)
    {
        *out = scores_cache[(size_t)index];
        ok   = true;
    }

    pthread_mutex_unlock(&scores_file_mutex);
    return ok;
}

void scores_prepare_match_end(int score)
{
    g_state.last_score      = score;
    g_state.player_name_len = 0;
    g_state.player_name_input[0] = '\0';
    g_state.score_saved     = false;

    if (score > g_state.high_score)
        g_state.high_score = score;

}

bool scores_commit_with_name(const char* name, int score)
{
    if (g_state.score_saved)
        return true;

    if (!save_score(name, score))
        return false;

    sanitize_name(name, g_state.last_player_name,
                  sizeof(g_state.last_player_name));
    g_state.last_score  = score;
    g_state.score_saved = true;
    return true;
}

void scores_reset_match_flag(void)
{
    g_state.score_saved     = false;
    g_state.player_name_len = 0;
    g_state.player_name_input[0] = '\0';
}
