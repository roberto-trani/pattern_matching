#include <assert.h>
#include "PatternMatcher.hpp"


void
test1() {
    const char *testString = "hello world string";
    const char *patterns[3] = {
            "hello",
            "world",
            "hello world",
    };

    // matcher initialization
    PatternMatcher<uint8_t> matcher;
    for (uint8_t i = 0; i < 3; ++i) {
        matcher.add_pattern(i, patterns[i]);
    }

    // compile
    matcher.compile();

    // test if add_pattern after compile operation throws an exception
    try {
        matcher.add_pattern(15, "wow");
        throw std::exception();  // "Exception not thrown"
    } catch (std::runtime_error) {}

    // two kind of matches are created
    PatternMatches<uint8_t> matches1(false);
    PatternMatches<uint8_t> matches2(true);

    // test find patterns without suffixes
    matcher.find_patterns(testString, matches1);
    assert(matches1.size() == 2);
    assert(matches1[0].pattern == 0 && matches1[0].end_pos == 0);
    assert(matches1[1].pattern == 2 && matches1[1].end_pos == 1);

    // test find patterns with suffixes
    matcher.find_patterns(testString, matches2);
    assert(matches2.size() == 3);
    assert(matches2[0].pattern == 0 && matches2[0].end_pos == 0);
    assert(matches2[1].pattern == 2 && matches2[1].end_pos == 1);
    assert(matches2[2].pattern == 1 && matches2[2].end_pos == 1);

    // test complete_with_suffix_matches
    PatternMatches<uint8_t> matches3(true);
    matcher.complete_with_suffix_matches(matches1, matches3);
    assert(matches2.size() == matches3.size());
    for (int i = 0; i < matches2.size(); ++i) {
        assert(matches2[i] == matches3[i]);
    }

    // test clear
    matches3.clear();
    assert(matches3.size() == 0);
}


void
test2() {
    // test the matches order
    const size_t num_chars = 20;
    const size_t seq_n_repetitions = 3 * 3;
    const size_t num_restrictions = 20;

    static_assert(num_chars > 0, "num_chars must be greater than 0");
    static_assert(seq_n_repetitions > 0, "seq_n_repetitions must be greater than 0");
    static_assert(seq_n_repetitions % 3 == 0, "seq_n_repetitions must be a multiple of 3");
    static_assert(num_restrictions > 0, "num_restrictions must be greater than 0");

    // create a list of matches and the map pattern to length
    char testString[seq_n_repetitions * num_chars * 2];
    for (size_t n = 0, i = 0; n < seq_n_repetitions; ++n) {
        for (size_t ci = 0; ci < num_chars; ++ci, i += 2) {
            testString[i] = 'a' + (char) ci;
            testString[i + 1] = ' ';
        }
    }
    PatternMatcher<uint16_t> matcher;
    size_t expected_num_matches1 = 0;
    size_t expected_num_matches2 = seq_n_repetitions * num_chars;  // at most one match in each position
    for (uint16_t ci = 0, id = 0; ci < num_chars; ++ci) {
        char str[8];
        for (size_t j = 0; j < 7; j += 2) {
            if (j > 0) {
                str[j - 1] = ' ';
            }
            str[j] = (char) ('a' + ((ci + j / 2) % num_chars));
            str[j + 1] = '\0';
            matcher.add_pattern(id++, std::string(str));

            if ((ci + j / 2) >= num_chars) {
                expected_num_matches1 += seq_n_repetitions - 1;
            } else {
                expected_num_matches1 += seq_n_repetitions;
            }
        }
    }
    matcher.compile();

    // two kind of matches are created
    PatternMatches<uint16_t> matches1(true);
    PatternMatches<uint16_t> matches2(false);
    PatternMatches<uint16_t> matches3(true);

    matcher.find_patterns(testString, matches1);
    matcher.find_patterns(testString, matches2);

    assert(matches1.size() == expected_num_matches1);
    assert(matches2.size() == expected_num_matches2);

    // check the order of matches1(true)
    for (int i = 1; i < matches1.size(); ++i) {
        // check that the matches are ordered by end_pos
        assert(matches1[i - 1].end_pos <= matches1[i].end_pos);
        // check if when end_pos is the same they are ordered by pattern length
        assert(matches1[i - 1].end_pos < matches1[i].end_pos ||
               matcher.get_pattern_length(matches1[i - 1].pattern) > matcher.get_pattern_length(matches1[i].pattern));
    }
    // check the order of matches2(true)
    for (int i = 2; i < matches2.size(); ++i) {
        assert(matches2[i - 1].end_pos < matches2[i].end_pos);
    }

    // check the method complete_with_suffix_matches
    matcher.complete_with_suffix_matches(matches2, matches3);
    assert(matches1.size() == matches3.size());
    for (int i = 0; i < matches1.size(); ++i) {
        // check that the two are the same after the complete_with_suffix_matches
        assert(matches1[i].end_pos == matches3[i].end_pos);
        assert(matches1[i].pattern == matches3[i].pattern);
    }
}


int main(int argc, char **argv) {
    test1();
    test2();

    return 0;
}
