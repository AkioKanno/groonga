#!/bin/sh

export BASE_DIR="`dirname $0`"
if test -z "$BUILD_DIR"; then
    BUILD_DIR="$BASE_DIR"
fi
export BUILD_DIR

top_dir="$BUILD_DIR/../.."
top_dir=$(cd -P "$top_dir" 2>/dev/null || cd "$top_dir"; pwd)

if test x"$NO_MAKE" != x"yes"; then
    make -C $top_dir > /dev/null || exit 1
fi

if test -z "$RUBY"; then
    RUBY="`make -s -C $top_dir echo-ruby`"
fi
export RUBY

if test -z "$GROONGA"; then
    GROONGA="`make -s -C $top_dir echo-groonga`"
fi
export GROONGA

if test -z "$GROONGA_SUGGEST_CREATE_DATASET"; then
    GROONGA_SUGGEST_CREATE_DATASET="`make -s -C $top_dir echo-groonga-suggest-create-dataset`"
fi
export GROONGA_SUGGEST_CREATE_DATASET

GRN_PLUGINS_DIR="$top_dir/plugins"
export GRN_PLUGINS_DIR

case `uname` in
    Darwin)
	DYLD_LIBRARY_PATH="$top_dir/lib/.libs:$DYLD_LIBRARY_PATH"
	export DYLD_LIBRARY_PATH
	;;
    *)
	:
	;;
esac

if test -z "$RUBY"; then
    exit 1
fi

groonga_test_dir="$BASE_DIR/groonga-test"
if ! test -d "$groonga_test_dir"; then
    git clone git://github.com/groonga/groonga-test.git "$groonga_test_dir"
fi

$RUBY -I "$groonga_test_dir/lib" \
    "$groonga_test_dir/bin/groonga-test" \
    --groonga "$GROONGA" \
    --groonga-suggest-create-dataset "$GROONGA_SUGGEST_CREATE_DATASET" \
    --base-directory "$BASE_DIR" \
    "$BASE_DIR/suite" "$@"
