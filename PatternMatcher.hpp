#ifndef PATTERNMATCHER_HPP
#define PATTERNMATCHER_HPP

#include <unordered_map>
#include <unordered_set>
#include <string.h>
#include <vector>

#include "BufferManager.hpp"
#include "AhoCorasickAutomaton.hpp"

typedef uint16_t pattern_length_t;


template <typename KeyType>
class PatternMatcher {
private:
    typedef uint32_t word_identifier_t;

private:
    AhoCorasickAutomaton<KeyType, word_identifier_t> automaton;
    BufferManager buffer_manager;
    std::unordered_set<MyString> pattern_set;
    std::unordered_map<KeyType, pattern_length_t> pattern_id_to_length;
    std::unordered_map<MyString, word_identifier_t> word_to_word_id;

public:
    PatternMatcher() {
    }

    void
    add_pattern(
            KeyType pattern_id,
            const std::string &pattern
    ) {
        word_identifier_t word_ids[32];

        // check if the same pattern has been already inserted
        if (pattern_set.count(MyString(pattern.c_str(), pattern.size()))) {
            throw std::runtime_error("This pattern has been already inserted");
        }
        // create the DataBlock using the BufferManager
        MyString pattern_block = this->buffer_manager.createDataBlock(&pattern[0], pattern.size());

        pattern_length_t num_words = 0;
        for (size_t marker_pos = 0, space_pos = 0, max_space_pos = pattern_block.size();
             marker_pos < max_space_pos;
             marker_pos = space_pos + 1
                ) {
            space_pos = pattern_block.find(' ', marker_pos);

            // skip empty sub portions
            if (marker_pos >= space_pos) {
                continue;
            }
            // store this word into the map
            MyString word = pattern_block.sub(marker_pos, space_pos - marker_pos);

            auto find_word_it = this->word_to_word_id.find(word);
            if (find_word_it == this->word_to_word_id.end()) {
                word_ids[num_words] = this->word_to_word_id[word] = (word_identifier_t) this->word_to_word_id.size() + 1;
            } else {
                word_ids[num_words] = find_word_it->second;
            }
            // increase the number of words
            ++num_words;
        }

        this->pattern_set.insert(pattern_block);
        this->automaton.add_pattern(pattern_id, &word_ids[0], &word_ids[num_words]);
        this->pattern_id_to_length[pattern_id] = num_words;
    }

    void
    compile() {
        this->automaton.compile();
        this->automaton.reduce_memory_footprint();
    }

    void
    complete_with_suffix_matches(
            PatternMatches<KeyType> &src_matches,
            PatternMatches<KeyType> &dst_matches
    ) const {
        this->automaton.complete_with_suffix_matches(src_matches, dst_matches);
    }

    void
    find_patterns(
            const std::string &text,
            PatternMatches<KeyType> &matches
    ) const {
        type_state_id current_state_id = 0;
        size_t pos = 0;
        word_identifier_t current_word_id = 0;
        MyString text_block = MyString(&text[0], text.size());
        auto find_result_it_end = this->word_to_word_id.cend();

        for (size_t marker_pos = 0, space_pos = 0, max_space_pos = text_block.size();
             marker_pos < max_space_pos;
             marker_pos = space_pos + 1
                ) {
            space_pos = text_block.find(' ', marker_pos);

            // skip empty sub portions
            if (marker_pos >= space_pos) {
                continue;
            }
            // recognize the current word
            MyString word = text_block.sub(marker_pos, space_pos - marker_pos);
            auto find_word_it = this->word_to_word_id.find(word);
            if (find_word_it != find_result_it_end) {
                current_word_id = find_word_it->second;
            } else {
                current_word_id = 0;
            }

            // go to the next state
            current_state_id = this->automaton.get_next_state_id(current_state_id, current_word_id, matches, pos);

            // advance the counter
            ++pos;
        }
    }

    pattern_length_t
    get_pattern_length(
            KeyType pattern_id
    ) const {
        auto it = this->pattern_id_to_length.find(pattern_id);
        if (it == this->pattern_id_to_length.end())
            throw std::runtime_error("The given pattern has not been found");
        return it->second;
    }

    const std::unordered_map<KeyType, pattern_length_t> &
    get_pattern_length_map() const {
        return this->pattern_id_to_length;
    }

    const std::unordered_set<MyString> &
    get_pattern_set() const {
        return this->pattern_set;
    }

    void
    reserve(
            size_t num_patterns
    ) {
        this->automaton.reserve(num_patterns);
        this->pattern_set.reserve(num_patterns);
        this->word_to_word_id.reserve(num_patterns);
    }
};

#endif //PATTERNMATCHER_HPP
