#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* -------------------------
   Simple Stock Analyzer (integrated)
   - Loads CSV (7 days per company)
   - Individual company analysis (moving avg, trend, small graph, best buy/sell)
   - Compare two companies: moving avg, performance (%), volatility, trend & suggestion, best buy/sell
   - Recent searches cache (FIFO)
   ------------------------- */

#define MAX_COMP 100
#define NAME_LEN 50
#define DAYS 7
#define CACHE_SIZE 5

/* A company with name and last DAYS prices */
typedef struct {
    char name[NAME_LEN];
    double price[DAYS];
} Company;

/* Global storage */
Company companies[MAX_COMP];
int count = 0;            /* number of companies loaded */
int selected = -1;        /* index of company selected in sub-menu */

/*FIFO queue for recent cache*/
typedef struct queue{
    int rear, front;
    int length;
    char* array[CACHE_SIZE];
} cache;

void enq_cache(cache *q, char *str); // prototype

/* ---------------- Helpers (very simple) ---------------- */

void remove_nl(char *s) {
    int i = 0;
    while (s[i] != '\0') {
        if (s[i] == '\n' || s[i] == '\r') { s[i] = '\0'; break; }
        i++;
    }
}

void trim(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) { s[len - 1] = '\0'; len--; }
}

void lower_copy(const char *src, char *dst, int max) {
    int i = 0;
    for (; src[i] != '\0' && i < max - 1; i++) dst[i] = (char) tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/* ---------------- Load CSV ----------------
   Expect file with header, then lines like:
   Company, p1, p2, p3, p4, p5, p6, p7
*/
int load_data(const char *file) {
    FILE *f = fopen(file, "r");
    char line[512];
    int c = 0;

    if (!f) {
        printf("\n[ERR] Cannot open %s\n", file);
        return 0;
    }

    if (!fgets(line, sizeof(line), f)) { fclose(f); printf("\n[ERR] Empty file\n"); return 0; }

    while (fgets(line, sizeof(line), f) != NULL) {
        char *tok;
        int d;

        if (c >= MAX_COMP) { printf("\n[WARN] Max companies reached\n"); break; }

        remove_nl(line);
        tok = strtok(line, ",");
        if (!tok) continue;

        strncpy(companies[c].name, tok, NAME_LEN - 1);
        companies[c].name[NAME_LEN - 1] = '\0';
        trim(companies[c].name);

        for (d = 0; d < DAYS; d++) {
            tok = strtok(NULL, ",");
            if (!tok) {
                printf("[WARN] Not enough prices for %s. Skipping.\n", companies[c].name);
                break;
            }
            companies[c].price[d] = atof(tok);
        }

        if (d == DAYS) c++;
    }

    fclose(f);
    count = c;
    printf("[OK] Loaded %d companies\n", count);
    return 1;
}

/* ---------------- Simple Menus ---------------- */

void show_main_menu(void) {
    printf("\n==== Stock Tool ====\n");
    printf("1. Load data\n");
    printf("2. Search company & analyze\n");
    printf("3. Top K gainers & losers\n");
    printf("4. Filter stocks by trend/class\n");
    printf("5. Compare two companies\n");
    printf("6. Save report (placeholder)\n");
    printf("7. Recent searches\n");
    printf("8. Plot with gnuplot (placeholder)\n");
    printf("9. Exit\n");
    printf("--------------------\n");
}

void show_company_menu(void) {
    const char *name = (selected != -1) ? companies[selected].name : "N/A";
    printf("\n--- Analyze %s ---\n", name);
    printf("1. Show data\n");
    printf("2. Moving average (last k days)\n");
    printf("3. Trend (up/down/mixed/flat)\n");
    printf("4. Small text graph\n");
    printf("5. Best buy/sell days\n");
    printf("6. Back to main\n");
    printf("-------------------\n");
}

/* ---------------- Search (supports spaces & case-insensitive) ---------------- */
int find_company(cache *q) {
    char line[256], low_input[NAME_LEN], low_name[NAME_LEN];

    /* clear leftover newline */
    while (getchar() != '\n' && !feof(stdin));

    printf("Enter company name: ");
    if (!fgets(line, sizeof(line), stdin)) return 0;
    remove_nl(line);
    trim(line);
    if (line[0] == '\0') { printf("[ERR] Empty name\n"); return 0; }

    lower_copy(line, low_input, sizeof(low_input));

    for (int i = 0; i < count; i++) {
        lower_copy(companies[i].name, low_name, sizeof(low_name));
        if (strcmp(low_name, low_input) == 0) {
            selected = i;
            printf("[OK] Found %s (index %d)\n", companies[i].name, i);

            enq_cache(q, companies[i].name); //adding cache line

            return 1;
        }
    }

    printf("[ERR] %s not found\n", line);
    selected = -1;
    return 0;
}

/* ---------------- Sub-menu operations ---------------- */

/* Show the 7 prices for selected company */
void show_data(void) {
    if (selected < 0) { printf("[ERR] No company selected\n"); return; }
    Company *c = &companies[selected];
    printf("\n%s prices (oldest -> newest):\n", c->name);
    for (int i = 0; i < DAYS; i++) printf(" Day %d: %.2f\n", i + 1, c->price[i]);
}

/* Moving average of last k days for a given company */
double moving_average_for_company(const Company *c, int k) {
    if (k < 1) return 0.0;
    if (k > DAYS) k = DAYS;
    double sum = 0.0;
    int last = DAYS - 1;
    int start = last - (k - 1);
    for (int i = start; i <= last; i++) sum += c->price[i];
    return sum / k;
}

void moving_average(void) {
    if (selected < 0) { printf("[ERR] No company selected\n"); return; }
    char buf[64];
    int k;

    printf("Enter k (1..%d): ", DAYS);
    if (!fgets(buf, sizeof(buf), stdin)) return;
    if (sscanf(buf, "%d", &k) != 1) { printf("[ERR] Bad number\n"); return; }
    if (k < 1 || k > DAYS) { printf("[ERR] k out of range\n"); return; }

    Company *c = &companies[selected];
    double avg = moving_average_for_company(c, k);
    printf("Moving average (last %d days) for %s = %.2f\n", k, c->name, avg);
}

/* Determine simple trend: Upward, Downward, Flat, Mixed */
void trend(void) {
    if (selected < 0) { printf("[ERR] No company selected\n"); return; }
    Company *c = &companies[selected];
    int up = 0, down = 0, same = 0;

    for (int i = 1; i < DAYS; i++) {
        if (c->price[i] > c->price[i-1]) up++;
        else if (c->price[i] < c->price[i-1]) down++;
        else same++;
    }

    printf("Trend for %s: ", c->name);
    if (up == DAYS - 1) printf("Strong Upward\n");
    else if (down == DAYS - 1) printf("Strong Downward\n");
    else if (same == DAYS - 1) printf("Flat\n");
    else if (up > down) printf("Overall Upward\n");
    else if (down > up) printf("Overall Downward\n");
    else printf("Mixed\n");
}

/* Small ASCII graph scaled to 40 chars */
void small_graph(void) {
    if (selected < 0) {
        printf("[ERR] No company selected\n");
        return;
    }

    Company *c = &companies[selected];

    // Find min and max price to scale properly
    double min = c->price[0], max = c->price[0];
    for (int i = 1; i < DAYS; i++) {
        if (c->price[i] < min) min = c->price[i];
        if (c->price[i] > max) max = c->price[i];
    }

    printf("\n%s small graph:\n", c->name);

    for (int i = 0; i < DAYS; i++) {
        int bars = 0;

        if (max > min) {
            bars = (int)((c->price[i] - min) / (max - min) * 40.0);
        }

        // ⭐ Ensure at least ONE '#'
        if (bars < 1) bars = 6;

        printf("Day %d: %6.2f | ", i + 1, c->price[i]);
        for (int b = 0; b < bars; b++) putchar('#');
        putchar('\n');
    }
}

/* Best buy day and best sell day (buy before sell) */
void best_buy_sell(void) {
    if (selected < 0) { printf("[ERR] No company selected\n"); return; }
    Company *c = &companies[selected];

    double min_price = c->price[0];
    int min_day = 0;
    double best_profit = 0.0;
    int buy_day = 0, sell_day = 0;

    for (int i = 1; i < DAYS; i++) {
        double profit = c->price[i] - min_price;
        if (profit > best_profit) {
            best_profit = profit;
            buy_day = min_day;
            sell_day = i;
        }
        if (c->price[i] < min_price) {
            min_price = c->price[i];
            min_day = i;
        }
    }

    if (best_profit <= 0.0) {
        printf("No profitable buy-sell found in this period for %s.\n", c->name);
    } else {
        printf("Buy day: %d at %.2f\n", buy_day + 1, c->price[buy_day]);
        printf("Sell day: %d at %.2f\n", sell_day + 1, c->price[sell_day]);
        printf("Profit: %.2f\n", best_profit);
    }
}

/* ---------------- Placeholder functions (kept simple) ---------------- */

/* Compute total profit for a company: newest - oldest */
double company_profit(int idx) {
    return companies[idx].price[DAYS - 1] - companies[idx].price[0];
}

/* Compute simple trend score: up - down (positive = more ups) */
int company_trend_score(int idx) {
    Company *c = &companies[idx];
    int up = 0, down = 0;
    for (int i = 1; i < DAYS; i++) {
        if (c->price[i] > c->price[i - 1]) up++;
        else if (c->price[i] < c->price[i - 1]) down++;
    }
    return up - down;
}

/* Return a human-friendly trend label (same logic as `trend()` prints) */
const char *trend_label_for(int idx) {
    Company *c = &companies[idx];
    int up = 0, down = 0, same = 0;
    for (int i = 1; i < DAYS; i++) {
        if (c->price[i] > c->price[i - 1]) up++;
        else if (c->price[i] < c->price[i - 1]) down++;
        else same++;
    }

    if (up == DAYS - 1) return "Strong Upward";
    if (down == DAYS - 1) return "Strong Downward";
    if (same == DAYS - 1) return "Flat";
    if (up > down) return "Overall Upward";
    if (down > up) return "Overall Downward";
    return "Mixed";
}

/* Top K performers implementation
   - Prompts user for K
   - Shows Top K Gainers (highest profit) and Top K Losers (lowest profit)
*/

/* Performance entry used for sorting (profit only) */
typedef struct {
    int idx;
    double profit;
} Perf;

/* Comparator for qsort: descending by profit */
static int perf_cmp_desc(const void *a, const void *b) {
    const Perf *pa = (const Perf *) a;
    const Perf *pb = (const Perf *) b;
    if (pa->profit < pb->profit) return 1;
    if (pa->profit > pb->profit) return -1;
    return 0;
}

void top_k(cache *q) {
    if (count == 0) { printf("[WARN] No data loaded. Load CSV first.\n"); return; }

    char buf[64];
    int K;
    printf("Enter K companies to display (1-%d): ", count);
    if (!fgets(buf, sizeof(buf), stdin)) return;
    if (sscanf(buf, "%d", &K) != 1) { printf("[ERR] Bad number\n"); return; }
    if (K < 1) K = 1;
    if (K > count) K = count;

    /* Build performance array */
    Perf *arr = (Perf *) malloc(sizeof(Perf) * count);
    if (!arr) { printf("[ERR] Out of memory\n"); return; }

    for (int i = 0; i < count; i++) {
        arr[i].idx = i;
        arr[i].profit = company_profit(i);
    }

    /* Sort descending by profit (gain -> loss). */
    qsort(arr, count, sizeof(Perf), perf_cmp_desc);

    printf("\n=== Top %d Gainers ===\n", K);
    for (int i = 0; i < K; i++) {
        int id = arr[i].idx;
        double p = arr[i].profit;
        printf("%2d. %s | Profit: %+6.2f\n", i + 1, companies[id].name, p);
        enq_cache(q, companies[id].name);
    }

    printf("\n=== Top %d Losers ===\n", K);
    /* Losers are at the end of the sorted array (lowest profits). Show them in worst->less-worst order */
    for (int i = 0; i < K; i++) {
        int pos = count - 1 - i;
        int id = arr[pos].idx;
        double p = arr[pos].profit;
        printf("%2d. %s | Profit: %+6.2f\n", i + 1, companies[id].name, p);
        enq_cache(q, companies[id].name);
    }

    free(arr);
}

/* ---------------- New helpers for compare-two (operate on any company) ---------------- */

/* percent change over the period */
double percent_change_for(const Company *c) {
    double first = c->price[0];
    double last = c->price[DAYS - 1];
    if (first == 0.0) return 0.0;
    return (last - first) / first * 100.0;
}

/* volatility: std dev of daily returns (in percentage points) */
double volatility_for(const Company *c) {
    if (DAYS < 2) return 0.0;
    int m = DAYS - 1;
    double mean = 0.0;
    double returns[DAYS];
    for (int i = 1; i < DAYS; ++i) {
        returns[i-1] = (c->price[i] - c->price[i-1]) / c->price[i-1];
        mean += returns[i-1];
    }
    mean /= m;
    double var = 0.0;
    for (int i = 0; i < m; ++i) {
        double diff = returns[i] - mean; var += diff * diff;
    }
    var /= m;
    return sqrt(var) * 100.0; // percentage points
}

/* linear trend slope (price units per day) */
double trend_slope_for(const Company *c) {
    double sumx=0, sumy=0, sumxy=0, sumx2=0;
    for (int i=0;i<DAYS;i++) {
        double x = i;
        double y = c->price[i];
        sumx += x; sumy += y; sumxy += x*y; sumx2 += x*x;
    }
    double n = DAYS;
    double denom = n*sumx2 - sumx*sumx;
    if (fabs(denom) < 1e-12) return 0.0;
    double slope = (n*sumxy - sumx*sumy) / denom;
    return slope;
}

/* best buy/sell for arbitrary company */
void best_buy_sell_for(const Company *c, int *buy_idx, int *sell_idx, double *profit) {
    double min_price = c->price[0];
    int min_day = 0;
    *profit = 0.0;
    *buy_idx = 0; *sell_idx = 0;
    for (int i = 1; i < DAYS; i++) {
        double p = c->price[i];
        double cur_profit = p - min_price;
        if (cur_profit > *profit) {
            *profit = cur_profit;
            *buy_idx = min_day; *sell_idx = i;
        }
        if (p < min_price) { min_price = p; min_day = i; }
    }
}

/* simple combined suggestion (weights: perf 0.5, slope 0.3, vol 0.2) */
int which_is_better(const Company *A, const Company *B) {
    double perfA = percent_change_for(A); double perfB = percent_change_for(B);
    double slopeA = trend_slope_for(A); double slopeB = trend_slope_for(B);
    double volA = volatility_for(A); double volB = volatility_for(B);

    // normalize each metric between the two
    double pmin = fmin(perfA, perfB), pmax = fmax(perfA, perfB);
    double pA = (pmax - pmin < 1e-9) ? 0.5 : (perfA - pmin) / (pmax - pmin);
    double pB = (pmax - pmin < 1e-9) ? 0.5 : (perfB - pmin) / (pmax - pmin);

    double smin = fmin(slopeA, slopeB), smax = fmax(slopeA, slopeB);
    double sA = (smax - smin < 1e-12) ? 0.5 : (slopeA - smin) / (smax - smin);
    double sB = (smax - smin < 1e-12) ? 0.5 : (slopeB - smin) / (smax - smin);

    double vmin = fmin(volA, volB), vmax = fmax(volA, volB);
    double vA = (vmax - vmin < 1e-9) ? 0.5 : (vmax - volA) / (vmax - vmin);
    double vB = (vmax - vmin < 1e-9) ? 0.5 : (vmax - volB) / (vmax - vmin);

    const double W_PERF = 0.5, W_SLOPE = 0.3, W_VOL = 0.2;
    double scoreA = W_PERF * pA + W_SLOPE * sA + W_VOL * vA;
    double scoreB = W_PERF * pB + W_SLOPE * sB + W_VOL * vB;

    if (scoreA > scoreB + 1e-9) return 0; // A better
    if (scoreB > scoreA + 1e-9) return 1; // B better
    return -1; // tie
}

/* find company by name (case-insensitive, exact or substring) */
int find_by_name(const char *name) {
    char low_input[NAME_LEN], low_name[NAME_LEN];
    lower_copy(name, low_input, sizeof(low_input));
    for (int i=0;i<count;i++) {
        lower_copy(companies[i].name, low_name, sizeof(low_name));
        if (strcmp(low_name, low_input) == 0) return i;
        if (strstr(low_name, low_input) != NULL) return i; // substring match
    }
    return -1;
}

/* ---------------- Updated compare_two (integrated) ---------------- */
void compare_two(void) {
    char line[256];
    char c1[NAME_LEN], c2[NAME_LEN];

    /* clear leftover newline */
    while (getchar() != '\n' && !feof(stdin));

    printf("Enter Company 1 name: ");
    if (!fgets(line, sizeof(line), stdin)) return;
    remove_nl(line); trim(line); strncpy(c1, line, NAME_LEN-1); c1[NAME_LEN-1]='\0';

    printf("Enter Company 2 name: ");
    if (!fgets(line, sizeof(line), stdin)) return;
    remove_nl(line); trim(line); strncpy(c2, line, NAME_LEN-1); c2[NAME_LEN-1]='\0';

    int idx1 = find_by_name(c1);
    int idx2 = find_by_name(c2);
    if (idx1 < 0) { printf("[ERR] Company '%s' not found\n", c1); return; }
    if (idx2 < 0) { printf("[ERR] Company '%s' not found\n", c2); return; }

    const Company *A = &companies[idx1];
    const Company *B = &companies[idx2];

    printf("Comparing '%s' (idx %d)  vs  '%s' (idx %d)\n", A->name, idx1, B->name, idx2);

    int sub_choice = 0;
    char buf[128];
    while (1) {
        printf("\nCompare menu:\n");
        printf("1. Moving average of last n days\n");
        printf("2. Performance (which gained/lost more %%)\n");
        printf("3. Volatility (which is more stable)\n");
        printf("4. Trends + suggestion (which looks better)\n");
        printf("5. Best buy & sell days (single-transaction)\n");
        printf("6. Back\n");
        printf("Choice: ");
        if (!fgets(buf, sizeof(buf), stdin)) continue;
        if (sscanf(buf, "%d", &sub_choice) != 1) { printf("[ERR] Enter a number\n"); continue; }
        if (sub_choice == 6) break;

        switch (sub_choice) {
            case 1: {
                int n;
                printf("Enter n (1..%d): ", DAYS);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                if (sscanf(buf, "%d", &n) != 1) { printf("[ERR] invalid n\n"); break; }
                double maA = moving_average_for_company(A, n);
                double maB = moving_average_for_company(B, n);
                printf("MA(%d) %s = %.4f\n", n, A->name, maA);
                printf("MA(%d) %s = %.4f\n", n, B->name, maB);
                if (fabs(maA - maB) < 1e-9) printf("Same moving average.\n");
                else printf("%s has %s MA (%.4f vs %.4f)\n",
                     (maA > maB) ? A->name : B->name,
                     (maA > maB) ? "higher" : "lower", maA, maB);
                break;
            }
            case 2: {
                double pA = percent_change_for(A);
                double pB = percent_change_for(B);
                printf("%s: Percent change over period = %.4f%%\n", A->name, pA);
                printf("%s: Percent change over period = %.4f%%\n", B->name, pB);
                if (fabs(pA - pB) < 1e-9) printf("Both performed equally.\n");
                else printf("%s performed better (%+.2f%% vs %+.2f%%)\n",
                            (pA > pB) ? A->name : B->name, (pA > pB) ? pA : pB, (pA > pB) ? pB : pA);
                break;
            }
            case 3: {
                double vA = volatility_for(A);
                double vB = volatility_for(B);
                printf("%s: Volatility (std dev of daily returns) = %.4f (percentage points)\n", A->name, vA);
                printf("%s: Volatility (std dev of daily returns) = %.4f (percentage points)\n", B->name, vB);
                if (fabs(vA - vB) < 1e-9) printf("Both have similar volatility.\n");
                else printf("%s is more stable (lower volatility).\n", (vA < vB) ? A->name : B->name);
                break;
            }
            case 4: {
                double slopeA = trend_slope_for(A);
                double slopeB = trend_slope_for(B);
                printf("%s: trend slope = %.6f (price units per day)\n", A->name, slopeA);
                printf("%s: trend slope = %.6f (price units per day)\n", B->name, slopeB);
                int winner = which_is_better(A, B);
                if (winner == 0) printf("Suggestion: %s looks better overall.\n", A->name);
                else if (winner == 1) printf("Suggestion: %s looks better overall.\n", B->name);
                else printf("Suggestion: Both look similar.\n");
                break;
            }
            case 5: {
                int buyA, sellA, buyB, sellB;
                double profA, profB;
                best_buy_sell_for(A, &buyA, &sellA, &profA);
                best_buy_sell_for(B, &buyB, &sellB, &profB);
                if (profA > 0) printf("%s: Buy day %d at %.2f, Sell day %d at %.2f => profit = %.2f\n",
                                      A->name, buyA+1, A->price[buyA], sellA+1, A->price[sellA], profA);
                else printf("%s: No profitable single-transaction found.\n", A->name);
                if (profB > 0) printf("%s: Buy day %d at %.2f, Sell day %d at %.2f => profit = %.2f\n",
                                      B->name, buyB+1, B->price[buyB], sellB+1, B->price[sellB], profB);
                else printf("%s: No profitable single-transaction found.\n", B->name);
                if (profA > profB + 1e-9) printf("%s had a better single-transaction opportunity.\n", A->name);
                else if (profB > profA + 1e-9) printf("%s had a better single-transaction opportunity.\n", B->name);
                else printf("Both had similar single-transaction profit opportunities.\n");
                break;
            }
            default:
                printf("[ERR] Invalid choice\n");
        }
    }
}

void save_report(void) { printf("[F] Save report placeholder\n"); }

/*initialize cache*/
void initialize(cache *q) {
    q->rear = CACHE_SIZE - 1;
    q->front = q->length = 0;
}

int isFull(cache *q) { return q->length == CACHE_SIZE; }
int isEmpty(cache *q) { return q->length == 0; }

/*To protect against duplicate - checks if company name is already present in cache*/
int exists_in_cache(cache *q, const char *str) {
    if(isEmpty(q)) return 0;

    int idx = q->front;
    for(int i = 0; i < q->length; i++) {
        if(strcmp(q->array[idx], str) == 0)
            return 1;  // found duplicate
        idx = (idx + 1) % CACHE_SIZE;
    }
    return 0;
}

/*cache dequeue function*/
char* deq(cache *q) {
    if(isEmpty(q)) return NULL;
    char *str = q->array[q->front];
    q->front = (q->front + 1) % CACHE_SIZE;
    q->length--;
    return str;
}

/*cache enqueue function*/
void enq_cache(cache *q, char *str) {
    if(exists_in_cache(q, str)) return; //If name is already in cache
    // If full, pop the oldest first (automatic FIFO replacement)
    if(isFull(q)) {
        char *old = deq(q);
        if(old) free(old);
    }
    q->rear = (q->rear + 1) % CACHE_SIZE;
    q->array[q->rear] = (char*)malloc(strlen(str) + 1);
    strcpy(q->array[q->rear], str);
    q->length++;
}

/*print recent cache*/
void recent_searches(cache *q) {
    if(isEmpty(q)) {
        printf("No company searched yet\n");
        return;
    }
    int idx = q->front;
    for(int i = 0; i < q->length; i++) {
        printf("%s\n", q->array[idx]);
        idx = (idx + 1) % CACHE_SIZE;
    }
}

void gnuplot_plot(void) { printf("[H] Gnuplot placeholder\n"); }

/* Classify company as Good/Bad/Mixed using same rule as top_k */
const char *classify_label_for(int idx) {
    double p = company_profit(idx);
    int trend = company_trend_score(idx);
    if (p > 0 && trend > 0) return "Good";
    if (p < 0 && trend < 0) return "Bad";
    return "Mixed";
}

/* all_trends: allow user to filter companies by trend label and class label. */
void all_trends(void) {
    if (count == 0) { printf("[WARN] No data loaded. Load CSV first.\n"); return; }

    char buf[64];
    int tchoice = -1;
    int cchoice = -1;

    printf("Filter by trend:\n");
    printf(" 0) Any\n");
    printf(" 1) Strong Upward\n");
    printf(" 2) Strong Downward\n");
    printf(" 3) Flat\n");
    printf(" 4) Overall Upward\n");
    printf(" 5) Overall Downward\n");
    printf(" 6) Mixed\n");
    printf("Choose trend filter (0-6): ");
    if (!fgets(buf, sizeof(buf), stdin)) return;
    if (sscanf(buf, "%d", &tchoice) != 1) { printf("[ERR] Bad number\n"); return; }
    if (tchoice < 0 || tchoice > 6) { printf("[ERR] Invalid choice\n"); return; }

    printf("\nFilter by class:\n");
    printf(" 0) Any\n");
    printf(" 1) Good\n");
    printf(" 2) Bad\n");
    printf(" 3) Mixed\n");
    printf("Choose class filter (0-3): ");
    if (!fgets(buf, sizeof(buf), stdin)) return;
    if (sscanf(buf, "%d", &cchoice) != 1) { printf("[ERR] Bad number\n"); return; }
    if (cchoice < 0 || cchoice > 3) { printf("[ERR] Invalid choice\n"); return; }

    const char *tlabels[] = {"Any", "Strong Upward", "Strong Downward", "Flat", "Overall Upward", "Overall Downward", "Mixed"};
    const char *clabels[] = {"Any", "Good", "Bad", "Mixed"};

    printf("\nMatching companies:\n");
    int found = 0;
    for (int i = 0; i < count; i++) {
        const char *tlabel = trend_label_for(i);
        const char *clabel = classify_label_for(i);
        int tmatch = (tchoice == 0) || (strcmp(tlabel, tlabels[tchoice]) == 0);
        int cmatch = (cchoice == 0) || (strcmp(clabel, clabels[cchoice]) == 0);
        if (tmatch && cmatch) {
            double p = company_profit(i);
            printf("%2d. %s | Profit: %+6.2f | Trend: %s | Class: %s\n", i + 1, companies[i].name, p, tlabel, clabel);
            found++;
        }
    }

    if (!found) printf("No companies matched the selected filters.\n");
}

/* ---------------- Main ---------------- */
int main() {
    char buf[64];
    int main_choice;
    cache recent; //to maintain recent cache
    initialize(&recent);

    printf("Welcome to Simple Stock Tool\n");

    while (1) {
        show_main_menu();
        printf("Your choice: ");
        if (!fgets(buf, sizeof(buf), stdin)) continue;
        if (sscanf(buf, "%d", &main_choice) != 1) { printf("[ERR] Enter a number\n"); continue; }

        if (main_choice == 9) { printf("Goodbye!\n"); break; }

        if ((main_choice >= 2 && main_choice <= 8) && count == 0) {
            printf("[WARN] No data loaded! Choose 1 to load CSV first.\n");
            continue;
        }

        if (main_choice == 1) {
            load_data("stock_prices.csv");
            continue;
        }

        switch (main_choice) {
            case 2:
                if (find_company(&recent)) {
                    int sub_choice;
                    while (1) {
                        show_company_menu();
                        printf("Sub-choice: ");
                        if (!fgets(buf, sizeof(buf), stdin)) continue;
                        if (sscanf(buf, "%d", &sub_choice) != 1) { printf("[ERR] Enter a number\n"); continue; }
                        if (sub_choice == 6) { selected = -1; break; }

                        switch (sub_choice) {
                            case 1: show_data(); break;
                            case 2: moving_average(); break;
                            case 3: trend(); break;
                            case 4: small_graph(); break;
                            case 5: best_buy_sell(); break;
                            default: printf("[ERR] Invalid sub-choice\n");
                        }
                    }
                }
                break;

            case 3: top_k(&recent); break;
            case 4: all_trends(); break;
            case 5: compare_two(); break;
            case 6: save_report(); break;
            case 7: recent_searches(&recent); break;
            case 8: gnuplot_plot(); break;
            default: printf("[ERR] Choose 1..9\n");
        }
    }

    /* free recent cache strings */
    while (!isEmpty(&recent)) { char *s = deq(&recent); if (s) free(s); }

    return 0;
}
