/* @(#)build.c	2.1.8.1 */
/* Sccsid:     @(#)build.c	9.1.1.17     11/15/95  12:52:28 */
/* stuff related to the customer table */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <time.h>

#ifdef HAVE_SYS_TYPES_H // #ifndef VMS originally

#include <sys/types.h>

#endif /* HAVE_SYS_TYPES_H */

#ifdef HAVE_UNISTD_H

#include <unistd.h>

#endif /* HAVE_UNISTD_H */

#include "dss.h"
#include "dsstypes.h"
#include "bcd2.h"

#ifdef ADHOC
#include "adhoc.h"
extern adhoc_t adhocs[];
#endif /* ADHOC */

#define LEAP_ADJ(yr, mnth)      \
((LEAP(yr) && (mnth) >= 2) ? 1 : 0)
#define JDAY_BASE       8035    /* start from 1/1/70 a la unix */
#define JMNTH_BASE      (-70 * 12) /* start from 1/1/70 a la unix */
#define RPRICE_BRIDGE(tgt, p) tgt = rpb_routine(p)
#define V_STR(avg, sd, tgt)  a_rnd((int)(avg * V_STR_LOW), \
(int)(avg * V_STR_HGH), sd, tgt)

/**
 * generate the numbered order and its associated lineitems
 */
void mk_sparse(long i, DSS_HUGE *ok, long seq) {
    ez_sparse(i, ok, seq);
}

/**
 * the "simple" version of mk_sparse, used on systems with 64b support
 * and on all systems at SF <= 300G where 32b support is sufficient
 */
void ez_sparse(long i, DSS_HUGE *ok, long seq) {
    long low_bits;

    LONG2HUGE(i, ok);
    low_bits = (long) (i & ((1 << SPARSE_KEEP) - 1));
    *ok = *ok >> SPARSE_KEEP;
    *ok = *ok << SPARSE_BITS;
    *ok += seq;
    *ok = *ok << SPARSE_KEEP;
    *ok += low_bits;
}

long rpb_routine(long p) {
    long price;
    price = 90000;
    price += (p / 10) % 20001;        /* limit contribution to $200 */
    price += (p % 1000) * 100;

    return price;
}

static void gen_phone(long ind, char *target, long seed) {
    long acode;
    long exchg;
    long number;

    RANDOM(acode, 100, 999, seed);
    RANDOM(exchg, 100, 999, seed);
    RANDOM(number, 1000, 9999, seed);
    sprintf(target, "%02ld", 10 + (ind % NATIONS_MAX));
    sprintf(target + 3, "%03ld", acode);
    sprintf(target + 7, "%03ld", exchg);
    sprintf(target + 11, "%04ld", number);
    target[2] = target[6] = target[10] = '-';
}

/*bug! <- maybe this was fixed by using a valid seed?*/
int gen_city(char *cityName, char *nationName) {
    int i = 0;
    long randomPick;
    int nlen = strlen(nationName);

    strncpy(cityName, nationName, CITY_FIX - 1);

    if (nlen < CITY_FIX - 1) {
        for (i = nlen; i < CITY_FIX - 1; i++)
            cityName[i] = ' ';
    }
    RANDOM(randomPick, 0, 9, P_CITY_SD);

    sprintf(cityName + CITY_FIX - 1, "%ld", randomPick);
    cityName[CITY_FIX] = '\0';
    return 0;
}

/*
P_NAME is as long as 55 bytes in TPC-H, which is unreasonably large.
We reduce it to 22 by limiting to a concatenation of two colors (see [TPC-H], pg 94).
We also add a new column named P_COLOR that could be used in queries where currently a
color must be chosen by substring from P_NAME.
*/
int gen_color(char *source, char *dest) {
    int i = 0;
    int j = 0;
    int clen = 0;

    while (source[i] != ' ') {
        dest[i] = source[i];
        i++;
    }
    dest[i] = '\0';

    i++;
    while (source[i] != '\0') {
        source[j] = source[i];
        j++;
        i++;
    }

    source[j] = '\0';

    clen = strlen(dest);
    return clen;
}

holiday holidays[] = {
        {"Christmas",     12, 24},
        {"New Years Day", 1,  1},
        {"holiday1",      2,  20},
        {"Easter Day",    4,  20},
        {"holiday2",      5,  20},
        {"holiday3",      7,  20},
        {"holiday4",      8,  20},
        {"holiday5",      9,  20},
        {"holiday6",      10, 20},
        {"holiday7",      11, 20}
};

int gen_holiday_fl(char *dest, int month, int day) {
    for (int i = 0; i < NUM_HOLIDAYS; i++) {
        if (holidays[i].month == month && holidays[i].day == day) {
            strcpy(dest, "1");
            return 0;
        }
    }
    strcpy(dest, "0");
    return 0;
}


int lookup_merchant(const merchant_distribution *md, long id) {
    for (int i = 0; i < md->merchant_count * md->parts_per_merchant; ++i) {
        if (id - md->part_owners[i].last_index <= 0) {
            return md->part_owners[i].owner;
        }
    }

    // This should not happen!
    return 0;
}

long key_for_merchant(int merchant_id, const merchant_distribution *md, long random_stream_id) {
    long rnd;
    RANDOM(rnd, 1, md->merchant_infos[merchant_id].total_count, random_stream_id);

    long current_index = rnd;
    int i = 0;
    for (; i < md->parts_per_merchant; ++i) {
        current_index -= md->merchant_infos[merchant_id].block_sizes[i];
        if (current_index <= 0) break;
    }
    if (current_index > 0) {
        printf("Fatal error in key mapping!");
        exit(1);
    }

    return md->merchant_infos[merchant_id].end_indexes[i] + current_index;
}

/**
 * Determines whether the id is the start of a new part with multiple sub-parts according to the given distribution.
 * @param md
 * @param id
 * @return
 */
int is_backup_id(const merchant_distribution *md, long id, int next_part_i) {
    return (md->parts[next_part_i].start == id);
}

/**
 * Determines whether the id is the start of a new sub-part according to the given distribution.
 * @param md
 * @param id
 * @return
 */
int is_restore_id(merchant_distribution *md, long id, int part_i, int sub_part_i) {
    return id == (md->parts[part_i].start + sub_part_i * md->parts[part_i].sub_part_size);
}


/**
 * Creates the customer c with the key index.
 */
long mk_cust(long index, customer_t *c) {

    static int next_part_i = 0;
    static int current_part_i = -1;
    static int backup_existing = 0;
    static int next_subpart_i = 1;
    static long index_offset = 0;

    static long random_streams[5] = {C_ADDR_SD, C_NTRG_SD, C_PHNE_SD, C_MSEG_SD, P_CITY_SD};

    if (is_backup_id(&m_cust_distribution, index, next_part_i)) {
        current_part_i = next_part_i;
        ++next_part_i;
        if (next_part_i == m_cust_distribution.part_count) next_part_i = 0; // Prevent out of range access
        backup_existing = (m_cust_distribution.parts[current_part_i].sub_part_count > 1);

        index_offset = 0;
        if (backup_existing) {
            backup_random_state(random_streams, 5);
            if (verbose > 0) printf("Backup: %ld\n", index);
        } else if (!backup_existing && verbose > 0) {
            printf("No backup: %ld\n", index);
        }
        fflush(stdout);
    } else if (backup_existing && is_restore_id(&m_cust_distribution, index, current_part_i, next_subpart_i)) {
        index_offset = m_cust_distribution.parts[current_part_i].sub_part_size * next_subpart_i;
        ++next_subpart_i;
        // if next_subpart_i points at the end of the range of subparts, reset it to 1 and set backup_existing to 0 to
        // not check for restore points from here on.
        if (next_subpart_i == m_cust_distribution.parts[current_part_i].sub_part_count) {
            next_subpart_i = 1;
            backup_existing = 0;
        }

        restore_random_state(random_streams, 5);
        if (verbose > 0) printf("Restore: %ld\n", index);
        fflush(stdout);
    }

    c->custkey = index;
    sprintf(c->name, C_NAME_FMT, C_NAME_TAG, index - index_offset);
    c->alen = V_STR(C_ADDR_LEN, C_ADDR_SD, c->address);
    long i;
    RANDOM(i, 0, nations.count - 1, C_NTRG_SD);
    strcpy(c->nation_name, nations.list[i].text);
    strcpy(c->region_name, regions.list[nations.list[i].weight].text);
    gen_city(c->city, c->nation_name);
    gen_phone(i, c->phone, (long) C_PHNE_SD);
    pick_str(&c_mseg_set, C_MSEG_SD, c->mktsegment);
    c->merchant_id = lookup_merchant(&m_cust_distribution, index);
    return 0;
}

long mk_order(long index, order_t *o, long upd_num) {
    long lcnt;
    long rprice;
    long tmp_date;
    long c_date;
    long clk_num;
    static char **asc_date = NULL;
    char **mk_ascdate PROTO((void));

    if (asc_date == NULL)
        asc_date = mk_ascdate();

    RANDOM(tmp_date, O_ODATE_MIN, O_ODATE_MAX, O_ODATE_SD);
    strcpy(o->odate, asc_date[tmp_date - STARTDATE]);

    mk_sparse(index, o->okey, (upd_num == 0) ? 0 : 1 + upd_num / (10000 / refresh));

    char merchant_id_str[3];
    pick_str(&m_order, O_MERCHANT_SD, merchant_id_str);
    o->merchant_id = atoi(merchant_id_str);

    o->custkey = key_for_merchant(o->merchant_id, &m_cust_distribution, O_CKEY_SD);
    //RANDOM(o->custkey, O_CKEY_MIN, O_CKEY_MAX, O_CKEY_SD);

    pick_str(&o_priority_set, O_PRIO_SD, o->opriority);
    RANDOM(clk_num, 1, MAX((scale * O_CLRK_SCL), O_CLRK_SCL), O_CLRK_SD);
    o->spriority = 0;

    o->totalprice = 0;

    RANDOM(o->lines, O_LCNT_MIN, O_LCNT_MAX, O_LCNT_SD);
    for (lcnt = 0; lcnt < o->lines; lcnt++) {

        HUGE_SET(o->okey, o->lineorders[lcnt].okey);
        o->lineorders[lcnt].linenumber = lcnt + 1;
        o->lineorders[lcnt].custkey = o->custkey;

        o->lineorders[lcnt].partkey = key_for_merchant(o->merchant_id, &m_part_distribution, L_PKEY_SD);
        o->lineorders[lcnt].suppkey = key_for_merchant(o->merchant_id, &m_supp_distribution, L_SKEY_SD);
//        RANDOM(o->lineorders[lcnt].partkey, L_PKEY_MIN, L_PKEY_MAX, L_PKEY_SD);
//        RANDOM(o->lineorders[lcnt].suppkey, L_SKEY_MIN, L_SKEY_MAX, L_SKEY_SD);

        RANDOM(o->lineorders[lcnt].quantity, L_QTY_MIN, L_QTY_MAX, L_QTY_SD);
        RANDOM(o->lineorders[lcnt].discount, L_DCNT_MIN, L_DCNT_MAX, L_DCNT_SD);
        RANDOM(o->lineorders[lcnt].tax, L_TAX_MIN, L_TAX_MAX, L_TAX_SD);

        strcpy(o->lineorders[lcnt].orderdate, o->odate);

        strcpy(o->lineorders[lcnt].opriority, o->opriority);
        o->lineorders[lcnt].ship_priority = o->spriority;

        RANDOM(c_date, L_CDTE_MIN, L_CDTE_MAX, L_CDTE_SD);
        c_date += tmp_date;
        strcpy(o->lineorders[lcnt].commit_date, asc_date[c_date - STARTDATE]);

        pick_str(&l_smode_set, L_SMODE_SD, o->lineorders[lcnt].shipmode);

        RPRICE_BRIDGE(rprice, o->lineorders[lcnt].partkey);
        o->lineorders[lcnt].extended_price = rprice * o->lineorders[lcnt].quantity;
        o->lineorders[lcnt].revenue =
                o->lineorders[lcnt].extended_price * ((long) 100 - o->lineorders[lcnt].discount) / (long) PENNIES;

        //round off problem with linux if use 0.6
        o->lineorders[lcnt].supp_cost = 6 * rprice / 10;

        o->totalprice +=
                ((o->lineorders[lcnt].extended_price *
                  ((long) 100 - o->lineorders[lcnt].discount)) / (long) PENNIES) *
                ((long) 100 + o->lineorders[lcnt].tax)
                / (long) PENNIES;
    }

    for (lcnt = 0; lcnt < o->lines; lcnt++) {
        o->lineorders[lcnt].order_totalprice = o->totalprice;
    }
    return 0;
}

long mk_part(long index, part_t *p) {
    long mfgr;
    long cat;
    long brnd;

    static int next_part_i = 0;
    static int current_part_i = -1;
    static int backup_existing = 0;
    static int next_subpart_i = 1;

    static long random_streams[7] = {P_NAME_SD, P_MFG_SD, P_CAT_SD, P_BRND_SD, P_TYPE_SD, P_SIZE_SD, P_CNTR_SD};

    if (is_backup_id(&m_part_distribution, index, next_part_i)) {
        current_part_i = next_part_i;
        ++next_part_i;
        if (next_part_i == m_part_distribution.part_count) next_part_i = 0; // Prevent out of range access
        backup_existing = (m_part_distribution.parts[current_part_i].sub_part_count > 1);

        if (backup_existing) {
            backup_random_state(random_streams, 7);
            if (verbose > 0) printf("Backup: %ld\n", index);
        } else if (!backup_existing && verbose > 0) {
            printf("No backup: %ld\n", index);
        }
        fflush(stdout);
    } else if (backup_existing && is_restore_id(&m_part_distribution, index, current_part_i, next_subpart_i)) {
        ++next_subpart_i;
        // if next_subpart_i points at the end of the range of subparts, reset it to 1 and set backup_existing to 0 to
        // not check for restore points from here on.
        if (next_subpart_i == m_part_distribution.parts[current_part_i].sub_part_count) {
            next_subpart_i = 1;
            backup_existing = 0;
        }

        restore_random_state(random_streams, 7);
        if (verbose > 0) printf("Restore: %ld\n", index);
        fflush(stdout);
    }

    p->partkey = index;

    agg_str(&colors, (long) P_NAME_SCL, (long) P_NAME_SD, p->name);

    /*extract color from substring of p->name*/
    p->clen = gen_color(p->name, p->color);


    RANDOM(mfgr, P_MFG_MIN, P_MFG_MAX, P_MFG_SD);
    sprintf(p->mfgr, "%s%ld", "MFGR#", mfgr);

    RANDOM(cat, P_CAT_MIN, P_CAT_MAX, P_CAT_SD);
    sprintf(p->category, "%s%ld", p->mfgr, cat);


    RANDOM(brnd, P_BRND_MIN, P_BRND_MAX, P_BRND_SD);
    sprintf(p->brand, "%s%ld", p->category, brnd);

    p->tlen = pick_str(&p_types_set, P_TYPE_SD, p->type);
    p->tlen = strlen(p_types_set.list[p->tlen].text);
    RANDOM(p->size, P_SIZE_MIN, P_SIZE_MAX, P_SIZE_SD);

    pick_str(&p_cntr_set, P_CNTR_SD, p->container);
    p->merchant_id = lookup_merchant(&m_part_distribution, index);

    return 0;
}

long mk_supp(long index, supplier_t *s) {
    long i;

    static int next_part_i = 0;
    static int current_part_i = -1;
    static int backup_existing = 0;
    static int next_subpart_i = 1;
    static long index_offset = 0;

    static long random_streams[4] = {S_ADDR_SD, S_NTRG_SD, C_PHNE_SD, P_CITY_SD};

    if (is_backup_id(&m_supp_distribution, index, next_part_i)) {
        current_part_i = next_part_i;
        ++next_part_i;
        if (next_part_i == m_supp_distribution.part_count) next_part_i = 0; // Prevent out of range access
        backup_existing = (m_supp_distribution.parts[current_part_i].sub_part_count > 1);

        index_offset = 0;
        if (backup_existing) {
            backup_random_state(random_streams, 4);
            if (verbose > 0) printf("Backup: %ld\n", index);
        } else if (!backup_existing && verbose > 0) {
            printf("No backup: %ld\n", index);
        }
        fflush(stdout);
    } else if (backup_existing && is_restore_id(&m_supp_distribution, index, current_part_i, next_subpart_i)) {
        index_offset = m_supp_distribution.parts[current_part_i].sub_part_size * next_subpart_i;
        ++next_subpart_i;
        // if next_subpart_i points at the end of the range of subparts, reset it to 1 and set backup_existing to 0 to
        // not check for restore points from here on.
        if (next_subpart_i == m_supp_distribution.parts[current_part_i].sub_part_count) {
            next_subpart_i = 1;
            backup_existing = 0;
        }
        restore_random_state(random_streams, 4);
        if (verbose > 0) printf("Restore: %ld\n", index);
        fflush(stdout);
    }

    s->suppkey = index;
    sprintf(s->name, S_NAME_FMT, S_NAME_TAG, index - index_offset);
    s->alen = V_STR(S_ADDR_LEN, S_ADDR_SD, s->address);
    RANDOM(i, 0, nations.count - 1, S_NTRG_SD);
    strcpy(s->nation_name, nations.list[i].text);
    strcpy(s->region_name, regions.list[nations.list[i].weight].text);
    gen_city(s->city, s->nation_name);
    gen_phone(i, s->phone, (long) C_PHNE_SD);
    s->merchant_id = lookup_merchant(&m_supp_distribution, index);
    return 0;
}

struct {
    char *mdes;
    long days;
    long dcnt;
}
        months[] =

        {
                {NULL,  0,  0},
                {"JAN", 31, 31},
                {"FEB", 28, 59},
                {"MAR", 31, 90},
                {"APR", 30, 120},
                {"MAY", 31, 151},
                {"JUN", 30, 181},
                {"JUL", 31, 212},
                {"AUG", 31, 243},
                {"SEP", 30, 273},
                {"OCT", 31, 304},
                {"NOV", 30, 334},
                {"DEC", 31, 365}
        };
long mk_time(long index, dss_time_t *t) {
    long m = 0;
    long y;
    long d;

    t->timekey = index + JDAY_BASE;
    y = julian(index + STARTDATE - 1) / 1000;
    d = julian(index + STARTDATE - 1) % 1000;
    while (d > months[m].dcnt + LEAP_ADJ(y, m))
        m++;
    PR_DATE(t->alpha, y, m,
            d - months[m - 1].dcnt - ((LEAP(y) && m > 2) ? 1 : 0));
    t->year = 1900 + y;
    t->month = m + 12 * y + JMNTH_BASE;
    t->week = (d + T_START_DAY - 1) / 7 + 1;
    t->day = d - months[m - 1].dcnt - LEAP_ADJ(y, m - 1);

    return 0;
}
/*Following functions are related to date table generation*/
int days_in_a_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int days_in_a_month_l[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

season seasons[] = {
        {"Christmas", 1, 11, 31, 12},
        {"Summer",    1, 5,  31, 8},
        {"Winter",    1, 1,  31, 3},
        {"Spring",    1, 4,  30, 4},
        {"Fall",      1, 9,  31, 10}
};

char *month_names[] = {"January", "February", "March", "April",
                       "May", "June", "July", "August",
                       "September", "October", "November", "December"};

char *weekday_names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


int
is_last_day_in_month(int year, int month, int day) {
    int *days;
    if (LEAP(year))
        days = days_in_a_month_l;
    else
        days = days_in_a_month;
    if (day == days[month - 1]) return 1;
    return 0;
}

int gen_season(char *dest, int month, int day) {
    for (int i = 0; i < NUM_SEASONS; i++) {
        season *seas;
        seas = &seasons[i];

        if (month >= seas->start_month && month <= seas->end_month &&
            day >= seas->start_day && day <= seas->end_day) {
            strcpy(dest, seas->name);
            return 0;
        }
    }
    strcpy(dest, "");

    return 0;
}

/*make the date table, it takes the continuous index , and add index*60*60*24 to
 *numeric representation 1/1/1992 01:01:01,
 *then convert the final numeric date time to tm structure, and thus extract other field
 *for date_t structure */
long
mk_date(long index, date_t *d) {
    long espan = (index - 1) * 60 * 60 * 24;

    time_t numDateTime = D_STARTDATE + espan;

    struct tm *localTime = localtime(&numDateTime);

    /*make Sunday be the first day of a week */
    d->daynuminweek = ((long) localTime->tm_wday + 1) % 7 + 1;
    d->monthnuminyear = (long) localTime->tm_mon + 1;
#if __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
    strncpy(d->dayofweek, weekday_names[d->daynuminweek - 1], D_DAYWEEK_LEN + 1);
    strncpy(d->month, month_names[d->monthnuminyear - 1], D_MONTH_LEN + 1);
#if __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif
    d->year = (long) localTime->tm_year + 1900;
    d->daynuminmonth = (long) localTime->tm_mday;
    d->yearmonthnum = d->year * 100 + d->monthnuminyear;

    sprintf(d->yearmonth, "%.3s%d", d->month, d->year);
    sprintf(d->date, "%s %d, %d", d->month, d->daynuminmonth, d->year);

    d->datekey = d->year * 10000 + d->monthnuminyear * 100 + d->daynuminmonth;

    d->daynuminyear = (int) localTime->tm_yday + 1;
    d->weeknuminyear = d->daynuminyear / 7 + 1;

    if (d->daynuminweek == 7) {
        d->lastdayinweekfl[0] = '1';
    } else {
        d->lastdayinweekfl[0] = '0';
    }
    d->lastdayinweekfl[1] = '\0';

    if (is_last_day_in_month(d->year, d->monthnuminyear, d->daynuminmonth) == 1) {
        d->lastdayinmonthfl[0] = '1';
    } else {
        d->lastdayinmonthfl[0] = '0';
    }
    d->lastdayinmonthfl[1] = '\0';

    if (d->daynuminweek != 1 && d->daynuminweek != 7) {
        d->weekdayfl[0] = '1';
    } else {
        d->weekdayfl[0] = '0';
    }

    d->weekdayfl[1] = '\0';

    gen_season(d->sellingseason, d->monthnuminyear, d->daynuminmonth);
    d->slen = strlen(d->sellingseason);
    gen_holiday_fl(d->holidayfl, d->monthnuminyear, d->daynuminmonth);
    return (0);
}




















