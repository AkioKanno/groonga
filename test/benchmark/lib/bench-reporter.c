/* -*- c-basic-offset: 2; coding: utf-8 -*- */
/*
  Copyright (C) 2008  Kouhei Sutou <kou@cozmixng.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>

#include "bench-reporter.h"

typedef struct _BenchItem BenchItem;
struct _BenchItem
{
  gchar *label;
  gint n;
  BenchSetupFunc bench_setup;
  BenchFunc bench;
  BenchTeardownFunc bench_teardown;
  gpointer data;
};

static BenchItem *
bench_item_new(const gchar *label, gint n,
               BenchSetupFunc bench_setup,
               BenchFunc bench,
               BenchTeardownFunc bench_teardown,
               gpointer data)
{
  BenchItem *item;

  item = g_slice_new(BenchItem);

  item->label = g_strdup(label);
  item->n = n;
  item->bench_setup = bench_setup;
  item->bench = bench;
  item->bench_teardown = bench_teardown;
  item->data = data;

  return item;
}

static void
bench_item_free(BenchItem *item)
{
  if (item->label)
    g_free(item->label);

  g_slice_free(BenchItem, item);
}

#define BENCH_REPORTER_GET_PRIVATE(obj)                                 \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj),                                   \
                               BENCH_TYPE_REPORTER,                     \
                               BenchReporterPrivate))

typedef struct _BenchReporterPrivate BenchReporterPrivate;
struct _BenchReporterPrivate
{
  GList *items;
};

G_DEFINE_TYPE(BenchReporter, bench_reporter, G_TYPE_OBJECT)

static void dispose        (GObject         *object);

static void
bench_reporter_class_init(BenchReporterClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->dispose = dispose;

  g_type_class_add_private(gobject_class, sizeof(BenchReporterPrivate));
}

static void
bench_reporter_init(BenchReporter *reporter)
{
  BenchReporterPrivate *priv;

  priv = BENCH_REPORTER_GET_PRIVATE(reporter);

  priv->items = NULL;
}

static void
dispose(GObject *object)
{
  BenchReporterPrivate *priv;

  priv = BENCH_REPORTER_GET_PRIVATE(object);

  if (priv->items) {
    g_list_foreach(priv->items, (GFunc)bench_item_free, NULL);
    g_list_free(priv->items);
    priv->items = NULL;
  }

  G_OBJECT_CLASS(bench_reporter_parent_class)->dispose(object);
}

BenchReporter *
bench_reporter_new(void)
{
  return g_object_new(BENCH_TYPE_REPORTER, NULL);
}

void
bench_reporter_register(BenchReporter *reporter,
                        const gchar *label, gint n,
                        BenchSetupFunc bench_setup,
                        BenchFunc bench,
                        BenchTeardownFunc bench_teardown,
                        gpointer data)
{
  BenchReporterPrivate *priv;

  priv = BENCH_REPORTER_GET_PRIVATE(reporter);

  priv->items = g_list_append(priv->items, bench_item_new(label, n,
                                                          bench_setup,
                                                          bench,
                                                          bench_teardown,
                                                          data));
}

#define INDENT "  "

static void
print_header(BenchReporterPrivate *priv, gint max_label_length)
{
  gint n_spaces;

  g_print(INDENT);
  for (n_spaces = max_label_length + strlen(": ");
       n_spaces > 0;
       n_spaces--) {
    g_print(" ");
  }
  g_print("(time)\n");
}

static void
print_label(BenchReporterPrivate *priv, BenchItem *item, gint max_label_length)
{
  gint n_left_spaces;

  g_print(INDENT);
  if (item->label) {
    n_left_spaces = max_label_length - strlen(item->label);
  } else {
    n_left_spaces = max_label_length;
  }
  for (; n_left_spaces > 0; n_left_spaces--) {
    g_print(" ");
  }
  if (item->label)
    g_print("%s", item->label);
  g_print(": ");
}

static void
run_item(BenchReporterPrivate *priv, BenchItem *item, gint max_label_length)
{
  GTimer *timer;
  gint i;

  print_label(priv, item, max_label_length);

  timer = g_timer_new();
  g_timer_stop(timer);
  g_timer_reset(timer);
  for (i = 0; i < item->n; i++) {
    if (item->bench_setup)
      item->bench_setup(item->data);
    g_timer_continue(timer);
    item->bench(item->data);
    g_timer_stop(timer);
    if (item->bench_teardown)
      item->bench_teardown(item->data);
  }

  g_print("(%g)\n", g_timer_elapsed(timer, NULL));

  g_timer_destroy(timer);
}

void
bench_reporter_run(BenchReporter *reporter)
{
  BenchReporterPrivate *priv;
  GList *node;
  gint max_label_length = 0;

  priv = BENCH_REPORTER_GET_PRIVATE(reporter);
  for (node = priv->items; node; node = g_list_next(node)) {
    BenchItem *item = node->data;

    if (item->label)
      max_label_length = MAX(max_label_length, strlen(item->label));
  }

  print_header(priv, max_label_length);
  for (node = priv->items; node; node = g_list_next(node)) {
    BenchItem *item = node->data;

    run_item(priv, item, max_label_length);
  }
}
