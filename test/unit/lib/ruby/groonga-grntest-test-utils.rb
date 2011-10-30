# -*- coding: utf-8 -*-
#
# Copyright (C) 2010-2011  Kouhei Sutou <kou@clear-code.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

require "tempfile"
require 'groonga-test-utils'

module GroongaGrntestTestUtils
  include GroongaTestUtils

  def setup_database
    setup_database_path
    input = ""
    ["ddl.grn", "areas.grn", "categories.grn", "shops.grn"].each do |grn|
      input << File.read(taiyaki_story_fixture(grn))
    end
    output, error, status = invoke_groonga("-n", @database_path, :input => input)
    assert_predicate(status, :success?, [output, error])
  end

  def teardown_database
    teardown_database_path
  end

  private
  def guess_grntest_path
    grntest = ENV["GRNTEST"]
    grntest ||= File.join(guess_top_source_dir, "src", "grntest")
    File.expand_path(grntest)
  end

  def grntest
    @grntest ||= guess_grntest_path
  end

  def invoke_grntest(*args)
    args.unshift(grntest)
    invoke_command(*args)
  end

  def taiyaki_story_fixture(file)
    File.join(File.dirname(__FILE__),
              "..",
              "..",
              "fixtures",
              "story",
              "taiyaki",
              file)
  end

  def tempfile(name)
    file = Tempfile.new(name, @tmp_base_dir)
    if block_given?
      yield(file)
      file.close
    end
    file
  end
end
