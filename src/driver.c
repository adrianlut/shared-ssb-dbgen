/* @(#)driver.c	2.1.8.4 */
/* main driver for dss benchmark */

#define DECLARER                /* EXTERN references get defined here */
#define NO_FUNC (int (*) ()) NULL    /* to clean up tdefs */
#define NO_LFUNC (long (*) ()) NULL        /* to clean up tdefs */

#include "config.h"

#if (defined(HAVE_FORK) && defined(HAVE_WAIT) && defined(HAVE_KILL))
#define CAN_PARALLELIZE_DATA_GENERATION
#endif /* (defined(HAVE_FORK) && defined(HAVE_WAIT) && defined(HAVE_KILL)) */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_STRINGS_H

#include <strings.h>

#endif

#if (!defined(STDLIB_HAS_GETOPT) && defined(HAVE_GETOPT_H))

#include <getopt.h>

#elif (!defined(HAVE_GETOPT))
int     getopt(int arg_cnt, char **arg_vect, char *options);
#endif /* (!defined(STDLIB_HAS_GETOPT) && defined(HAVE_GETOPT_H)) */

#ifdef HAVE_UNISTD_H

#include <unistd.h>

#endif

#ifdef HAVE_SYS_WAIT_H

#include <sys/wait.h>

#endif

#if (defined(HAVE_PROCESS_H) && defined(HAVE_WINDOWS_H)) // Windows system
/* TODO: Do we really need all of these Windows-specific definitions? */
#include <process.h>
#ifdef _MSC_VER
#pragma warning(disable:4201)
#pragma warning(disable:4214)
#pragma warning(disable:4514)
#endif
#define WIN32_LEAN_AND_MEAN
#define NOATOM
#define NOGDICAPMASKS
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NORASTEROPS
#define NOSCROLL
#define NOSOUND
#define NOSYSMETRICS
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOMCX

#include <windows.h>
#ifdef _MSC_VER
#pragma warning(default:4201)
#pragma warning(default:4214)
#endif
#endif

#include "dss.h"
#include "dsstypes.h"
#include "bcd2.h"
#include "life_noise.h"
#include "util.h"

/*
* Function prototypes
*/
void usage(void);

int prep_direct(char *);

int close_direct(void);

void kill_load(void);

int pload(int tbl);

void gen_tbl(int tnum, DSS_HUGE start, DSS_HUGE count, long upd_num_f);

int pr_drange(int tbl, DSS_HUGE min, DSS_HUGE cnt, long num);

int set_files(int t, int pload);

int partial(int, int);


extern int optind, opterr;
extern char *optarg;
DSS_HUGE rowcnt = 0, minrow = 0;
long upd_num = 0;
double flt_scale;
#if (defined(WIN32) && !defined(_POSIX_C_SOURCE))
char *spawn_args[25];
#endif


/*
* general table descriptions. See dss.h for details on structure
* NOTE: tables with no scaling info are scaled according to
* another table
*
*
* the following is based on the tdef structure defined in dss.h as:
* typedef struct
* {
* char     *name;            -- name of the table; 
*                               flat file output in <name>.tbl
* long      base;            -- base scale rowcount of table; 
*                               0 if derived
* int       (*header) ();    -- function to prep output
* int       (*loader[2]) (); -- functions to present output
* long      (*gen_seed) ();  -- functions to seed the RNG
* int       (*verify) ();    -- function to verfiy the data set without building it
* int       child;           -- non-zero if there is an associated detail table
* unsigned long vtotal;      -- "checksum" total 
* }         tdef;
*
*/

/*
* flat file print functions; used with -F(lat) option
*/
int pr_cust(customer_t *c, int mode);

int pr_part(part_t *p, int mode);

int pr_supp(supplier_t *s, int mode);

int pr_line(order_t *o, int mode);

/*
* inline load functions; used with -D(irect) option
*/
int ld_cust(customer_t *c, int mode);

int ld_part(part_t *p, int mode);

int ld_supp(supplier_t *s, int mode);

/*todo: get rid of ld_order*/
int ld_line(order_t *o, int mode);

int ld_order(order_t *o, int mode);


/*
* seed generation functions; used with '-O s' option
*/
long sd_cust(int child, long skip_count);

long sd_part(int child, long skip_count);

long sd_supp(int child, long skip_count);

long sd_line(int child, long skip_count);

long sd_order(int child, long skip_count);


/*
* header output functions); used with -h(eader) option
*/
int hd_cust(FILE *f);

int hd_part(FILE *f);

int hd_supp(FILE *f);

int hd_line(FILE *f);


/*
* data verfication functions; used with -O v option
*/
int vrf_cust(customer_t *c, int mode);

int vrf_part(part_t *p, int mode);

int vrf_supp(supplier_t *s, int mode);

int vrf_line(order_t *o, int mode);

int vrf_order(order_t *o, int mode);

int vrf_date(date_t, int mode);


tdef tdefs[] =
        {

                {"part.tbl",      "part table",      200000, hd_part,
                                                                {pr_part, ld_part}, sd_part, vrf_part, NONE, 0},
                {0,               0,                 0,      0, {0,       0},       0,       0, 0,           0},
                {"supplier.tbl",  "suppliers table", 2000,   hd_supp,
                                                                {pr_supp, ld_supp}, sd_supp, vrf_supp, NONE, 0},

                {"customer.tbl",  "customers table", 30000,  hd_cust,
                                                                {pr_cust, ld_cust}, sd_cust, vrf_cust, NONE, 0},
                {"date.tbl",      "date table",      2557,   0, {pr_date, ld_date}, NULL,    vrf_date, NONE, 0},
                /*line order is SF*1,500,000, however due to the implementation
                  the base here is 150,000 instead if 1500,000*/
                {"lineorder.tbl", "lineorder table", 150000, hd_line,
                                                                {pr_line, ld_line}, sd_line, vrf_line, NONE, 0},
                {0,               0,                 0,      0, {0,       0},       0,       0, 0,           0},
                {0,               0,                 0,      0, {0,       0},       0,       0, 0,           0},
                {0,               0,                 0,      0, {0,       0},       0,       0, 0,           0},
                {0,               0,                 0,      0, {0,       0},       0,       0, 0,           0},
        };

int *pids;

/*
* routines to handle the graceful cleanup of multiprocess loads
*/

void
stop_proc(int signum) {
    UNUSED(signum);
    exit(0);
}

/*
 * Notes:
 * The parallel load code is at best brittle, and seems not to
 * have been tested or even built on non-Linux platforms.
 */

#ifdef HAVE_KILL

void
kill_load(void) {
    int i;

    for (i = 0; i < children; i++) {
        if (pids[i])
            kill(SIGUSR1, pids[i]);
    }
}

#endif

/*
* re-set default output file names 
*/
int
set_files(int i, int pload) {
    char line[80], *new_name;

    if (table & (1 << i))
        child_table:
        {
            if (pload != -1)
                sprintf(line, "%s.%d", tdefs[i].name, pload);
            else {
                printf("Enter new destination for %s data: ",
                       tdefs[i].name);
                if (fgets(line, sizeof(line), stdin) == NULL)
                    return (-1);;
                if ((new_name = strchr(line, '\n')) != NULL)
                    *new_name = '\0';
                if (strlen(line) == 0)
                    return (0);
            }
            new_name = (char *) malloc(strlen(line) + 1);
            MALLOC_CHECK (new_name);
            strcpy(new_name, line);
            tdefs[i].name = new_name;
            if (tdefs[i].child != NONE) {
                i = tdefs[i].child;
                tdefs[i].child = NONE;
                goto child_table;
            }
        }

    return (0);
}

void split_dimension(merchant_distribution *target, long total_count, distribution * source_distribution) {
    int merchant_count = m_order.count;
    int part_count = (1 << merchant_count) - 1;
    int parts_per_merchant = 1 << (merchant_count - 1);

    target->merchant_count = merchant_count;
    target->part_count = part_count;
    target->parts_per_merchant = parts_per_merchant;

    target->parts = (distribution_part *) malloc(sizeof(distribution_part) * part_count);
    target->merchant_infos = (merchant_info *) malloc(sizeof(merchant_info) * merchant_count);
    target->part_owners = (sub_part_owner *) malloc(sizeof(sub_part_owner) * merchant_count * parts_per_merchant);

    for (int merchant_i = 0; merchant_i < merchant_count; ++merchant_i) {
        target->merchant_infos[merchant_i].parts = parts_per_merchant;
        target->merchant_infos[merchant_i].total_count = 0;
        target->merchant_infos[merchant_i].block_sizes = (long *) malloc(sizeof(long) * parts_per_merchant);
        target->merchant_infos[merchant_i].end_indexes = (long *) malloc(sizeof(long) * parts_per_merchant);

        for (int i = 0; i < parts_per_merchant; ++i) {
            target->merchant_infos[merchant_i].block_sizes[i] = 0;
            target->merchant_infos[merchant_i].end_indexes[i] = 0;
        }
    }

    int part_owner_index = 0;
    int * merchant_counter = (int *) malloc(sizeof(int) * merchant_count);
    for (int i = 0; i < merchant_count; ++i) {
        merchant_counter[i] = 0;
    }
    long cumsum = 0;

    for (int part_index = 0; part_index < part_count; ++part_index) {
        if ((total_count * source_distribution->list[part_index].weight_single) % source_distribution->max != 0) {
            printf("cannot divide customers evenly between merchants!");
            exit(1);
        }

        target->parts[part_index].start = cumsum + 1;
        target->parts[part_index].size = total_count * source_distribution->list[part_index].weight_single / source_distribution->max;
        target->parts[part_index].name = source_distribution->list[part_index].text;

        long name_len = strlen(target->parts[part_index].name);
        target->parts[part_index].sub_part_count = name_len;
        long per_merchant_size = target->parts[part_index].size / name_len;
        target->parts[part_index].sub_part_size = per_merchant_size;
        for (int merchant_index = 0; merchant_index < merchant_count; ++merchant_index) {
            if (strstr(target->parts[part_index].name, m_order.list[merchant_index].text) != NULL) {
                cumsum += per_merchant_size;
                target->part_owners[part_owner_index].last_index = cumsum;
                target->part_owners[part_owner_index].owner = merchant_index;
                target->merchant_infos[merchant_index].block_sizes[merchant_counter[merchant_index]] = per_merchant_size;
                target->merchant_infos[merchant_index].end_indexes[merchant_counter[merchant_index]] = cumsum;
                target->merchant_infos[merchant_index].total_count += per_merchant_size;
                ++part_owner_index;
                ++(merchant_counter[merchant_index]);
            }
        }
    }

    free(merchant_counter);
}


/*
* read the distributions needed in the benchamrk
*/
void
load_dists(void) {
    read_dist(env_config(DIST_TAG, DIST_DFLT), "p_cntr", &p_cntr_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "colors", &colors);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "p_types", &p_types_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "nations", &nations);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "regions", &regions);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "o_oprio",
              &o_priority_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "instruct",
              &l_instruct_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "smode", &l_smode_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "category",
              &l_category_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "rflag", &l_rflag_set);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "msegmnt", &c_mseg_set);

    /* load the distributions that contain text generation */
    read_dist(env_config(DIST_TAG, DIST_DFLT), "nouns", &nouns);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "verbs", &verbs);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "adjectives", &adjectives);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "adverbs", &adverbs);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "auxillaries", &auxillaries);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "terminators", &terminators);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "articles", &articles);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "prepositions", &prepositions);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "grammar", &grammar);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "np", &np);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "vp", &vp);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "merchant_order", &m_order);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "merchant_customer", &m_cust);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "merchant_supplier", &m_supp);
    read_dist(env_config(DIST_TAG, DIST_DFLT), "merchant_part", &m_part);

    split_dimension(&m_cust_distribution, O_CKEY_MAX, &m_cust);
    split_dimension(&m_supp_distribution, L_SKEY_MAX, &m_supp);
    split_dimension(&m_part_distribution, L_PKEY_MAX, &m_part);
}

/*
* generate a particular table
*/
void
gen_tbl(int tnum, DSS_HUGE start, DSS_HUGE count, long upd_num_f) {
    static order_t o;
    supplier_t supp;
    customer_t cust;
    part_t part;
    date_t dt;
    static int completed = 0;
    static int init = 0;
    DSS_HUGE i;

    int rows_per_segment = 0;
    int rows_this_segment = -1;
    int residual_rows = 0;

    if (insert_segments) {
        rows_per_segment = count / insert_segments;
        residual_rows = count - (rows_per_segment * insert_segments);
    }

    if (init == 0) {
        INIT_HUGE(o.okey);
        for (i = 0; i < O_LCNT_MAX; i++) INIT_HUGE(o.lineorders[i].okey);
        init = 1;
    }

    for (i = start; count; count--, i++) {
        LIFENOISE (1000, i);
        row_start(tnum);

        switch (tnum) {
            case LINE:
                mk_order(i, &o, upd_num_f % 10000);

                if (insert_segments && (upd_num_f > 0)) {
                    if ((upd_num_f / 10000) < residual_rows) {
                        if ((++rows_this_segment) > rows_per_segment) {
                            rows_this_segment = 0;
                            upd_num_f += 10000;
                        }
                    } else {
                        if ((++rows_this_segment) >= rows_per_segment) {
                            rows_this_segment = 0;
                            upd_num_f += 10000;
                        }
                    }
                }

                if (set_seeds == 0) {
                    if (validate)
                        tdefs[tnum].verify(&o, 0);
                    else
                        tdefs[tnum].loader[direct](&o, upd_num_f);
                }
                break;
            case SUPP:
                mk_supp(i, &supp);
                if (set_seeds == 0) {
                    if (validate)
                        tdefs[tnum].verify(&supp, 0);
                    else
                        tdefs[tnum].loader[direct](&supp, upd_num_f);
                }
                break;
            case CUST:
                mk_cust(i, &cust);
                if (set_seeds == 0) {
                    if (validate)
                        tdefs[tnum].verify(&cust, 0);
                    else
                        tdefs[tnum].loader[direct](&cust, upd_num_f);
                }
                break;
            case PART:
                mk_part(i, &part);
                if (set_seeds == 0) {
                    if (validate)
                        tdefs[tnum].verify(&part, 0);
                    else
                        tdefs[tnum].loader[direct](&part, upd_num_f);
                }
                break;
            case DATE:
                mk_date(i, &dt);
                if (set_seeds == 0) {
                    if (validate)
                        tdefs[tnum].verify(&dt, 0);
                    else
                        tdefs[tnum].loader[direct](&dt, 0);
                }
                break;
            default:
                printf("Unknown table in gen_tbl!\n");
        }
        row_stop(tnum);
        if (set_seeds && (i % tdefs[tnum].base) < 2) {
            printf("\nSeeds for %s at rowcount " HUGE_FORMAT "\n", tdefs[tnum].comment, i);
            dump_seeds(tnum);
        }
    }
    completed |= 1 << tnum;
}


void
usage(void) {
    fprintf(stderr, "%s\n%s\n\t%s\n%s %s\n\n",
            "USAGE:",
            "dbgen [-{vfFD}] [-O {fhmsv}][-T {pcsdla}]",
            "[-s <scale>][-C <chunks>][-S <step>]",
            "dbgen [-v] [-O {dfhmr}] [-s <scale>]",
            "[-U <updates>] [-r <percent>]");

    fprintf(stderr, "-b <s> -- load distributions from file <s> (default: " DIST_DFLT ")\n");
    fprintf(stderr, "-C <n> -- separate data set into <n> chunks\n");
    fprintf(stderr, "          (requires -S; default: 1; uses <n> child processes)\n");
    fprintf(stderr, "-D     -- do database load in line\n");
    fprintf(stderr, "-d <n> -- split deletes between <n> files\n");
    fprintf(stderr, "-f     -- force. Overwrite existing files\n");
    fprintf(stderr, "-F     -- generate flat files output\n");
    fprintf(stderr, "-h     -- display this message\n");
    fprintf(stderr, "-i <n> -- split inserts between <n> files\n");
    fprintf(stderr, "-n <s> -- inline load into database <s>\n");
    fprintf(stderr, "-O d   -- generate SQL syntax for deletes\n");
    fprintf(stderr, "-O f   -- over-ride default output file names\n");
    fprintf(stderr, "-O h   -- output files with headers\n");
    fprintf(stderr, "-O m   -- produce columnar output\n");
    fprintf(stderr, "-O r   -- generate key ranges for deletes.\n");
    fprintf(stderr, "-O v   -- Verify data set without generating it.\n");
    fprintf(stderr, "-q     -- enable QUIET mode\n");
    fprintf(stderr, "-r <n> -- updates refresh (n/100)%% of the data set\n");
    fprintf(stderr, "-s <n> -- set Scale Factor (SF) to  <n> \n");
    fprintf(stderr, "-S <n> -- build the <n>th step of the data/update set\n");

    fprintf(stderr, "-T c   -- generate cutomers dimension table ONLY\n");
    fprintf(stderr, "-T p   -- generate parts dimension table ONLY\n");
    fprintf(stderr, "-T s   -- generate suppliers dimension table ONLY\n");
    fprintf(stderr, "-T d   -- generate date dimension table ONLY\n");
    fprintf(stderr, "-T l   -- generate lineorder fact table ONLY\n");

    fprintf(stderr, "-U <s> -- generate <s> update sets\n");
    fprintf(stderr, "-v     -- enable VERBOSE mode\n");
    fprintf(stderr,
            "\nTo generate the SF=1 (1GB), validation database population, use:\n");
    fprintf(stderr, "\tdbgen -vfF -s 1\n");
    fprintf(stderr, "\nTo generate updates for a SF=1 (1GB), use:\n");
    fprintf(stderr, "\tdbgen -v -U 1 -s 1\n");
}

/*
* pload() -- handle the parallel loading of tables
*/
/*
* int partial(int tbl, int s) -- generate the s-th part of the named tables data
*/
int
partial(int tbl, int s) {
    long rowcnt;
    long extra;

    if (verbose > 0) {
        fprintf(stderr, "\tStarting to load stage %d of %ld for %s...",
                s, children, tdefs[tbl].comment);
    }

    if (direct == 0)
        set_files(tbl, s);

    rowcnt = set_state(tbl, scale, children, s, &extra);

    if (s == children)
        gen_tbl(tbl, rowcnt * (s - 1) + 1, rowcnt + extra, upd_num);
    else
        gen_tbl(tbl, rowcnt * (s - 1) + 1, rowcnt, upd_num);

    if (verbose > 0)
        fprintf(stderr, "done.\n");

    return (0);
}

/*
 * Notes:
 * This code is at best brittle, and seems not to have been tested or
 * even built on non-Linux platforms.
 */

#ifdef CAN_PARALLELIZE_DATA_GENERATION

int
pload(int tbl) {
    int c = 0, i, status;

    if (verbose > 0) {
        fprintf(stderr, "Starting %ld children to load %s",
                children, tdefs[tbl].comment);
    }
    for (c = 0; c < children; c++) {
        pids[c] = fork();
        if (pids[c] == -1) {
            perror("Child loader not created");
            kill_load();
            exit(-1);
        } else if (pids[c] == 0)    /* CHILD */
        {
            signal(SIGUSR1, stop_proc);
            verbose = 0;
            partial(tbl, c + 1);
            exit(0);
        } else if (verbose > 0)            /* PARENT */
        {
            fprintf(stderr, ".");
        }
    }

    if (verbose > 0)
        fprintf(stderr, "waiting...");

    c = children;
    while (c) {
        i = wait(&status);
        if (i == -1 && children) {
            if (errno == ECHILD)
                fprintf(stderr, "\nCould not wait on pid %d\n", pids[c - 1]);
            else if (errno == EINTR)
                fprintf(stderr, "\nProcess %d stopped abnormally\n", pids[c - 1]);
            else if (errno == EINVAL)
                fprintf(stderr, "\nProgram bug\n");
        }
        if (!WIFEXITED(status)) {
            (void) fprintf(stderr, "\nProcess %d: ", i);
            if (WIFSIGNALED(status)) {
                (void) fprintf(stderr, "rcvd signal %d\n",
                               WTERMSIG(status));
            } else if (WIFSTOPPED(status)) {
                (void) fprintf(stderr, "stopped, signal %d\n",
                               WSTOPSIG(status));
            }

        }
        c--;
    }

    if (verbose > 0)
        fprintf(stderr, "done\n");
    return (0);
}

#endif /* CAN_PARALLELIZE_DATA_GENERATION */


void
process_options(int count, char **vector) {
    int option;

    while ((option = getopt(count, vector,
                            "b:C:Dd:Ffi:hn:O:P:qr:s:S:T:U:v")) != -1)
        switch (option) {
            case 'b':                /* load distributions from named file */
                d_path = (char *) malloc(strlen(optarg) + 1);
                MALLOC_CHECK(d_path);
                strcpy(d_path, optarg);
                break;
            case 'q':                /* all prompts disabled */
                verbose = -1;
                break;
            case 'i':
                insert_segments = atoi(optarg);
                break;
            case 'd':
                delete_segments = atoi(optarg);
                break;
            case 'S':                /* generate a particular STEP */
                step = atoi(optarg);
                if (step > 1) {
                    table &= ~(1 << DATE);
                }
                break;
            case 'v':                /* life noises enabled */
                verbose = 1;
                break;
            case 'f':                /* blind overwrites; Force */
                force = 1;
                break;
            case 'T':                /* generate a specifc table */
                switch (*optarg) {
                    case 'c':            /* generate customer ONLY */
                        table = 1 << CUST;
                        break;
                    case 'p':            /* generate part ONLY */
                        table = 1 << PART;
                        break;
                    case 's':            /* generate partsupp ONLY */
                        table = 1 << SUPP;
                        break;
                    case 'd':            /* generate date ONLY */
                        table = 1 << DATE;
                        break;
                    case 'l':            /* generate lineorder table ONLY */
                        table = 1 << LINE;
                        break;
                    default:
                        fprintf(stderr, "Unknown table name %s\n",
                                optarg);
                        usage();
                        exit(1);
                }
                break;
            case 's':                /* scale by Percentage of base rowcount */
            case 'P':                /* for backward compatibility */
                flt_scale = atof(optarg);
                if (flt_scale < MIN_SCALE) {
                    int i;

                    scale = 1;
                    for (i = PART; i < REGION; i++) {
                        tdefs[i].base = (long) (tdefs[i].base * flt_scale);
                        if (tdefs[i].base < 1)
                            tdefs[i].base = 1;
                    }
                } else
                    scale = (long) flt_scale;
                if (scale > MAX_SCALE) {
                    fprintf(stderr, "%s %5.0f %s\n\t%s\n\n",
                            "NOTE: Data generation for scale factors >",
                            MAX_SCALE,
                            "GB is still in development,",
                            "and is not yet supported.\n");
                    fprintf(stderr,
                            "Your resulting data set MAY NOT BE COMPLIANT!\n");
                }
                break;
            case 'O':                /* optional actions */
                switch (tolower (*optarg)) {
                    case 'd':            /* generate SQL for deletes */
                        gen_sql = 1;
                        break;
                    case 'f':            /* over-ride default file names */
                        fnames = 1;
                        break;
                    case 'h':            /* generate headers */
                        header = 1;
                        break;
                    case 'm':            /* generate columnar output */
                        columnar = 1;
                        break;
                    case 'r':            /* generate key ranges for delete */
                        gen_rng = 1;
                        break;
                    case 's':            /* calibrate the RNG usage */
                        set_seeds = 1;
                        break;
                    case 'v':            /* validate the data set */
                        validate = 1;
                        break;
                    default:
                        fprintf(stderr, "Unknown option name %s\n",
                                optarg);
                        usage();
                        exit(1);
                }
                break;
            case 'D':                /* direct load of generated data */
                direct = 1;
                break;
            case 'F':                /* generate flat files for later loading */
                direct = 0;
                break;
            case 'U':                /* generate flat files for update stream */
                updates = atoi(optarg);
                break;
            case 'r':                /* set the refresh (update) percentage */
                refresh = atoi(optarg);
                break;
#ifndef DOS
            case 'C':
                children = atoi(optarg);
                break;
#endif /* !DOS */
            case 'n':                /* set name of database for direct load */
                db_name = (char *) malloc(strlen(optarg) + 1);
                MALLOC_CHECK (db_name);
                strcpy(db_name, optarg);
                break;
            default:
                printf("ERROR: option '%c' unknown.\n",
                       *(vector[optind] + 1));
                /* fallthrough */
            case 'h':                /* something unexpected */
                fprintf(stderr,
                        "%s Population Generator (Version %d.%d.%d%s)\n",
                        NAME, VERSION, RELEASE,
                        MODIFICATION, PATCH);
                fprintf(stderr, "Copyright %s %s\n", TPC, C_DATES);
                usage();
                exit(1);
        }

#ifndef DOS
    if (children != 1 && step == -1) {
        pids = malloc(children * sizeof(pid_t));
        MALLOC_CHECK(pids)
    }
#else
    if (children != 1 && step < 0)
        {
        fprintf(stderr, "ERROR: -C must be accompanied by -S on this platform\n");
        exit(1);
        }
#endif /* DOS */

    return;
}

/*
* MAIN
*
* using getopt() to clean up the command line handling
*/
int
main(int ac, char **av) {
    int i;

    table =
            (1 << CUST) |
            (1 << PART) |
            (1 << SUPP) |
            (1 << DATE) |
            (1 << LINE);
    force = 0;
    insert_segments = 0;
    delete_segments = 0;
    insert_orders_segment = 0;
    insert_lineitem_segment = 0;
    delete_segment = 0;
    verbose = 0;
    columnar = 0;
    set_seeds = 0;
    header = 0;
    direct = 0;
    scale = 1;
    flt_scale = 1.0;
    updates = 0;
    refresh = UPD_PCT;
    step = -1;
    tdefs[LINE].base *=
            ORDERS_PER_CUST;            /* have to do this after init */
    fnames = 0;
    db_name = NULL;
    gen_sql = 0;
    gen_rng = 0;
    children = 1;
    d_path = NULL;

    process_options(ac, av);
#if (defined(WIN32) && !defined(_POSIX_C_SOURCE))
    for (i = 0; i < ac; i++)
    {
        spawn_args[i] = malloc ((strlen (av[i]) + 1) * sizeof (char));
        MALLOC_CHECK (spawn_args[i]);
        strcpy (spawn_args[i], av[i]);
    }
    spawn_args[ac] = NULL;
#endif

    if (verbose >= 0) {
        fprintf(stderr,
                "%s Population Generator (Version %d.%d.%d%s)\n",
                NAME, VERSION, RELEASE, MODIFICATION, PATCH);
        fprintf(stderr, "Copyright %s %s\n", C_DATES, TPC);
        fprintf(stderr, "Copyright %s %s\n", "2023", "Adrian Lutsch");
    }

    load_dists();
    /* have to do this after init */
    tdefs[NATION].base = nations.count;
    tdefs[REGION].base = regions.count;

    /*
    * updates are never parallelized
    */
    if (updates) {
        /*
         * set RNG to start generating rows beyond SF=scale
         */
        double fix1;

        set_state(LINE, scale, 1, 2, (long *) &i);
        fix1 = (double) tdefs[LINE].base / (double) 10000; /*represent the %% percentage (n/100)%*/
        rowcnt = (int) (fix1 * scale * refresh);
        if (step > 0) {
            /*
             * adjust RNG for any prior update generation
             */
            sd_order(0, rowcnt * (step - 1));
            sd_line(0, rowcnt * (step - 1));
            upd_num = step - 1;
        } else
            upd_num = 0;

        while (upd_num < updates) {
            if (verbose > 0)
                fprintf(stderr,
                        "Generating update pair #%ld for %s [pid: %d]",
                        upd_num + 1, tdefs[LINE].comment, DSS_PROC);
            insert_orders_segment = 0;
            insert_lineitem_segment = 0;
            delete_segment = 0;
            minrow = upd_num * rowcnt + 1;
            gen_tbl(LINE, minrow, rowcnt, upd_num + 1);
            if (verbose > 0)
                fprintf(stderr, "done.\n");
            pr_drange(LINE, minrow, rowcnt, upd_num + 1);
            upd_num++;
        }

        exit(0);
    }

    /**
    ** actual data generation section starts here
    **/
/*
 * open database connection or set all the file names, as appropriate
 */
    if (direct)
        prep_direct((db_name) ? db_name : DBNAME);
    else if (fnames)
        for (i = PART; i <= REGION; i++) {
            if (table & (1 << i))
                if (set_files(i, -1)) {
                    fprintf(stderr, "Load aborted!\n");
                    exit(1);
                }
        }

/*
 * traverse the tables, invoking the appropriate data generation routine for any to be built
 */
    for (i = PART; i <= REGION; i++)
        if (table & (1 << i)) {
            if (children > 1 && i < NATION) {
                if (step >= 0) {
                    if (validate) {
                        INTERNAL_ERROR("Cannot validate parallel data generation");
                    } else
                        partial(i, step);
                }
#ifdef CAN_PARALLELIZE_DATA_GENERATION
                else {
                    if (validate) {
                        INTERNAL_ERROR("Cannot validate parallel data generation");
                    } else
                        pload(i);
                }
#else
                else
                {
                    fprintf(stderr,
                        "Parallel load is not supported on your platform currently.\n");
                    exit(1);
                }
#endif /* CAN_PARALLELIZE_DATA_GENERATION */
            } else {
                minrow = 1;
                if (i < NATION)
                    rowcnt = tdefs[i].base * scale;
                else
                    rowcnt = tdefs[i].base;
                if (i == PART) {
                    rowcnt = (DSS_HUGE) (tdefs[i].base * (floor(1 + log((double) (scale)) / (log(2)))));
                }
                if (i == DATE) {
                    rowcnt = tdefs[i].base;
                }
                if (verbose > 0)
                    fprintf(stderr, "%s data for %s [pid: %d]: ",
                            (validate) ? "Validating" : "Generating", tdefs[i].comment, DSS_PROC);
                gen_tbl(i, minrow, rowcnt, upd_num);
                if (verbose > 0)
                    fprintf(stderr, "done.\n");
            }
            if (validate)
                printf("Validation checksum for %s at %ld GB: %0lx\n",
                       tdefs[i].name, scale, tdefs[i].vtotal);
        }

    if (direct)
        close_direct();

    return (0);
}

