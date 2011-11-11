/* -*- c-basic-offset: 2; coding: utf-8 -*- */
/*
  Copyright (C) 2011  Brazil

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
#include <cppcutter.h>

#include <grn-assertions.h>
#include <dat/key-cursor.hpp>
#include <dat/trie.hpp>

#include <cstring>

namespace
{
  void create_trie(grn::dat::Trie *trie)
  {
    trie->create();
    trie->insert("Werdna", std::strlen("Werdna"));  // ID: 1, 7th
    trie->insert("Trebor", std::strlen("Trebor"));  // ID: 2, 6th
    trie->insert("Human", std::strlen("Human"));    // ID: 3, 5th
    trie->insert("Elf", std::strlen("Elf"));        // ID: 4, 2nd
    trie->insert("Dwarf", std::strlen("Dward"));    // ID: 5, 1st
    trie->insert("Gnome", std::strlen("Gnome"));    // ID: 6, 3rd
    trie->insert("Hobbit", std::strlen("Hobbit"));  // ID: 7, 4th
  }
}

namespace test_dat_key_cursor
{
  void test_null(void)
  {
    grn::dat::Trie trie;
    create_trie(&trie);

    grn::dat::KeyCursor cursor;
    cursor.open(trie, grn::dat::String(), grn::dat::String());
    cppcut_assert_equal(grn::dat::UInt32(5), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(1), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());
  }

  void test_min(void)
  {
    grn::dat::Trie trie;
    create_trie(&trie);

    grn::dat::KeyCursor cursor;

    cursor.open(trie, grn::dat::String("Hobbit"), grn::dat::String());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(1), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("T"), grn::dat::String());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(1), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Z"), grn::dat::String());
    cppcut_assert_equal(false, cursor.next().is_valid());
  }

  void test_max_by_str(void)
  {
    grn::dat::Trie trie;
    create_trie(&trie);

    grn::dat::KeyCursor cursor;

    cursor.open(trie, grn::dat::String(), grn::dat::String("Elf"));
    cppcut_assert_equal(grn::dat::UInt32(5), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String(), grn::dat::String("F"));
    cppcut_assert_equal(grn::dat::UInt32(5), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String(), grn::dat::String("A"));
    cppcut_assert_equal(false, cursor.next().is_valid());
  }

  void test_min_max(void)
  {
    grn::dat::Trie trie;
    create_trie(&trie);

    grn::dat::KeyCursor cursor;
    cursor.open(trie, grn::dat::String("Gnome"), grn::dat::String("Trebor"));
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());
  }

  void test_offset(void)
  {
    grn::dat::Trie trie;
    create_trie(&trie);

    grn::dat::KeyCursor cursor;

    cursor.open(trie, grn::dat::String("Hobbit"), grn::dat::String("Trebor"), 0);
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String(), grn::dat::String(), 5);
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(1), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Gnome"), grn::dat::String("Trebor"), 2);
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Gnome"), grn::dat::String("Trebor"), 100);
    cppcut_assert_equal(false, cursor.next().is_valid());
  }

  void test_limit(void)
  {
    grn::dat::Trie trie;
    create_trie(&trie);

    grn::dat::KeyCursor cursor;

    cursor.open(trie, grn::dat::String("Gnome"), grn::dat::String("Werdna"),
                0, grn::dat::UINT32_MAX);
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(1), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Gnome"), grn::dat::String("Werdna"),
                0, 3);
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("A"), grn::dat::String("Z"), 3, 2);
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("A"), grn::dat::String("Z"), 0, 0);
    cppcut_assert_equal(false, cursor.next().is_valid());
  }

  void test_ascending_cursor(void)
  {
    grn::dat::Trie trie;
    create_trie(&trie);

    grn::dat::KeyCursor cursor;

    cursor.open(trie, grn::dat::String(), grn::dat::String(),
                0, grn::dat::UINT32_MAX, grn::dat::ASCENDING_CURSOR);
    cppcut_assert_equal(grn::dat::UInt32(5), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(1), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Elf"), grn::dat::String("Human"),
                0, grn::dat::UINT32_MAX, grn::dat::ASCENDING_CURSOR);
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Dwarf"), grn::dat::String("Trebor"),
                3, 2, grn::dat::ASCENDING_CURSOR);
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());
  }

  void test_descending_cursor(void)
  {
    grn::dat::Trie trie;
    create_trie(&trie);

    grn::dat::KeyCursor cursor;

    cursor.open(trie, grn::dat::String(), grn::dat::String(),
                0, grn::dat::UINT32_MAX, grn::dat::DESCENDING_CURSOR);
    cppcut_assert_equal(grn::dat::UInt32(1), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(5), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Elf"), grn::dat::String("Human"),
                0, grn::dat::UINT32_MAX, grn::dat::DESCENDING_CURSOR);
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Dwarf"), grn::dat::String("Trebor"),
                3, 2, grn::dat::DESCENDING_CURSOR);
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());
  }

  void test_except_boundary(void)
  {
    grn::dat::Trie trie;
    create_trie(&trie);

    grn::dat::KeyCursor cursor;

    cursor.open(trie, grn::dat::String(), grn::dat::String(),
                0, grn::dat::UINT32_MAX,
                grn::dat::EXCEPT_LOWER_BOUND | grn::dat::EXCEPT_UPPER_BOUND);
    cppcut_assert_equal(grn::dat::UInt32(5), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(1), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Dwarf"), grn::dat::String("Werdna"),
                0, grn::dat::UINT32_MAX,
                grn::dat::EXCEPT_LOWER_BOUND | grn::dat::EXCEPT_UPPER_BOUND);
    cppcut_assert_equal(grn::dat::UInt32(4), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Elf"), grn::dat::String("Trebor"),
                2, 100, grn::dat::EXCEPT_LOWER_BOUND);
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(2), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Elf"), grn::dat::String("Trebor"),
                2, 100, grn::dat::EXCEPT_UPPER_BOUND);
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());

    cursor.open(trie, grn::dat::String("Fighter"), grn::dat::String("Samurai"),
                0, grn::dat::UINT32_MAX,
                grn::dat::EXCEPT_LOWER_BOUND | grn::dat::EXCEPT_UPPER_BOUND);
    cppcut_assert_equal(grn::dat::UInt32(6), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(7), cursor.next().id());
    cppcut_assert_equal(grn::dat::UInt32(3), cursor.next().id());
    cppcut_assert_equal(false, cursor.next().is_valid());
  }
}
