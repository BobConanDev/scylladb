/*
 * Copyright 2015 Cloudius Systems
 */

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE core

#include "bytes_ostream.hh"
#include <boost/test/unit_test.hpp>

void append_sequence(bytes_ostream& buf, int count) {
    for (int i = 0; i < count; i++) {
        buf.write(i);
    }
}

void assert_sequence(bytes_ostream& buf, int count) {
    bytes_view v = buf.linearize();
    assert(buf.size() == count * sizeof(int));
    for (int i = 0; i < count; i++) {
        auto val = read_simple<int>(v);
        BOOST_REQUIRE_EQUAL(val, i);
    }
}

BOOST_AUTO_TEST_CASE(test_appended_data_is_retained) {
    bytes_ostream buf;
    append_sequence(buf, 1024);
    assert_sequence(buf, 1024);
}

BOOST_AUTO_TEST_CASE(test_copy_constructor) {
    bytes_ostream buf;
    append_sequence(buf, 1024);

    bytes_ostream buf2(buf);

    BOOST_REQUIRE(buf.size() == 1024 * sizeof(int));
    BOOST_REQUIRE(buf2.size() == 1024 * sizeof(int));
    BOOST_REQUIRE(buf2.is_linearized());

    assert_sequence(buf, 1024);
    assert_sequence(buf2, 1024);
}

BOOST_AUTO_TEST_CASE(test_copy_assignment) {
    bytes_ostream buf;
    append_sequence(buf, 512);

    bytes_ostream buf2;
    append_sequence(buf2, 1024);

    buf2 = buf;

    BOOST_REQUIRE(buf.size() == 512 * sizeof(int));
    BOOST_REQUIRE(buf2.size() == 512 * sizeof(int));
    BOOST_REQUIRE(buf2.is_linearized());

    assert_sequence(buf, 512);
    assert_sequence(buf2, 512);
}

BOOST_AUTO_TEST_CASE(test_move_assignment) {
    bytes_ostream buf;
    append_sequence(buf, 512);

    bytes_ostream buf2;
    append_sequence(buf2, 1024);

    buf2 = std::move(buf);

    BOOST_REQUIRE(buf.size() == 0);
    BOOST_REQUIRE(buf2.size() == 512 * sizeof(int));

    assert_sequence(buf2, 512);
}

BOOST_AUTO_TEST_CASE(test_move_constructor) {
    bytes_ostream buf;
    append_sequence(buf, 1024);

    bytes_ostream buf2(std::move(buf));

    BOOST_REQUIRE(buf.size() == 0);
    BOOST_REQUIRE(buf2.size() == 1024 * sizeof(int));

    assert_sequence(buf2, 1024);
}

BOOST_AUTO_TEST_CASE(test_size) {
    bytes_ostream buf;
    append_sequence(buf, 1024);
    BOOST_REQUIRE_EQUAL(buf.size(), sizeof(int) * 1024);
}

BOOST_AUTO_TEST_CASE(test_is_linearized) {
    bytes_ostream buf;

    BOOST_REQUIRE(buf.is_linearized());

    buf.write(1);

    BOOST_REQUIRE(buf.is_linearized());

    append_sequence(buf, 1024);

    BOOST_REQUIRE(!buf.is_linearized()); // probably
}

BOOST_AUTO_TEST_CASE(test_view) {
    bytes_ostream buf;

    buf.write(1);

    BOOST_REQUIRE(buf.is_linearized());

    auto view = buf.view();
    BOOST_REQUIRE_EQUAL(1, read_simple<int>(view));
}

BOOST_AUTO_TEST_CASE(test_writing_blobs) {
    bytes_ostream buf;

    bytes b("hello");
    bytes_view b_view(b.begin(), b.size());

    buf.write(b_view);
    BOOST_REQUIRE(buf.linearize() == b_view);
}

BOOST_AUTO_TEST_CASE(test_writing_large_blobs) {
    bytes_ostream buf;

    bytes b(bytes::initialized_later(), 1024);
    std::fill(b.begin(), b.end(), 7);
    bytes_view b_view(b.begin(), b.size());

    buf.write(b_view);

    auto buf_view = buf.linearize();
    BOOST_REQUIRE(std::all_of(buf_view.begin(), buf_view.end(), [] (auto&& c) { return c == 7; }));
}

BOOST_AUTO_TEST_CASE(test_fragment_iteration) {
    int count = 64*1024;

    bytes_ostream buf;
    append_sequence(buf, count);

    bytes_ostream buf2;
    for (bytes_view frag : buf.fragments()) {
        buf2.write(frag);
    }

    // If this fails, we will only have one fragment, and the test will be weak.
    // Bump up the 'count' if this is triggered.
    assert(!buf2.is_linearized());

    assert_sequence(buf2, count);
}

BOOST_AUTO_TEST_CASE(test_writing_empty_blobs) {
    bytes_ostream buf;

    bytes b;
    buf.write(b);

    BOOST_REQUIRE(buf.size() == 0);
    BOOST_REQUIRE(buf.linearize().empty());
}

BOOST_AUTO_TEST_CASE(test_retraction_to_initial_state) {
    bytes_ostream buf;

    auto pos = buf.pos();
    buf.write(1);

    buf.retract(pos);

    BOOST_REQUIRE(buf.size() == 0);
    BOOST_REQUIRE(buf.linearize().empty());
}

BOOST_AUTO_TEST_CASE(test_retraction_to_the_same_chunk) {
    bytes_ostream buf;

    buf.write(1);
    buf.write(2);
    auto pos = buf.pos();
    buf.write(3);
    buf.write(4);

    buf.retract(pos);

    BOOST_REQUIRE(buf.size() == sizeof(int) * 2);

    bytes_view v(buf.linearize());
    BOOST_REQUIRE_EQUAL(read_simple<int>(v), 1);
    BOOST_REQUIRE_EQUAL(read_simple<int>(v), 2);
    BOOST_REQUIRE(v.empty());
}

BOOST_AUTO_TEST_CASE(test_no_op_retraction) {
    bytes_ostream buf;

    buf.write(1);
    buf.write(2);
    auto pos = buf.pos();

    buf.retract(pos);

    BOOST_REQUIRE(buf.size() == sizeof(int) * 2);

    bytes_view v(buf.linearize());
    BOOST_REQUIRE_EQUAL(read_simple<int>(v), 1);
    BOOST_REQUIRE_EQUAL(read_simple<int>(v), 2);
    BOOST_REQUIRE(v.empty());
}

BOOST_AUTO_TEST_CASE(test_retraction_discarding_chunks) {
    bytes_ostream buf;

    buf.write(1);
    auto pos = buf.pos();
    append_sequence(buf, 64*1024);

    buf.retract(pos);

    BOOST_REQUIRE(buf.size() == sizeof(int));
    bytes_view v(buf.linearize());
    BOOST_REQUIRE_EQUAL(read_simple<int>(v), 1);
    BOOST_REQUIRE(v.empty());
}

BOOST_AUTO_TEST_CASE(test_writing_placeholders) {
    bytes_ostream buf;

    auto ph = buf.write_place_holder<int>();
    buf.write<int>(2);
    buf.set(ph, 1);

    auto buf_view = buf.linearize();
    BOOST_REQUIRE(read_simple<int>(buf_view) == 1);
    BOOST_REQUIRE(read_simple<int>(buf_view) == 2);
    BOOST_REQUIRE(buf_view.empty());
}
