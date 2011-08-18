/* -*- c-basic-offset: 2 -*- */
/* Copyright(C) 2010-2011 Brazil

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 2.1 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "ctx.h"
#include "db.h"
#include "ii.h"
#include "token.h"
#include "output.h"
#include <groonga/plugin.h>
#include <string.h>

#define VAR GRN_PROC_GET_VAR_BY_OFFSET
#define CONST_STR_LEN(x) x, x ? sizeof(x) - 1 : 0
#define TEXT_VALUE_LEN(x) GRN_TEXT_VALUE(x), GRN_TEXT_LEN(x)

#define COMPLETE 1
#define CORRECT  2
#define SUGGEST  4

typedef enum {
  GRN_SUGGEST_PREFIX_SEARCH_YES,
  GRN_SUGGEST_PREFIX_SEARCH_NO,
  GRN_SUGGEST_PREFIX_SEARCH_AUTO
} grn_suggest_prefix_search_mode;


static int
grn_parse_suggest_types(const char *nptr, const char *end)
{
  int types = 0;
  while (nptr < end) {
    if (*nptr == '|') {
      nptr += 1;
      continue;
    }
    if (!memcmp(nptr, CONST_STR_LEN("complete"))) {
      types |= COMPLETE;
      nptr += sizeof("complete") - 1;
    } else if (!memcmp(nptr, CONST_STR_LEN("correct"))) {
      types |= CORRECT;
      nptr += sizeof("correct") - 1;
    } else if (!memcmp(nptr, CONST_STR_LEN("suggest"))) {
      types |= SUGGEST;
      nptr += sizeof("suggest") - 1;
    } else {
      break;
    }
  }
  return types;
}

static int32_t
cooccurrence_search(grn_ctx *ctx, grn_obj *items, grn_obj *items_boost, grn_id id,
                    grn_obj *res, int query_type, int frequency_threshold,
                    double conditional_probability_threshold)
{
  int32_t max_score = 0;
  if (id) {
    grn_ii_cursor *c;
    grn_obj *co = grn_obj_column(ctx, items, CONST_STR_LEN("co"));
    grn_obj *pairs = grn_ctx_at(ctx, grn_obj_get_range(ctx, co));
    grn_obj *items_freq = grn_obj_column(ctx, items, CONST_STR_LEN("freq"));
    grn_obj *items_freq2 = grn_obj_column(ctx, items, CONST_STR_LEN("freq2"));
    grn_obj *pairs_freq, *pairs_post = grn_obj_column(ctx, pairs, CONST_STR_LEN("post"));
    switch (query_type) {
    case COMPLETE :
      pairs_freq = grn_obj_column(ctx, pairs, CONST_STR_LEN("freq0"));
      break;
    case CORRECT :
      pairs_freq = grn_obj_column(ctx, pairs, CONST_STR_LEN("freq1"));
      break;
    case SUGGEST :
      pairs_freq = grn_obj_column(ctx, pairs, CONST_STR_LEN("freq2"));
      break;
    default :
      return max_score;
    }
    if ((c = grn_ii_cursor_open(ctx, (grn_ii *)co, id, GRN_ID_NIL, GRN_ID_MAX,
                                ((grn_ii *)co)->n_elements - 1, 0))) {
      grn_ii_posting *p;
      grn_obj post, pair_freq, item_freq, item_freq2, item_boost;
      GRN_RECORD_INIT(&post, 0, grn_obj_id(ctx, items));
      GRN_INT32_INIT(&pair_freq, 0);
      GRN_INT32_INIT(&item_freq, 0);
      GRN_INT32_INIT(&item_freq2, 0);
      GRN_INT32_INIT(&item_boost, 0);
      while ((p = grn_ii_cursor_next(ctx, c))) {
        grn_id post_id;
        int pfreq, ifreq, ifreq2, boost;
        double conditional_probability;
        GRN_BULK_REWIND(&post);
        GRN_BULK_REWIND(&pair_freq);
        GRN_BULK_REWIND(&item_freq);
        GRN_BULK_REWIND(&item_freq2);
        GRN_BULK_REWIND(&item_boost);
        grn_obj_get_value(ctx, pairs_post, p->rid, &post);
        grn_obj_get_value(ctx, pairs_freq, p->rid, &pair_freq);
        post_id = GRN_RECORD_VALUE(&post);
        grn_obj_get_value(ctx, items_freq, post_id, &item_freq);
        grn_obj_get_value(ctx, items_freq2, post_id, &item_freq2);
        grn_obj_get_value(ctx, items_boost, post_id, &item_boost);
        pfreq = GRN_INT32_VALUE(&pair_freq);
        ifreq = GRN_INT32_VALUE(&item_freq);
        ifreq2 = GRN_INT32_VALUE(&item_freq2);
        if (ifreq2 > 0) {
          conditional_probability = (double)pfreq / (double)ifreq2;
        } else {
          conditional_probability = 0.0;
        }
        boost = GRN_INT32_VALUE(&item_boost);
        if (pfreq >= frequency_threshold && ifreq >= frequency_threshold &&
            conditional_probability >= conditional_probability_threshold &&
            boost >= 0) {
          grn_rset_recinfo *ri;
          void *value;
          int32_t score = pfreq;
          int added;
          if (max_score < score + boost) { max_score = score + boost; }
          /* put any formula if desired */
          if (grn_hash_add(ctx, (grn_hash *)res,
                           &post_id, sizeof(grn_id), &value, &added)) {
            ri = value;
            ri->score += score;
            if (added) {
              ri->score += boost;
            }
          }
        }
      }
      GRN_OBJ_FIN(ctx, &post);
      GRN_OBJ_FIN(ctx, &pair_freq);
      GRN_OBJ_FIN(ctx, &item_freq);
      GRN_OBJ_FIN(ctx, &item_freq2);
      GRN_OBJ_FIN(ctx, &item_boost);
      grn_ii_cursor_close(ctx, c);
    }
  }
  return max_score;
}

#define DEFAULT_LIMIT           10
#define DEFAULT_SORTBY          "-_score"
#define DEFAULT_OUTPUT_COLUMNS  "_key,_score"
#define DEFAULT_FREQUENCY_THRESHOLD 100
#define DEFAULT_CONDITIONAL_PROBABILITY_THRESHOLD 0.5

static void
output(grn_ctx *ctx, grn_obj *table, grn_obj *res, grn_id tid,
       grn_obj *sortby, grn_obj *output_columns, int offset, int limit)
{
  grn_obj *sorted;
  if ((sorted = grn_table_create(ctx, NULL, 0, NULL, GRN_OBJ_TABLE_NO_KEY, NULL, res))) {
    uint32_t nkeys;
    grn_obj_format format;
    grn_table_sort_key *keys;
    const char *sortby_val = GRN_TEXT_VALUE(sortby);
    unsigned sortby_len = GRN_TEXT_LEN(sortby);
    const char *oc_val = GRN_TEXT_VALUE(output_columns);
    unsigned oc_len = GRN_TEXT_LEN(output_columns);
    if (!sortby_val || !sortby_len) {
      sortby_val = DEFAULT_SORTBY;
      sortby_len = sizeof(DEFAULT_SORTBY) - 1;
    }
    if (!oc_val || !oc_len) {
      oc_val = DEFAULT_OUTPUT_COLUMNS;
      oc_len = sizeof(DEFAULT_OUTPUT_COLUMNS) - 1;
    }
    if ((keys = grn_table_sort_key_from_str(ctx, sortby_val, sortby_len, res, &nkeys))) {
      grn_table_sort(ctx, res, offset, limit, sorted, keys, nkeys);
      LAP(":", "sort(%d)", limit);
      GRN_OBJ_FORMAT_INIT(&format, grn_table_size(ctx, res), 0, limit, offset);
      format.flags =
        GRN_OBJ_FORMAT_WITH_COLUMN_NAMES|
        GRN_OBJ_FORMAT_XML_ELEMENT_RESULTSET;
      grn_obj_columns(ctx, sorted, oc_val, oc_len, &format.columns);
      GRN_OUTPUT_OBJ(sorted, &format);
      GRN_OBJ_FORMAT_FIN(ctx, &format);
      grn_table_sort_key_close(ctx, keys, nkeys);
    }
    grn_obj_unlink(ctx, sorted);
  } else {
    ERR(GRN_UNKNOWN_ERROR, "cannot create temporary sort table.");
  }
}

static inline void
complete_add_item(grn_ctx *ctx, grn_id id, grn_obj *res, int frequency_threshold,
                  grn_obj *items_freq, grn_obj *items_boost,
                  grn_obj *item_freq, grn_obj *item_boost)
{
  GRN_BULK_REWIND(item_freq);
  GRN_BULK_REWIND(item_boost);
  grn_obj_get_value(ctx, items_freq, id, item_freq);
  grn_obj_get_value(ctx, items_boost, id, item_boost);
  if (GRN_INT32_VALUE(item_boost) >= 0) {
    int32_t score;
    score = 1 +
            GRN_INT32_VALUE(item_freq) +
            GRN_INT32_VALUE(item_boost);
    if (score >= frequency_threshold) {
      void *value;
      if (grn_hash_add(ctx, (grn_hash *)res, &id, sizeof(grn_id),
                       &value, NULL)) {
        grn_rset_recinfo *ri;
        ri = value;
        ri->score += score;
      }
    }
  }
}

static void
complete(grn_ctx *ctx, grn_obj *items, grn_obj *items_boost, grn_obj *col,
         grn_obj *query, grn_obj *sortby,
         grn_obj *output_columns, int offset, int limit,
         int frequency_threshold, double conditional_probability_threshold,
         grn_suggest_prefix_search_mode prefix_search_mode)
{
  grn_obj *res;
  grn_obj *items_freq = grn_obj_column(ctx, items, CONST_STR_LEN("freq"));
  grn_obj item_freq, item_boost;
  GRN_INT32_INIT(&item_freq, 0);
  GRN_INT32_INIT(&item_boost, 0);
  if ((res = grn_table_create(ctx, NULL, 0, NULL,
                              GRN_TABLE_HASH_KEY|GRN_OBJ_WITH_SUBREC, items, NULL))) {
    grn_id tid = grn_table_get(ctx, items, TEXT_VALUE_LEN(query));
    if (GRN_TEXT_LEN(query)) {
      grn_str *norm;
      grn_table_cursor *cur;
      if ((norm = grn_str_open(ctx, TEXT_VALUE_LEN(query), GRN_STR_NORMALIZE))) {
        /* RK search + prefix search */
        grn_obj *index;
        /* FIXME: support index selection */
        if (grn_column_index(ctx, col, GRN_OP_PREFIX, &index, 1, NULL)) {
          if ((cur = grn_table_cursor_open(ctx, grn_ctx_at(ctx, index->header.domain),
                                           norm->norm, norm->norm_blen, NULL, 0, 0, -1,
                                           GRN_CURSOR_PREFIX|GRN_CURSOR_RK))) {
            grn_id id;
            while ((id = grn_table_cursor_next(ctx, cur))) {
              grn_ii_cursor *icur;
              if ((icur = grn_ii_cursor_open(ctx, (grn_ii *)index, id,
                                             GRN_ID_NIL, GRN_ID_MAX, 1, 0))) {
                grn_ii_posting *p;
                while ((p = grn_ii_cursor_next(ctx, icur))) {
                  complete_add_item(ctx, p->rid, res, frequency_threshold,
                                    items_freq, items_boost,
                                    &item_freq, &item_boost);
                }
                grn_ii_cursor_close(ctx, icur);
              }
            }
            grn_table_cursor_close(ctx, cur);
          } else {
            ERR(GRN_UNKNOWN_ERROR, "cannot open cursor for prefix RK search.");
          }
        } else {
          ERR(GRN_UNKNOWN_ERROR, "cannot find index for prefix RK search.");
        }
        grn_str_close(ctx, norm);
      }
      cooccurrence_search(ctx, items, items_boost, tid, res, COMPLETE,
                          frequency_threshold,
                          conditional_probability_threshold);
      if (((prefix_search_mode == GRN_SUGGEST_PREFIX_SEARCH_YES) ||
           (prefix_search_mode == GRN_SUGGEST_PREFIX_SEARCH_AUTO &&
            !grn_table_size(ctx, res))) &&
          (cur = grn_table_cursor_open(ctx, items, TEXT_VALUE_LEN(query),
                                       NULL, 0, 0, -1, GRN_CURSOR_PREFIX))) {
        grn_id id;
        while ((id = grn_table_cursor_next(ctx, cur))) {
          complete_add_item(ctx, id, res, frequency_threshold,
                            items_freq, items_boost, &item_freq, &item_boost);
        }
        grn_table_cursor_close(ctx, cur);
      }
    }
    output(ctx, items, res, tid, sortby, output_columns, offset, limit);
    grn_obj_close(ctx, res);
  } else {
    ERR(GRN_UNKNOWN_ERROR, "cannot create temporary table.");
  }
  GRN_OBJ_FIN(ctx, &item_boost);
  GRN_OBJ_FIN(ctx, &item_freq);
}

static void
correct(grn_ctx *ctx, grn_obj *items, grn_obj *items_boost,
        grn_obj *query, grn_obj *sortby,
        grn_obj *output_columns, int offset, int limit,
        int frequency_threshold, double conditional_probability_threshold)
{
  grn_obj *res;
  grn_obj *items_freq2 = grn_obj_column(ctx, items, CONST_STR_LEN("freq2"));
  grn_obj item_freq2, item_boost;
  GRN_INT32_INIT(&item_freq2, 0);
  GRN_INT32_INIT(&item_boost, 0);
  if ((res = grn_table_create(ctx, NULL, 0, NULL,
                              GRN_TABLE_HASH_KEY|GRN_OBJ_WITH_SUBREC, items, NULL))) {
    grn_id tid = grn_table_get(ctx, items, TEXT_VALUE_LEN(query));
    int32_t max_score;
    max_score = cooccurrence_search(ctx, items, items_boost, tid, res, CORRECT,
                                    frequency_threshold,
                                    conditional_probability_threshold);
    LAP(":", "cooccur(%d)", max_score);
    if (GRN_TEXT_LEN(query) && max_score < frequency_threshold) {
      grn_obj *key, *index;
      if ((key = grn_obj_column(ctx, items, CONST_STR_LEN("_key")))) {
        if (grn_column_index(ctx, key, GRN_OP_MATCH, &index, 1, NULL)) {
          grn_select_optarg optarg;
          memset(&optarg, 0, sizeof(grn_select_optarg));
          optarg.mode = GRN_OP_SIMILAR;
          optarg.similarity_threshold = 0;
          optarg.max_size = 2;
          grn_ii_select(ctx, (grn_ii *)index, TEXT_VALUE_LEN(query),
                        (grn_hash *)res, GRN_OP_OR, &optarg);
          grn_obj_unlink(ctx, index);
          LAP(":", "similar(%d)", grn_table_size(ctx, res));
          {
            grn_hash_cursor *hc = grn_hash_cursor_open(ctx, (grn_hash *)res, NULL,
                                                       0, NULL, 0, 0, -1, 0);
            if (hc) {
              while (grn_hash_cursor_next(ctx, hc)) {
                void *key, *value;
                if (grn_hash_cursor_get_key_value(ctx, hc, &key, NULL, &value)) {
                  grn_id *rp;
                  rp = key;
                  GRN_BULK_REWIND(&item_freq2);
                  GRN_BULK_REWIND(&item_boost);
                  grn_obj_get_value(ctx, items_freq2, *rp, &item_freq2);
                  grn_obj_get_value(ctx, items_boost, *rp, &item_boost);
                  if (GRN_INT32_VALUE(&item_boost) >= 0) {
                    int32_t score;
                    grn_rset_recinfo *ri;
                    score = 1 +
                            (GRN_INT32_VALUE(&item_freq2) >> 4) +
                            GRN_INT32_VALUE(&item_boost);
                    ri = value;
                    ri->score += score;
                    if (score >= frequency_threshold) { continue; }
                  }
                  /* score < frequency_threshold || item_boost < 0 */
                  grn_hash_cursor_delete(ctx, hc, NULL);
                }
              }
              grn_hash_cursor_close(ctx, hc);
            }
          }
          LAP(":", "filter(%d)", grn_table_size(ctx, res));
          {
            /* exec _score -= edit_distance(_key, "query string") for all records */
            grn_obj *var;
            grn_obj *expr;

            GRN_EXPR_CREATE_FOR_QUERY(ctx, res, expr, var);
            if (expr) {
              grn_table_cursor *tc;
              grn_obj *score = grn_obj_column(ctx, res, CONST_STR_LEN("_score"));
              grn_obj *key = grn_obj_column(ctx, res, CONST_STR_LEN("_key"));
              grn_expr_append_obj(ctx, expr,
                                  score,
                                  GRN_OP_GET_VALUE, 1);
              grn_expr_append_obj(ctx, expr,
                                  grn_ctx_get(ctx, CONST_STR_LEN("edit_distance")),
                                  GRN_OP_PUSH, 1);
              grn_expr_append_obj(ctx, expr,
                                  key,
                                  GRN_OP_GET_VALUE, 1);
              grn_expr_append_const(ctx, expr, query, GRN_OP_PUSH, 1);
              grn_expr_append_op(ctx, expr, GRN_OP_CALL, 2);
              grn_expr_append_op(ctx, expr, GRN_OP_MINUS_ASSIGN, 2);

              if ((tc = grn_table_cursor_open(ctx, res, NULL, 0, NULL, 0, 0, -1, 0))) {
                grn_obj score_value;
                GRN_INT32_INIT(&score_value, 0);
                while (!grn_table_cursor_next_o(ctx, tc, var)) {
                  grn_expr_exec(ctx, expr, 0);
                  GRN_BULK_REWIND(&score_value);
                  grn_obj_get_value(ctx, score, GRN_RECORD_VALUE(var),
                                    &score_value);
                  if (GRN_INT32_VALUE(&score_value) < frequency_threshold) {
                    grn_table_cursor_delete(ctx, tc);
                  }
                }
                grn_obj_unlink(ctx, &score_value);
                grn_table_cursor_close(ctx, tc);
              }
              grn_obj_unlink(ctx, score);
              grn_obj_unlink(ctx, key);
              grn_obj_unlink(ctx, expr);
            } else {
              ERR(GRN_UNKNOWN_ERROR,
                  "error on building expr. for calicurating edit distance");
            }
          }
        }
        grn_obj_unlink(ctx, key);
      }
    }
    output(ctx, items, res, tid, sortby, output_columns, offset, limit);
    grn_obj_close(ctx, res);
  } else {
    ERR(GRN_UNKNOWN_ERROR, "cannot create temporary table.");
  }
  GRN_OBJ_FIN(ctx, &item_boost);
  GRN_OBJ_FIN(ctx, &item_freq2);
}

static void
suggest(grn_ctx *ctx, grn_obj *items, grn_obj *items_boost,
        grn_obj *query, grn_obj *sortby,
        grn_obj *output_columns, int offset, int limit,
        int frequency_threshold, double conditional_probability_threshold)
{
  grn_obj *res;
  if ((res = grn_table_create(ctx, NULL, 0, NULL,
                              GRN_TABLE_HASH_KEY|GRN_OBJ_WITH_SUBREC, items, NULL))) {
    grn_id tid = grn_table_get(ctx, items, TEXT_VALUE_LEN(query));
    cooccurrence_search(ctx, items, items_boost, tid, res, SUGGEST,
                        frequency_threshold, conditional_probability_threshold);
    output(ctx, items, res, tid, sortby, output_columns, offset, limit);
    grn_obj_close(ctx, res);
  } else {
    ERR(GRN_UNKNOWN_ERROR, "cannot create temporary table.");
  }
}

static grn_obj *
command_suggest(grn_ctx *ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  grn_obj *items, *col, *items_boost;
  int types;
  int offset = 0;
  int limit = DEFAULT_LIMIT;
  int frequency_threshold = DEFAULT_FREQUENCY_THRESHOLD;
  int prefix_search_len;
  double conditional_probability_threshold =
    DEFAULT_CONDITIONAL_PROBABILITY_THRESHOLD;
  grn_suggest_prefix_search_mode prefix_search_mode;

  types = grn_parse_suggest_types(GRN_TEXT_VALUE(VAR(0)), GRN_BULK_CURR(VAR(0)));
  if (GRN_TEXT_LEN(VAR(6)) > 0) {
    offset = grn_atoi(GRN_TEXT_VALUE(VAR(6)), GRN_BULK_CURR(VAR(6)), NULL);
  }
  if (GRN_TEXT_LEN(VAR(7)) > 0) {
    limit = grn_atoi(GRN_TEXT_VALUE(VAR(7)), GRN_BULK_CURR(VAR(7)), NULL);
  }
  if (GRN_TEXT_LEN(VAR(8)) > 0) {
    frequency_threshold = grn_atoi(GRN_TEXT_VALUE(VAR(8)), GRN_BULK_CURR(VAR(8)), NULL);
  }
  if (GRN_TEXT_LEN(VAR(9)) > 0) {
    GRN_TEXT_PUTC(ctx, VAR(9), '\0');
    conditional_probability_threshold = strtod(GRN_TEXT_VALUE(VAR(9)), NULL);
  }

  prefix_search_len = GRN_TEXT_LEN(VAR(10));
  if (prefix_search_len == 3 &&
      strncasecmp("yes", GRN_TEXT_VALUE(VAR(10)), 3) == 0) {
    prefix_search_mode = GRN_SUGGEST_PREFIX_SEARCH_YES;
  } else if (prefix_search_len == 2 &&
             strncasecmp("no", GRN_TEXT_VALUE(VAR(10)), 2) == 0) {
    prefix_search_mode = GRN_SUGGEST_PREFIX_SEARCH_NO;
  } else {
    prefix_search_mode = GRN_SUGGEST_PREFIX_SEARCH_AUTO;
  }

  if ((items = grn_ctx_get(ctx, TEXT_VALUE_LEN(VAR(1))))) {
    if ((items_boost = grn_obj_column(ctx, items, CONST_STR_LEN("boost")))) {
      GRN_OUTPUT_MAP_OPEN("RESULT_SET", -1);
      if (types & COMPLETE) {
        if ((col = grn_obj_column(ctx, items, TEXT_VALUE_LEN(VAR(2))))) {
          GRN_OUTPUT_CSTR("complete");
          complete(ctx, items, items_boost, col, VAR(3), VAR(4),
                   VAR(5), offset, limit,
                   frequency_threshold, conditional_probability_threshold,
                   prefix_search_mode);
        } else {
          ERR(GRN_INVALID_ARGUMENT, "invalid column.");
        }
      }
      if (types & CORRECT) {
        GRN_OUTPUT_CSTR("correct");
        correct(ctx, items, items_boost, VAR(3), VAR(4),
                VAR(5), offset, limit,
                frequency_threshold, conditional_probability_threshold);
      }
      if (types & SUGGEST) {
        GRN_OUTPUT_CSTR("suggest");
        suggest(ctx, items, items_boost, VAR(3), VAR(4),
                VAR(5), offset, limit,
                frequency_threshold, conditional_probability_threshold);
      }
      GRN_OUTPUT_MAP_CLOSE();
    } else {
      ERR(GRN_INVALID_ARGUMENT, "nonexistent column: <%.*s.boost>",
          GRN_TEXT_LEN(VAR(1)), GRN_TEXT_VALUE(VAR(1)));
    }
    grn_obj_unlink(ctx, items);
  } else {
    ERR(GRN_INVALID_ARGUMENT, "nonexistent table: <%.*s>",
        GRN_TEXT_LEN(VAR(1)), GRN_TEXT_VALUE(VAR(1)));
  }
  return NULL;
}

static grn_obj *
func_suggest_preparer(grn_ctx *ctx, int nargs, grn_obj **args, grn_user_data *user_data)
{
  int r = 0;
  grn_obj *obj;
  if (nargs == 6) {
    grn_obj v1, pre_events;
    grn_id post_event = GRN_RECORD_VALUE(args[0]);
    grn_id post_type = GRN_RECORD_VALUE(args[1]);
    grn_id post_item = GRN_RECORD_VALUE(args[2]);
    grn_id seq = GRN_RECORD_VALUE(args[3]);
    int64_t post_time = GRN_TIME_VALUE(args[4]);
    grn_obj *pairs = args[5];
    if (post_event && post_item && seq) {
      grn_obj *items = grn_ctx_at(ctx, GRN_OBJ_GET_DOMAIN(args[2]));
      grn_obj *items_freq = grn_obj_column(ctx, items, CONST_STR_LEN("freq"));
      grn_obj *items_freq2 = grn_obj_column(ctx, items, CONST_STR_LEN("freq2"));
      grn_obj *items_last = grn_obj_column(ctx, items, CONST_STR_LEN("last"));
      grn_obj *seqs = grn_ctx_at(ctx, GRN_OBJ_GET_DOMAIN(args[3]));
      grn_obj *seqs_events = grn_obj_column(ctx, seqs, CONST_STR_LEN("events"));
      grn_obj *events = grn_ctx_at(ctx, grn_obj_get_range(ctx, seqs_events));
      grn_obj *events_type = grn_obj_column(ctx, events, CONST_STR_LEN("type"));
      grn_obj *events_time = grn_obj_column(ctx, events, CONST_STR_LEN("time"));
      grn_obj *events_item = grn_obj_column(ctx, events, CONST_STR_LEN("item"));
      grn_obj *pairs_pre = grn_obj_column(ctx, pairs, CONST_STR_LEN("pre"));
      grn_obj *pairs_post = grn_obj_column(ctx, pairs, CONST_STR_LEN("post"));
      grn_obj *pairs_freq0 = grn_obj_column(ctx, pairs, CONST_STR_LEN("freq0"));
      grn_obj *pairs_freq1 = grn_obj_column(ctx, pairs, CONST_STR_LEN("freq1"));
      grn_obj *pairs_freq2 = grn_obj_column(ctx, pairs, CONST_STR_LEN("freq2"));
      GRN_UINT32_INIT(&v1, 0);
      GRN_UINT32_SET(ctx, &v1, 1);
      GRN_RECORD_INIT(&pre_events, 0, grn_obj_id(ctx, events));
      grn_obj_set_value(ctx, items_freq, post_item, &v1, GRN_OBJ_INCR);
      grn_obj_set_value(ctx, items_last, post_item, args[4], GRN_OBJ_SET);
      if (post_type) {
        int added;
        grn_id pid, tid, *ep, *es;
        grn_obj pre_type, pre_time, pre_item;
        uint64_t key, key_ = ((uint64_t)post_item) << 32;
        grn_obj_set_value(ctx, items_freq2, post_item, &v1, GRN_OBJ_INCR);
        grn_obj_get_value(ctx, seqs_events, seq, &pre_events);
        ep = (grn_id *)GRN_BULK_CURR(&pre_events);
        es = (grn_id *)GRN_BULK_HEAD(&pre_events);
        GRN_RECORD_INIT(&pre_type, 0, grn_obj_get_range(ctx, events_type));
        GRN_TIME_INIT(&pre_time, 0);
        GRN_RECORD_INIT(&pre_item, 0, grn_obj_get_range(ctx, events_item));
        while (es < ep--) {
          GRN_BULK_REWIND(&pre_type);
          GRN_BULK_REWIND(&pre_time);
          GRN_BULK_REWIND(&pre_item);
          grn_obj_get_value(ctx, events_type, *ep, &pre_type);
          grn_obj_get_value(ctx, events_time, *ep, &pre_time);
          grn_obj_get_value(ctx, events_item, *ep, &pre_item);
          if (GRN_TIME_VALUE(&pre_time) + 60 * GRN_TIME_USEC_PER_SEC < post_time) {
            r = (int)((post_time - GRN_TIME_VALUE(&pre_time))/GRN_TIME_USEC_PER_SEC);
            break;
          }
          key = key_ + GRN_RECORD_VALUE(&pre_item);
          pid = grn_table_add(ctx, pairs, &key, sizeof(uint64_t), &added);
          if (added) {
            grn_obj_set_value(ctx, pairs_pre, pid, &pre_item, GRN_OBJ_SET);
            grn_obj_set_value(ctx, pairs_post, pid, args[2], GRN_OBJ_SET);
          }
          if (GRN_RECORD_VALUE(&pre_type)) {
            grn_obj_set_value(ctx, pairs_freq1, pid, &v1, GRN_OBJ_INCR);
            break;
          } else {
            grn_obj_set_value(ctx, pairs_freq0, pid, &v1, GRN_OBJ_INCR);
          }
        }
        {
          char keybuf[GRN_TABLE_MAX_KEY_SIZE];
          int keylen = grn_table_get_key(ctx, items, post_item,
                                         keybuf, GRN_TABLE_MAX_KEY_SIZE);
          grn_token *token = grn_token_open(ctx, items, keybuf, keylen, 1);
          if (token) {
            while ((tid = grn_token_next(ctx, token)) && tid != post_item) {
              key = key_ + tid;
              pid = grn_table_add(ctx, pairs, &key, sizeof(uint64_t), &added);
              if (added) {
                GRN_RECORD_SET(ctx, &pre_item, tid);
                grn_obj_set_value(ctx, pairs_pre, pid, &pre_item, GRN_OBJ_SET);
                grn_obj_set_value(ctx, pairs_post, pid, args[2], GRN_OBJ_SET);
              }
              grn_obj_set_value(ctx, pairs_freq2, pid, &v1, GRN_OBJ_INCR);
            }
            grn_token_close(ctx, token);
          }
        }
        GRN_OBJ_FIN(ctx, &pre_type);
        GRN_OBJ_FIN(ctx, &pre_time);
        GRN_OBJ_FIN(ctx, &pre_item);
        GRN_BULK_REWIND(&pre_events);
      }
      GRN_RECORD_SET(ctx, &pre_events, post_event);
      grn_obj_set_value(ctx, seqs_events, seq, &pre_events, GRN_OBJ_APPEND);
      GRN_OBJ_FIN(ctx, &pre_events);
      GRN_OBJ_FIN(ctx, &v1);
    }
  }
  if ((obj = GRN_PROC_ALLOC(GRN_DB_UINT32, 0))) { GRN_UINT32_SET(ctx, obj, r); }
  return obj;
}

grn_rc
GRN_PLUGIN_INIT(grn_ctx *ctx)
{
  return GRN_SUCCESS;
}

grn_rc
GRN_PLUGIN_REGISTER(grn_ctx *ctx)
{
  grn_expr_var vars[] = {
    {CONST_STR_LEN("types")},
    {CONST_STR_LEN("table")},
    {CONST_STR_LEN("column")},
    {CONST_STR_LEN("query")},
    {CONST_STR_LEN("sortby")},
    {CONST_STR_LEN("output_columns")},
    {CONST_STR_LEN("offset")},
    {CONST_STR_LEN("limit")},
    {CONST_STR_LEN("frequency_threshold")},
    {CONST_STR_LEN("conditional_probability_threshold")},
    {CONST_STR_LEN("prefix_search")}
  };
  GRN_TEXT_INIT(&vars[0].value, 0);
  GRN_TEXT_INIT(&vars[1].value, 0);
  GRN_TEXT_INIT(&vars[2].value, 0);
  GRN_TEXT_INIT(&vars[3].value, 0);
  GRN_TEXT_INIT(&vars[4].value, 0);
  GRN_TEXT_INIT(&vars[5].value, 0);
  GRN_TEXT_INIT(&vars[6].value, 0);
  GRN_TEXT_INIT(&vars[7].value, 0);
  GRN_TEXT_INIT(&vars[8].value, 0);
  GRN_TEXT_INIT(&vars[9].value, 0);
  GRN_TEXT_INIT(&vars[10].value, 0);
  grn_proc_create(ctx, CONST_STR_LEN("suggest"), GRN_PROC_COMMAND,
                  command_suggest, NULL, NULL, 11, vars);

  grn_proc_create(ctx, CONST_STR_LEN("suggest_preparer"), GRN_PROC_FUNCTION,
                  func_suggest_preparer, NULL, NULL, 0, NULL);
  return ctx->rc;
}

grn_rc
GRN_PLUGIN_FIN(grn_ctx *ctx)
{
  return GRN_SUCCESS;
}
