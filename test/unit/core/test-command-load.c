/* -*- c-basic-offset: 2; coding: utf-8 -*- */
/*
  Copyright (C) 2010-2012  Kouhei Sutou <kou@clear-code.com>

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

#include <gcutter.h>
#include <glib/gstdio.h>

#include "../lib/grn-assertions.h"

#include <str.h>

void test_columns(void);
void attributes_bool(void);
void data_bool(void);
void test_bool(gconstpointer data);
void test_int32_key(void);
void test_time_float(void);
void data_null(void);
void test_null(gconstpointer data);
void test_index_geo_point(void);
void test_nonexistent_columns(void);
void test_no_key_table(void);
void test_two_bigram_indexes_to_key(void);
void test_invalid_start_with_symbol(void);
void test_no_key_table_without_columns(void);
void test_nonexistent_table_name(void);
void test_invalid_table_name(void);
void data_each(void);
void test_each(gconstpointer data);
void test_vector_reference_column(void);
void test_invalid_int32_value(void);
void data_valid_geo_point_valid(void);
void test_valid_geo_point_value(gconstpointer data);
void data_invalid_geo_point_valid(void);
void test_invalid_geo_point_value(gconstpointer data);

static gchar *tmp_directory;
static const gchar *database_path;

static grn_ctx *context;
static grn_obj *database;

void
cut_startup(void)
{
  tmp_directory = g_build_filename(grn_test_get_tmp_dir(),
                                   "load",
                                   NULL);
}

void
cut_shutdown(void)
{
  g_free(tmp_directory);
}

static void
remove_tmp_directory(void)
{
  cut_remove_path(tmp_directory, NULL);
}

void
cut_setup(void)
{
  remove_tmp_directory();
  g_mkdir_with_parents(tmp_directory, 0700);

  context = g_new0(grn_ctx, 1);
  grn_ctx_init(context, 0);

  database_path = cut_build_path(tmp_directory, "database.groonga", NULL);
  database = grn_db_create(context, database_path, NULL);
}

void
cut_teardown(void)
{
  if (context) {
    grn_obj_close(context, database);
    grn_ctx_fin(context);
    g_free(context);
  }

  remove_tmp_directory();
}

void
test_columns(void)
{
  assert_send_command("table_create Users TABLE_HASH_KEY ShortText");
  assert_send_command("column_create Users name COLUMN_SCALAR ShortText");
  assert_send_command("column_create Users desc COLUMN_SCALAR ShortText");
  cut_assert_equal_string(
    "2",
    send_command("load --table Users --columns '_key,name,desc'\n"
                 "[\n"
                 "  [\"mori\", \"モリ\", \"タポ\"],\n"
                 "  [\"tapo\", \"タポ\", \"モリモリモリタポ\"]\n"
                 "]"));
  cut_assert_equal_string("[[[2],"
                          "["
                          "[\"_id\",\"UInt32\"],"
                          "[\"_key\",\"ShortText\"],"
                          "[\"desc\",\"ShortText\"],"
                          "[\"name\",\"ShortText\"]"
                          "],"
                          "[1,\"mori\",\"タポ\",\"モリ\"],"
                          "[2,\"tapo\",\"モリモリモリタポ\",\"タポ\"]"
                          "]]",
                          send_command("select Users"));
}

void
attributes_bool(void)
{
  cut_set_attributes("bug", "123, 304",
                     NULL);
}

void
data_bool(void)
{
#define ADD_DATUM(label, load_command)                          \
  gcut_add_datum(label,                                         \
                 "load-command", G_TYPE_STRING, load_command,   \
                 NULL)

  ADD_DATUM("symbol",
            "load --table Users --columns '_key,enabled'\n"
            "[\n"
            "  [\"mori\",true],\n"
            "  [\"tapo\",false]\n"
            "]");
  ADD_DATUM("number",
            "load --table Users --columns '_key,enabled'\n"
            "[\n"
            "  [\"mori\",1],\n"
            "  [\"tapo\",0]\n"
            "]");
  ADD_DATUM("string",
            "load --table Users --columns '_key,enabled'\n"
            "[\n"
            "  [\"mori\",\"1\"],\n"
            "  [\"tapo\",\"\"]\n"
            "]");
  ADD_DATUM("string (null)",
            "load --table Users --columns '_key,enabled'\n"
            "[\n"
            "  [\"mori\",\"0\"],\n"
            "  [\"tapo\",null]\n"
            "]");

#undef ADD_DATUM
}

void
test_bool(gconstpointer data)
{
  assert_send_command("table_create Users TABLE_HASH_KEY ShortText");
  assert_send_command("column_create Users enabled COLUMN_SCALAR Bool");
  cut_assert_equal_string("2",
                          send_command(gcut_data_get_string(data,
                                                            "load-command")));
  cut_assert_equal_string("[[[2],"
                          "["
                          "[\"_id\",\"UInt32\"],"
                          "[\"_key\",\"ShortText\"],"
                          "[\"enabled\",\"Bool\"]"
                          "],"
                          "[1,\"mori\",true],"
                          "[2,\"tapo\",false]"
                          "]]",
                          send_command("select Users"));
}

void
test_int32_key(void)
{
  assert_send_command("table_create Students TABLE_HASH_KEY Int32");
  assert_send_command("column_create Students name COLUMN_SCALAR ShortText");
  cut_assert_equal_string(
    "1",
    send_command("load --table Students\n"
                 "[{\"_key\": 1, \"name\": \"morita\"}]"));
  cut_assert_equal_string("[[[1],"
                          "["
                          "[\"_id\",\"UInt32\"],"
                          "[\"_key\",\"Int32\"],"
                          "[\"name\",\"ShortText\"]"
                          "],"
                          "[1,1,\"morita\"]"
                          "]]",
                          send_command("select Students"));
}

void
test_time_float(void)
{
  assert_send_command("table_create Logs TABLE_NO_KEY");
  assert_send_command("column_create Logs time_stamp COLUMN_SCALAR Time");
  cut_assert_equal_string("1",
                          send_command("load --table Logs\n"
                                       "[{\"time_stamp\": 1295851581.41798}]"));
  cut_assert_equal_string("[[[1],"
                          "["
                          "[\"_id\",\"UInt32\"],"
                          "[\"time_stamp\",\"Time\"]"
                          "],"
                          "[1,1295851581.41798]"
                          "]]",
                          send_command("select Logs"));
}

void
data_null(void)
{
#define ADD_DATUM(label, expected, load)                        \
  gcut_add_datum(label,                                         \
                 "expected", G_TYPE_STRING, expected,           \
                 "load", G_TYPE_STRING, load,                   \
                 NULL)

  ADD_DATUM("string - null",
            "[[[1],"
              "[[\"_id\",\"UInt32\"],"
               "[\"_key\",\"ShortText\"],"
               "[\"nick\",\"ShortText\"],"
               "[\"scores\",\"Int32\"]],"
             "[1,\"Daijiro MORI\",\"\",[5,5,5]]]]",
            "load --table Students --columns '_key, nick'\n"
            "[\n"
            "  [\"Daijiro MORI\", null]\n"
            "]");
  ADD_DATUM("string - empty string",
            "[[[1],"
              "[[\"_id\",\"UInt32\"],"
               "[\"_key\",\"ShortText\"],"
               "[\"nick\",\"ShortText\"],"
               "[\"scores\",\"Int32\"]],"
             "[1,\"Daijiro MORI\",\"\",[5,5,5]]]]",
            "load --table Students --columns '_key, nick'\n"
            "[\n"
            "  [\"Daijiro MORI\", \"\"]\n"
            "]");

  ADD_DATUM("vector - empty null",
            "[[[1],"
              "[[\"_id\",\"UInt32\"],"
               "[\"_key\",\"ShortText\"],"
               "[\"nick\",\"ShortText\"],"
               "[\"scores\",\"Int32\"]],"
             "[1,\"Daijiro MORI\",\"morita\",[]]]]",
            "load --table Students --columns '_key, scores'\n"
            "[\n"
            "  [\"Daijiro MORI\", null]\n"
            "]");
  ADD_DATUM("vector - empty string",
            "[[[1],"
              "[[\"_id\",\"UInt32\"],"
               "[\"_key\",\"ShortText\"],"
               "[\"nick\",\"ShortText\"],"
               "[\"scores\",\"Int32\"]],"
             "[1,\"Daijiro MORI\",\"morita\",[]]]]",
            "load --table Students --columns '_key, scores'\n"
            "[\n"
            "  [\"Daijiro MORI\", \"\"]\n"
            "]");
  ADD_DATUM("vector - empty array",
            "[[[1],"
              "[[\"_id\",\"UInt32\"],"
               "[\"_key\",\"ShortText\"],"
               "[\"nick\",\"ShortText\"],"
               "[\"scores\",\"Int32\"]],"
             "[1,\"Daijiro MORI\",\"morita\",[]]]]",
            "load --table Students --columns '_key, scores'\n"
            "[\n"
            "  [\"Daijiro MORI\", []]\n"
            "]");

#undef ADD_DATUM
}

void
test_null(gconstpointer data)
{
  assert_send_command("table_create Students TABLE_HASH_KEY ShortText");
  assert_send_command("column_create Students nick COLUMN_SCALAR ShortText");
  assert_send_command("column_create Students scores COLUMN_VECTOR Int32");

  cut_assert_equal_string("1",
                          send_command("load --table Students\n"
                                       "[{\"_key\": \"Daijiro MORI\", "
                                         "\"nick\": \"morita\", "
                                         "\"scores\": [5, 5, 5]}]"));
  cut_assert_equal_string("1",
                          send_command(gcut_data_get_string(data, "load")));
  cut_assert_equal_string(gcut_data_get_string(data, "expected"),
                          send_command("select Students"));
}

void
test_index_geo_point(void)
{
  const gchar *query =
    "select Shops "
    "--filter 'geo_in_circle(location, \"128514964x502419287\", 1)'";
  const gchar *hit_result =
    "[[[1],"
    "[[\"_id\",\"UInt32\"],"
    "[\"_key\",\"ShortText\"],"
    "[\"location\",\"WGS84GeoPoint\"]],"
    "[1,\"たかね\",\"128514964x502419287\"]]]";
  const gchar *no_hit_result =
    "[[[0],"
    "[[\"_id\",\"UInt32\"],"
    "[\"_key\",\"ShortText\"],"
    "[\"location\",\"WGS84GeoPoint\"]]]]";
  const gchar *load_takane =
    "load --table Shops\n"
    "[{\"_key\": \"たかね\", \"location\": \"128514964x502419287\"}]";
  const gchar *load_takane_with_empty_location =
    "load --table Shops\n"
    "[{\"_key\": \"たかね\", \"location\": null}]";

  assert_send_command("table_create Shops TABLE_HASH_KEY ShortText");
  assert_send_command("column_create Shops location COLUMN_SCALAR WGS84GeoPoint");
  assert_send_command("table_create Locations TABLE_PAT_KEY WGS84GeoPoint");
  assert_send_command("column_create Locations shop COLUMN_INDEX Shops location");

  cut_assert_equal_string("1",
                          send_command(load_takane));
  cut_assert_equal_string(hit_result,
                          send_command(query));

  cut_assert_equal_string("1",
                          send_command(load_takane_with_empty_location));
  cut_assert_equal_string(no_hit_result,
                          send_command(query));

  cut_assert_equal_string("1",
                          send_command(load_takane));
  cut_assert_equal_string(hit_result,
                          send_command(query));
}

void
test_nonexistent_columns(void)
{
  assert_send_command("table_create Users TABLE_NO_KEY");
  assert_send_command("column_create Users name COLUMN_SCALAR ShortText");
  grn_test_assert_send_command_error(context,
                                     GRN_INVALID_ARGUMENT,
                                     "nonexistent column: <nonexistent>",
                                     "load "
                                     "--table Users "
                                     "--columns nonexistent "
                                     "--values "
                                     "'[[\"nonexistent column value\"]]'");
}

void
test_no_key_table(void)
{
  assert_send_command("table_create Users TABLE_NO_KEY");
  assert_send_command("column_create Users name COLUMN_SCALAR ShortText");
  assert_send_command("column_create Users desc COLUMN_SCALAR ShortText");
  cut_assert_equal_string("2",
                          send_command("load "
                                       "--table Users "
                                       "--columns 'name, desc' "
                                       "--values "
                                       "'[[\"mori\", \"the author of groonga\"],"
                                       "[\"gunyara-kun\", \"co-author\"]]'"));
}

void
test_two_bigram_indexes_to_key(void)
{
  cut_omit("crashed!!!");
  assert_send_command("table_create Books TABLE_PAT_KEY ShortText");
  assert_send_command("table_create Authors TABLE_PAT_KEY ShortText");
  assert_send_command("table_create Bigram "
                      "TABLE_PAT_KEY|KEY_NORMALIZE ShortText "
                      "--default_tokenizer TokenBigram");
  assert_send_command("table_create BigramLoose "
                      "TABLE_PAT_KEY|KEY_NORMALIZE ShortText "
                      "--default_tokenizer TokenBigramIgnoreBlankSplitSymbolAlphaDigit");
  assert_send_command("column_create Books author COLUMN_SCALAR Authors");
  assert_send_command("column_create Bigram author "
                      "COLUMN_INDEX|WITH_POSITION Authors _key");
  assert_send_command("column_create BigramLoose author "
                      "COLUMN_INDEX|WITH_POSITION Authors _key");
  cut_assert_equal_string(
    "2",
    send_command("load "
                 "--table Books "
                 "--columns '_key, author' "
                 "--values "
                 "'["
                 "[\"The groonga book\", \"mori\"],"
                 "[\"The first groonga\", \"morita\"]"
                 "]'"));
  cut_assert_equal_string(
    "[[[2],"
    "[[\"_key\",\"ShortText\"],"
    "[\"author\",\"Authors\"],"
    "[\"_score\",\"Int32\"]],"
    "[\"The groonga book\",\"mori\",11],"
    "[\"The first groonga\",\"morita\",1]]]",
    send_command("select Books "
                 "--match_columns "
                 "'Bigram.author * 10 || BigramLoose.author * 1' "
                 "--query 'mori' "
                 "--output_columns '_key, author, _score'"));
}

void
test_invalid_start_with_symbol(void)
{
  const gchar *table_list_result;

  table_list_result =
    cut_take_printf("["
                    "[[\"id\",\"UInt32\"],"
                     "[\"name\",\"ShortText\"],"
                     "[\"path\",\"ShortText\"],"
                     "[\"flags\",\"ShortText\"],"
                     "[\"domain\",\"ShortText\"],"
                     "[\"range\",\"ShortText\"]],"
                    "[256,"
                     "\"Authors\","
                     "\"%s.0000100\","
                     "\"TABLE_PAT_KEY|PERSISTENT\","
                     "\"ShortText\","
                     "\"null\"]]",
                    database_path);

  assert_send_command("table_create Authors TABLE_PAT_KEY ShortText");
  cut_assert_equal_string(table_list_result, send_command("table_list"));
  grn_test_assert_send_command_error(context,
                                     GRN_INVALID_ARGUMENT,
                                     "JSON must start with '[' or '{': <invalid>",
                                     "load "
                                     "--table Authors "
                                     "--columns '_key' "
                                     "--values 'invalid'");
  cut_assert_equal_string(table_list_result, send_command("table_list"));
}

void
test_no_key_table_without_columns(void)
{
  assert_send_command("table_create Numbers TABLE_NO_KEY");
  grn_test_assert_send_command_error(context,
                                     GRN_INVALID_ARGUMENT,
                                     "column name must be string: <1>",
                                     "load --table Numbers\n"
                                     "[\n"
                                     "[1],\n"
                                     "[2],\n"
                                     "[3]\n"
                                     "]");
}

void
test_nonexistent_table_name(void)
{
  grn_test_assert_send_command_error(context,
                                     GRN_INVALID_ARGUMENT,
                                     "nonexistent table: <Users>",
                                     "load --table Users\n"
                                     "[\n"
                                     "[\"_key\": \"alice\"]\n"
                                     "]");
}

void
test_invalid_table_name(void)
{
  grn_test_assert_send_command_error(
    context,
    GRN_INVALID_ARGUMENT,
    "[table][load]: name can't start with '_' "
    "and contains only 0-9, A-Z, a-z, #, - or _: <_Users>",
    "load --table _Users\n"
    "[\n"
    "{\"_key\": \"alice\"}\n"
    "]");
}

void
data_each(void)
{
#define ADD_DATUM(label, load_values_format)                            \
  gcut_add_datum(label,                                                 \
                 "load-values-format", G_TYPE_STRING, load_values_format, \
                 NULL)

  ADD_DATUM("brace",
            "{\"_key\": \"alice\", \"location\": \"%s\"},"
            "{\"_key\": \"bob\", \"location\": \"%s\"}");

  ADD_DATUM("bracket",
            "[\"_key\", \"location\"],"
            "[\"alice\", \"%s\"],"
            "[\"bob\", \"%s\"]");

#undef ADD_DATUM
}

void
test_each(gconstpointer data)
{
  gdouble tokyo_tocho_latitude = 35.689444;
  gdouble tokyo_tocho_longitude = 139.691667;
  gdouble yurakucho_latitude = 35.67487;
  gdouble yurakucho_longitude = 139.76352;
  gdouble asagaya_latitude = 35.70452;
  gdouble asagaya_longitude = 139.6351;

  assert_send_command("table_create Users TABLE_HASH_KEY ShortText");
  assert_send_command("column_create Users location "
                      "COLUMN_SCALAR WGS84GeoPoint");
  assert_send_command("column_create Users distance_from_tokyo_tocho "
                      "COLUMN_SCALAR UInt32");
  cut_assert_equal_string(
    "2",
    send_command(
      cut_take_printf(
        "load "
        "--table Users "
        "--each 'distance_from_tokyo_tocho = geo_distance(location, \"%s\")' "
        "--values '[%s]'",
        grn_test_location_string(tokyo_tocho_latitude, tokyo_tocho_longitude),
        cut_take_printf(gcut_data_get_string(data, "load-values-format"),
                        grn_test_location_string(yurakucho_latitude,
                                                 yurakucho_longitude),
                        grn_test_location_string(asagaya_latitude,
                                                 asagaya_longitude)))));
  cut_assert_equal_string(
    "[[[2],"
     "[[\"_id\",\"UInt32\"],"
      "[\"_key\",\"ShortText\"],"
      "[\"distance_from_tokyo_tocho\",\"UInt32\"],"
      "[\"location\",\"WGS84GeoPoint\"]],"
     "[1,\"alice\",6674,\"128429532x503148672\"],"
     "[2,\"bob\",5364,\"128536272x502686360\"]]]",
    send_command("select Users"));
}

void
test_vector_reference_column(void)
{
  assert_send_command("table_create Tags TABLE_HASH_KEY ShortText "
                      "--default_tokenizer TokenDelimit");
  assert_send_command("table_create Users TABLE_HASH_KEY ShortText");
  assert_send_command("column_create Users tags COLUMN_VECTOR Tags");
  cut_assert_equal_string(
    "1",
    send_command(
      "load "
      "--table Users "
      "--values '[{\"_key\": \"mori\", \"tags\": \"groonga search engine\"}]'"));
  cut_assert_equal_string(
    "[[[1],"
     "[[\"_id\",\"UInt32\"],"
      "[\"_key\",\"ShortText\"],"
      "[\"tags\",\"Tags\"]],"
     "[1,\"mori\",[\"groonga\",\"search\",\"engine\"]]]]",
    send_command("select Users"));
}

void
test_invalid_int32_value(void)
{
  assert_send_command("table_create Users TABLE_NO_KEY");
  assert_send_command("column_create Users age COLUMN_SCALAR Int32");
  grn_test_assert_send_command_error(
    context,
    GRN_INVALID_ARGUMENT,
    "<Users.age>: failed to cast to <Int32>: <\"invalid number!\">",
    "load --table Users\n"
    "[\n"
    "{\"age\": \"invalid number!\"}\n"
    "]\n");
  send_command("]\n");
  cut_assert_equal_string(
    "[[[1],"
     "[[\"_id\",\"UInt32\"],"
      "[\"age\",\"Int32\"]],"
     "[1,0]]]",
    send_command("select Users"));
}

void
data_valid_geo_point_value(void)
{
#define ADD_DATUM(label, location)                                      \
  gcut_add_datum(label " (" location ")",                               \
                 "location", G_TYPE_STRING, location,                   \
                 NULL)

  ADD_DATUM("too large latitude", "324000000x502419287");
  ADD_DATUM("too small latitude", "-324000000x502419287");
  ADD_DATUM("too large longitude", "128514964x648000000");
  ADD_DATUM("too small longitude", "128514964x-648000000");

#undef ADD_DATUM
}

void
test_valid_geo_point_value(gconstpointer data)
{
  assert_send_command("table_create Shops TABLE_HASH_KEY ShortText");
  assert_send_command("column_create Shops location COLUMN_SCALAR WGS84GeoPoint");
  cut_assert_equal_string(
    "1",
    send_command(
      cut_take_printf("load --table Shops\n"
                      "[\n"
                      "{\"_key\": \"たかね\", \"location\": \"%s\"}\n"
                      "]\n",
                      gcut_data_get_string(data, "location"))));
  cut_assert_equal_string(
    cut_take_printf("[[[1],"
                    "[[\"_id\",\"UInt32\"],"
                    "[\"_key\",\"ShortText\"],"
                    "[\"location\",\"WGS84GeoPoint\"]],"
                    "[1,\"たかね\",\"%s\"]]]",
                    gcut_data_get_string(data, "location")),
    send_command("select Shops"));
}

void
data_invalid_geo_point_value(void)
{
#define ADD_DATUM(label, location)                                      \
  gcut_add_datum(label " (" location ")",                               \
                 "location", G_TYPE_STRING, location,                   \
                 NULL)

  ADD_DATUM("too large latitude", "324000001x502419287");
  ADD_DATUM("too small latitude", "-324000001x502419287");
  ADD_DATUM("too large longitude", "128514964x648000001");
  ADD_DATUM("too small longitude", "128514964x-648000001");

#undef ADD_DATUM
}

void
test_invalid_geo_point_value(gconstpointer data)
{
  assert_send_command("table_create Shops TABLE_HASH_KEY ShortText");
  assert_send_command("column_create Shops location COLUMN_SCALAR WGS84GeoPoint");
  grn_test_assert_send_command_error(
    context,
    GRN_INVALID_ARGUMENT,
    cut_take_printf("<Shops.location>: "
                    "failed to cast to <WGS84GeoPoint>: <\"%s\">",
                    gcut_data_get_string(data, "location")),
    cut_take_printf("load --table Shops\n"
                    "[\n"
                    "{\"_key\": \"たかね\", \"location\": \"%s\"}\n"
                    "]\n",
                    gcut_data_get_string(data, "location")));
  send_command("]\n");
  cut_assert_equal_string(
    "[[[1],"
     "[[\"_id\",\"UInt32\"],"
      "[\"_key\",\"ShortText\"],"
      "[\"location\",\"WGS84GeoPoint\"]],"
     "[1,\"たかね\",\"0x0\"]]]",
    send_command("select Shops"));
}
