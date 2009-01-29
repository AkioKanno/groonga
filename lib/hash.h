/* -*- c-basic-offset: 2 -*- */
/* Copyright(C) 2009 Brazil

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
#ifndef GRN_HASH_H
#define GRN_HASH_H

#ifndef GROONGA_H
#include "groonga_in.h"
#endif /* GROONGA_H */

#ifndef GRN_CTX_H
#include "ctx.h"
#endif /* GRN_CTX_H */

#ifndef GRN_DB_H
#include "db.h"
#endif /* GRN_DB_H */

#ifdef	__cplusplus
extern "C" {
#endif

/**** grn_tiny_array ****/

#define GRN_TINY_ARRAY_CLEAR      (1L<<0)
#define GRN_TINY_ARRAY_THREADSAFE (1L<<1)
#define GRN_TINY_ARRAY_USE_MALLOC (1L<<2)

#define GRN_TINY_ARRAY_W 0
#define GRN_TINY_ARRAY_R(i) (1<<((i)<<GRN_TINY_ARRAY_W))
#define GRN_TINY_ARRAY_S (GRN_TINY_ARRAY_R(1)-1)
#define GRN_TINY_ARRAY_N (32>>GRN_TINY_ARRAY_W)

typedef struct _grn_tiny_array grn_tiny_array;

struct _grn_tiny_array {
  grn_ctx *ctx;
  grn_id max;
  uint16_t element_size;
  uint16_t flags;
  grn_mutex lock;
  void *elements[GRN_TINY_ARRAY_N];
};

#define GRN_TINY_ARRAY_EACH(a,head,tail,key,val,block) do {\
  int _ei;\
  const grn_id h = (head);\
  const grn_id t = (tail);\
  for (_ei = 0, (key) = (h); _ei < GRN_TINY_ARRAY_N && (key) <= (t); _ei++) {\
    int _ej = GRN_TINY_ARRAY_S * GRN_TINY_ARRAY_R(_ei);\
    if (((val) = (a)->elements[_ei])) {\
      for (; _ej-- && (key) <= (t); (key)++, (val) = (void *)((byte *)(val) + (a)->element_size)) block\
    } else {\
      (key) += _ej;\
    }\
  }\
} while (0)

#define GRN_TINY_ARRAY_AT(a,id,e) do {\
  grn_id id_ = (id);\
  byte **e_;\
  size_t o_;\
  {\
    int l_, i_;\
    if (!id_) { e = NULL; break; }\
    GRN_BIT_SCAN_REV(id_, l_);\
    i_ = l_ >> GRN_TINY_ARRAY_W;\
    o_ = GRN_TINY_ARRAY_R(i_);\
    e_ = (byte **)&(a)->elements[i_];\
  }\
  if (!*e_) {\
    grn_ctx *ctx = (a)->ctx;\
    if ((a)->flags & GRN_TINY_ARRAY_THREADSAFE) { MUTEX_LOCK((a)->lock); }\
    if (!*e_) {\
      if ((a)->flags & GRN_TINY_ARRAY_USE_MALLOC) {\
        if ((a)->flags & GRN_TINY_ARRAY_CLEAR) {\
          *e_ = GRN_CALLOC(GRN_TINY_ARRAY_S * o_ * (a)->element_size);\
        } else {\
          *e_ = GRN_MALLOC(GRN_TINY_ARRAY_S * o_ * (a)->element_size);\
        }\
      } else {\
        *e_ = GRN_CTX_ALLOC(ctx, GRN_TINY_ARRAY_S * o_ * (a)->element_size, 0);\
      }\
    }\
    if ((a)->flags & GRN_TINY_ARRAY_THREADSAFE) { MUTEX_UNLOCK((a)->lock); }\
    if (!*e_) { e = NULL; break; }\
  }\
  if (id_ > (a)->max) { (a)->max = id_; }\
  (e) = (void *)(*e_ + (id_ - o_) * (a)->element_size);\
} while (0)

#define GRN_TINY_ARRAY_NEXT(a,e) GRN_TINY_ARRAY_AT((a), (a)->max + 1, e)

#define GRN_TINY_ARRAY_BIT_AT(array,offset,res) {\
  uint8_t *ptr_;\
  GRN_TINY_ARRAY_AT((array), ((offset) >> 3) + 1, ptr_);\
  res = ptr_ ? ((*ptr_ >> ((offset) & 7)) & 1) : 0;\
}

#define GRN_TINY_ARRAY_BIT_ON(array,offset) {\
  uint8_t *ptr_;\
  GRN_TINY_ARRAY_AT((array), ((offset) >> 3) + 1, ptr_);\
  if (ptr_) { *ptr_ |= (1 << ((offset) & 7)); }\
}

#define GRN_TINY_ARRAY_BIT_OFF(array,offset) {\
  uint8_t *ptr_;\
  GRN_TINY_ARRAY_AT((array), ((offset) >> 3) + 1, ptr_);\
  if (ptr_) { *ptr_ &= ~(1 << ((offset) & 7)); }\
}

#define GRN_TINY_ARRAY_BIT_FLIP(array,offset) {\
  uint8_t *ptr_;\
  GRN_TINY_ARRAY_AT((array), ((offset) >> 3) + 1, ptr_);\
  if (ptr_) { *ptr_ ^= (1 << ((offset) & 7)); }\
}

void grn_tiny_array_init(grn_tiny_array *a, grn_ctx *ctx, uint16_t element_size, uint16_t flags);
void grn_tiny_array_fin(grn_tiny_array *a);
void *grn_tiny_array_at(grn_tiny_array *a, grn_id id);
grn_id grn_tiny_array_id(grn_tiny_array *a, void *p);

/**** grn_array ****/

typedef struct _grn_array grn_array;

#define GRN_ARRAY_TINY (1L<<1)

typedef struct _grn_array_cursor grn_array_cursor;

struct _grn_array {
  grn_db_obj obj;
  grn_ctx *ctx;
  uint32_t value_size;
  uint32_t *n_garbages;
  uint32_t *n_entries;
  /* portions for io_array */
  grn_io *io;
  struct grn_array_header *header;
  uint32_t *lock;
  /* portions for tiny_array */
  uint32_t n_garbages_;
  uint32_t n_entries_;
  grn_id garbages;
  grn_tiny_array a;
  grn_tiny_array bitmap;
};

struct _grn_array_cursor {
  grn_db_obj obj;
  grn_id curr_rec;
  grn_array *array;
  grn_ctx *ctx;
  int dir;
  grn_id limit;
};

grn_array *grn_array_create(grn_ctx *ctx, const char *path,
                            uint32_t value_size, uint32_t flags);
grn_array *grn_array_open(grn_ctx *ctx, const char *path);
grn_rc grn_array_close(grn_ctx *ctx, grn_array *array);
grn_id grn_array_add(grn_ctx *ctx, grn_array *array, void **value);
int grn_array_get_value(grn_ctx *ctx, grn_array *array, grn_id id, void *valuebuf);
grn_rc grn_array_set_value(grn_ctx *ctx, grn_array *array, grn_id id,
                           void *value, int flags);
grn_array_cursor *grn_array_cursor_open(grn_ctx *ctx, grn_array *array,
                                        grn_id min, grn_id max, int flags);
grn_id grn_array_cursor_next(grn_ctx *ctx, grn_array_cursor *c);
int grn_array_cursor_get_value(grn_ctx *ctx, grn_array_cursor *c, void **value);
grn_rc grn_array_cursor_set_value(grn_ctx *ctx, grn_array_cursor *c,
                                  void *value, int flags);
grn_rc grn_array_cursor_delete(grn_ctx *ctx, grn_array_cursor *c,
                               grn_table_delete_optarg *optarg);
void grn_array_cursor_close(grn_ctx *ctx, grn_array_cursor *c);
grn_rc grn_array_delete_by_id(grn_ctx *ctx, grn_array *array, grn_id id,
                              grn_table_delete_optarg *optarg);

grn_id grn_array_next(grn_ctx *ctx, grn_array *array, grn_id id);

void *_grn_array_get_value(grn_ctx *ctx, grn_array *array, grn_id id);

#define GRN_ARRAY_EACH(array,head,tail,id,value,block) do {\
  grn_array_cursor *_sc = grn_array_cursor_open(ctx, array, head, tail, 0);\
  if (_sc) {\
    grn_id id;\
    while ((id = grn_array_cursor_next(ctx, _sc))) {\
      grn_array_cursor_get_value(ctx, _sc, (void **)(value)); \
      block\
    }\
    grn_array_cursor_close(ctx, _sc); \
  }\
} while (0)

#define GRN_ARRAY_SIZE(array) (*((array)->n_entries))

/**** grn_hash ****/

#define GRN_HASH_TINY (1L<<1)
#define GRN_HASH_MAX_KEY_SIZE GRN_TABLE_MAX_KEY_SIZE

struct _grn_hash {
  grn_db_obj obj;
  grn_ctx *ctx;
  uint32_t key_size;
  grn_encoding encoding;
  uint32_t value_size;
  uint32_t entry_size;
  uint32_t *n_garbages;
  uint32_t *n_entries;
  uint32_t *max_offset;
  grn_obj *tokenizer;
  /* portions for io_hash */
  grn_io *io;
  struct grn_hash_header *header;
  uint32_t *lock;
  // uint32_t nref;
  // unsigned int max_n_subrecs;
  // unsigned int record_size;
  // unsigned int subrec_size;
  // grn_rec_unit record_unit;
  // grn_rec_unit subrec_unit;
  // uint8_t arrayp;
  // grn_recordh *curr_rec;
  // grn_set_cursor *cursor;
  // int limit;
  // void *userdata;
  // grn_id subrec_id;
  /* portions for tiny_hash */
  uint32_t max_offset_;
  uint32_t n_garbages_;
  uint32_t n_entries_;
  grn_id *index;
  grn_id garbages;
  grn_tiny_array a;
  grn_tiny_array bitmap;
};

struct grn_hash_header {
  uint32_t flags;
  grn_encoding encoding;
  uint32_t key_size;
  uint32_t value_size;
  grn_id tokenizer;
  uint32_t curr_rec;
  int32_t curr_key;
  uint32_t idx_offset;
  uint32_t entry_size;
  uint32_t max_offset;
  uint32_t n_entries;
  uint32_t n_garbages;
  uint32_t lock;
  uint32_t reserved[16];
  grn_id garbages[GRN_HASH_MAX_KEY_SIZE];
};

struct _grn_hash_cursor {
  grn_db_obj obj;
  grn_id curr_rec;
  grn_hash *hash;
  grn_ctx *ctx;
  int dir;
  grn_id limit;
};

/* deprecated */

#define GRN_TABLE_SORT_BY_KEY      0
#define GRN_TABLE_SORT_BY_ID       (1L<<1)
#define GRN_TABLE_SORT_BY_VALUE    (1L<<2)
#define GRN_TABLE_SORT_RES_ID      0
#define GRN_TABLE_SORT_RES_KEY     (1L<<3)
#define GRN_TABLE_SORT_AS_BIN      0
#define GRN_TABLE_SORT_AS_NUMBER   (1L<<4)
#define GRN_TABLE_SORT_AS_SIGNED   0
#define GRN_TABLE_SORT_AS_UNSIGNED (1L<<5)
#define GRN_TABLE_SORT_AS_INT32    0
#define GRN_TABLE_SORT_AS_INT64    (1L<<6)
#define GRN_TABLE_SORT_NO_PROC     0
#define GRN_TABLE_SORT_WITH_PROC   (1L<<7)

typedef struct _grn_table_sort_optarg grn_table_sort_optarg;

struct _grn_table_sort_optarg {
  grn_table_sort_flags flags;
  int (*compar)(grn_ctx *ctx,
                grn_obj *table1, void *target1, unsigned int target1_size,
                grn_obj *table2, void *target2, unsigned int target2_size,
                void *compare_arg);
  void *compar_arg;
  grn_obj *proc;
  int offset;
};

int grn_hash_sort(grn_ctx *ctx, grn_hash *hash, int limit,
                  grn_array *result, grn_table_sort_optarg *optarg);

grn_id grn_hash_at(grn_ctx *ctx, grn_hash *hash, const void *key, int key_size,
                   void **value);
grn_id grn_hash_get(grn_ctx *ctx, grn_hash *hash, const void *key, int key_size,
                    void **value, grn_search_flags *flags);

grn_rc grn_hash_lock(grn_ctx *ctx, grn_hash *hash, int timeout);
grn_rc grn_hash_unlock(grn_ctx *ctx, grn_hash *hash);
grn_rc grn_hash_clear_lock(grn_ctx *ctx, grn_hash *hash);

#define GRN_HASH_SIZE(hash) (*((hash)->n_entries))

/* private */
typedef enum {
  grn_rec_document = 0,
  grn_rec_section,
  grn_rec_position,
  grn_rec_userdef,
  grn_rec_none
} grn_rec_unit;

int grn_rec_unit_size(grn_rec_unit unit, int rec_size);

const char * _grn_hash_key(grn_ctx *ctx, grn_hash *hash, grn_id id, uint32_t *key_size);

int grn_hash_get_key_value(grn_ctx *ctx, grn_hash *hash, grn_id id,
                           void *keybuf, int bufsize, void *valuebuf);

void grn_rhash_add_subrec(grn_hash *s, grn_rset_recinfo *ri,
                          int score, void *body, int dir);

int _grn_hash_get_key_value(grn_ctx *ctx, grn_hash *hash, grn_id id,
                            void **key, void **value);

grn_id grn_hash_next(grn_ctx *ctx, grn_hash *hash, grn_id id);

/* only valid for hash tables, GRN_OBJ_KEY_VAR_SIZE && GRN_HASH_TINY */
const char *_grn_hash_strkey_by_val(void *v, uint16_t *size);

const char *grn_hash_get_value_(grn_ctx *ctx, grn_hash *hash, grn_id id, uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif /* GRN_HASH_H */
