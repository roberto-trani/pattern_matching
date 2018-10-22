#ifndef AHOCORASICKAUTOAMATON_HPP
#define AHOCORASICKAUTOAMATON_HPP

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>

typedef uint32_t type_state_id;


/**
 *
 * @tparam KeyType
 */
template<typename KeyType>
class PatternMatch {
public:
    const KeyType pattern;
    size_t end_pos;

public:
    PatternMatch(const KeyType &pattern, size_t end_pos) :
            pattern(pattern),
            end_pos(end_pos) {}

    bool operator==(const PatternMatch<KeyType> other) {
        return this->pattern == other.pattern && this->end_pos == other.end_pos;
    }
};


/**
 *
 * @tparam KeyType
 */
template<typename KeyType>
class PatternMatches : public std::vector<PatternMatch<KeyType>> {
private:
    bool b_include_suffixes;

public:
    PatternMatches(bool include_suffixes = true) :
            b_include_suffixes(include_suffixes) {}

    bool include_suffixes() const {
        return this->b_include_suffixes;
    }
};


template<typename KeyType, typename SequenceType>
class AhoCorasickAutomaton {
private:
    typedef uint32_t type_pattern_id;
    typedef uint32_t type_goto_id;
    typedef std::unordered_map<SequenceType, type_state_id> GotoTableType;
    typedef typename GotoTableType::const_iterator GotoTableIteratorType;

    const type_goto_id NO_GOTO_ID = (type_goto_id) -1;
    const type_pattern_id NO_PATTERN_ID = (type_pattern_id) -1;

    /**
     *
     */
    class AhoCorasickNode {
    public:
        type_goto_id l_goto_id;
        type_pattern_id l_pattern_id;

    public:
        AhoCorasickNode(type_goto_id goto_id, type_pattern_id pattern_id) :
                l_goto_id(goto_id),
                l_pattern_id(pattern_id) {}
    };

    /**
     *
     */
    class BFSQueueEntry {
    public:
        type_state_id fail_state_id;
        type_state_id curr_state_id;

    public:
        BFSQueueEntry() :
                fail_state_id(0),
                curr_state_id(0) {}

        BFSQueueEntry(type_state_id fail_state_id, type_state_id curr_state_id) :
                fail_state_id(fail_state_id),
                curr_state_id(curr_state_id) {}
    };


private:
    bool b_is_compiled;
    std::vector<AhoCorasickAutomaton::AhoCorasickNode> v_state_id_to_node;
    std::vector<AhoCorasickAutomaton::GotoTableType> v_goto_id_to_goto;
    std::vector<KeyType> v_pattern_id_to_pattern_key;
    std::vector<type_pattern_id> v_pattern_id_to_longest_suffix_pattern_id;
    std::unordered_map<KeyType, type_pattern_id> h_pattern_key_to_pattern_id;

public:
    /**
     * Create a new Aho-Corasick Trie, that will become an automaton after the compilation.
     */
    AhoCorasickAutomaton() :
            b_is_compiled(false),
            v_state_id_to_node({AhoCorasickNode(0, AhoCorasickAutomaton::NO_PATTERN_ID)}),
            v_goto_id_to_goto(1),
            v_pattern_id_to_pattern_key(0),
            v_pattern_id_to_longest_suffix_pattern_id(0),
            h_pattern_key_to_pattern_id(0) {}

    /**
     * Add a new pattern into the trie.
     * @param key A value to associate to this pattern, that will be retrieved during the parsing
     * @param pattern_begin A pointer to the begin of the pattern
     * @param pattern_end A pointer to the end of the pattern
     */
    void
    add_pattern(
            const KeyType &key,
            const SequenceType *pattern_begin,
            const SequenceType *pattern_end
    ) {
        if (this->b_is_compiled) {
            throw std::runtime_error("This method cannot be called after the Automaton has been compiled");
        }
        this->_add_pattern(key, pattern_begin, pattern_end);
    }

    /**
     * Compile this Aho-Corasick Trie into an automaton to perform an efficient parsing.
     */
    void
    compile() {
        // remember: goto 0 is the default behaviour
        if (this->b_is_compiled) {
            return;
        }
        this->_compile();
        this->b_is_compiled = true;
    }

    /**
     * Put into the second vector the matches of the first vector plus all the suffixes of these. This operation
     * preserves the matching order.
     * @param src_matches The source vector with the matches (without suffixes)
     * @param dst_matches The destination vector where to put the matches (with suffixes)
     */
    void
    complete_with_suffix_matches(
            PatternMatches<KeyType> &src_matches,
            PatternMatches<KeyType> &dst_matches
    ) const {
        if (src_matches.include_suffixes()) {
            throw std::runtime_error("The _first argument must not include the suffixes");
        }
        if (!dst_matches.include_suffixes()) {
            throw std::runtime_error("The _second argument must include the suffixes");
        }

        // fill the matches vector
        const size_t dst_initial_size = dst_matches.size();
        for (size_t i = 0, i_max = src_matches.size(); i < i_max; ++i) {
            // look for the pattern in the dictionary
            auto it = this->h_pattern_key_to_pattern_id.find(((PatternMatch<KeyType>) src_matches[i]).pattern);
            if (it == this->h_pattern_key_to_pattern_id.end()) {
                // the pattern is not in, hence I remove the inserted matches and then throws an exception
                for (size_t j = 0, j_max = dst_matches.size() - dst_initial_size; j < j_max; ++j) {
                    dst_matches.pop_back();
                }
                throw std::runtime_error("One of the patterns inside the source matches have not been found");
            }
            type_pattern_id current_pattern_id = it->second;
            const size_t end_pos = ((PatternMatch<KeyType>) src_matches[i]).end_pos;

            // insert the starting match
            dst_matches.push_back(src_matches[i]);
            // insert all its suffixes
            while (true) {
                current_pattern_id = this->v_pattern_id_to_longest_suffix_pattern_id[current_pattern_id];
                if (current_pattern_id == AhoCorasickAutomaton::NO_PATTERN_ID)
                    break;
                dst_matches.push_back(
                        PatternMatch<KeyType>(this->v_pattern_id_to_pattern_key[current_pattern_id], end_pos));
            }
        }
    }

    /**
     * Get the next state identifier starting from the current state id and a sequence element.+
     * If the automaton has not been compiled the result of this operation can be the wrong one!
     * @param current_state_id The current state identifier (the initial state is 0)
     * @param sequence_element The current sequence element
     * @return The identifier of the next state
     */
    type_state_id
    get_next_state_id(
            type_state_id current_state_id,
            const SequenceType &sequence_element
    ) const {
        return this->_get_next_state_id(current_state_id, sequence_element);
    }

    /**
     * Get the next state identifier starting from the current state id and a sequence element. If the last argument is
     * different from nullptr all the patterns discovered with this operation will be pushed into the vector.
     * If the automaton has not been compiled the result of this operation can be the wrong one!
     * @param current_state_id The current state identifier (the initial state is 0)
     * @param sequence_element The current sequence element
     * @param patterns_accumulator A vector to use as pattern accumulator
     * @return The identifier of the next state
     */
    type_state_id
    get_next_state_id(
            type_state_id current_state_id,
            const SequenceType &sequence_element,
            std::vector<KeyType> &patterns_accumulator
    ) const {
        type_state_id next_state_id = this->_get_next_state_id(current_state_id, sequence_element);

        type_pattern_id current_pattern_id = this->v_state_id_to_node[next_state_id].l_output_pattern_id;

        // fill the patterns_accumulator
        if (current_pattern_id != AhoCorasickAutomaton::NO_PATTERN_ID) {
            patterns_accumulator.push_back(this->v_pattern_id_to_pattern_key[current_pattern_id]);

            while (true) {
                current_pattern_id = this->v_pattern_id_to_longest_suffix_pattern_id[current_pattern_id];
                if (current_pattern_id == AhoCorasickAutomaton::NO_PATTERN_ID)
                    break;
                patterns_accumulator.push_back(this->v_pattern_id_to_pattern_key[current_pattern_id]);
            }
        }

        // return the next state identifier
        return next_state_id;
    }

    /**
     * Get the next state identifier starting from the current state id and a sequence element. All the patterns
     * discovered with this operation will be pushed into the matches' vector.
     * @param current_state_id The current state identifier (the initial state is 0)
     * @param sequence_element The current sequence element
     * @param matches A vector to use as matches accumulator
     * @param pos Position to associate to this match
     * @return The identifier of the next state
     */
    type_state_id
    get_next_state_id(
            type_state_id current_state_id,
            const SequenceType &sequence_element,
            PatternMatches<KeyType> &matches,
            const size_t pos = 0
    ) const {
        type_state_id next_state_id = this->_get_next_state_id(current_state_id, sequence_element);

        type_pattern_id current_pattern_id = ((AhoCorasickNode) this->v_state_id_to_node[next_state_id]).l_pattern_id;

        // fill the matches vector
        if (current_pattern_id != AhoCorasickAutomaton::NO_PATTERN_ID) {
            matches.push_back(PatternMatch<KeyType>(this->v_pattern_id_to_pattern_key[current_pattern_id], pos));

            if (matches.include_suffixes()) {
                while (true) {
                    current_pattern_id = this->v_pattern_id_to_longest_suffix_pattern_id[current_pattern_id];
                    if (current_pattern_id == AhoCorasickAutomaton::NO_PATTERN_ID)
                        break;
                    matches.push_back(
                            PatternMatch<KeyType>(this->v_pattern_id_to_pattern_key[current_pattern_id], pos));
                }
            }
        }

        // return the next state identifier
        return next_state_id;
    }

    /**
     * Reduce the memory footprint of the internal data structures
     */
    void
    reduce_memory_footprint() {
        if (!this->b_is_compiled) {
            throw std::runtime_error("This method cannot be called before the Automaton compilation");
        }

        // I shrink and rehash the main data structures to reduce the amount of memory
        this->v_state_id_to_node.shrink_to_fit();
        this->v_goto_id_to_goto.shrink_to_fit();
        this->v_pattern_id_to_pattern_key.shrink_to_fit();
        this->v_pattern_id_to_longest_suffix_pattern_id.shrink_to_fit();
        // for the hash tables we ensures a load factor of 0.5
        const size_t size_multiplier = 2;
        this->h_pattern_key_to_pattern_id.rehash(this->h_pattern_key_to_pattern_id.size() * size_multiplier);
        for (size_t i = 0, i_max = this->v_goto_id_to_goto.size(); i < i_max; ++i) {
            this->v_goto_id_to_goto[i].rehash(this->v_goto_id_to_goto[i].size() * size_multiplier);
        }
    }

    /**
     * Reserve enough space to contain the given number of patterns
     * @param num_patterns An estimate of the number of patterns that will be added
     */
    void
    reserve(
            size_t num_patterns
    ) {
        if (this->b_is_compiled) {
            throw std::runtime_error("This method cannot be called after the Automaton has been compiled");
        }
        if (num_patterns < 1) {
            num_patterns = 1;
        }
        this->v_state_id_to_node.reserve(num_patterns);
        this->v_goto_id_to_goto.reserve(num_patterns);
        this->v_pattern_id_to_pattern_key.reserve(num_patterns);
        this->h_pattern_key_to_pattern_id.reserve(num_patterns * 2);
    }


private:
    /**
     * Add a new pattern into the trie.
     * @param key A value to associate to this pattern, that will be retrieved during the parsing
     * @param pattern_begin A pointer to the begin of the pattern
     * @param pattern_end A pointer to the end of the pattern
     */
    void
    _add_pattern(
            const KeyType &key,
            SequenceType const *pattern_begin,
            SequenceType const *pattern_end
    ) {
        if (this->h_pattern_key_to_pattern_id.count(key) > 0) {
            throw std::invalid_argument("The given key has been already inserted");
        }

        AhoCorasickNode *curr_node = &(this->v_state_id_to_node[0]);

        // variables used in the next loop
        type_state_id next_state_id;
        GotoTableType *goto_table;
        GotoTableIteratorType find_result;
        bool perform_find;

        // perform a search
        for (SequenceType const *pattern_element = pattern_begin; pattern_element != pattern_end; ++pattern_element) {
            // check if the node has a goto table (otherwise create it)
            if (curr_node->l_goto_id == AhoCorasickAutomaton::NO_GOTO_ID) {
                const size_t next_goto = this->v_goto_id_to_goto.size();
                if (next_goto == AhoCorasickAutomaton::NO_GOTO_ID) {
                    throw std::runtime_error("Too many branches have been inserted in the trie");
                }
                this->v_goto_id_to_goto.push_back(GotoTableType());
                curr_node->l_goto_id = (type_goto_id) next_goto;
                perform_find = false;
            } else {
                perform_find = true;
            }

            // the goto table now exists
            goto_table = &(this->v_goto_id_to_goto[curr_node->l_goto_id]);

            // if the edge already exists I can follow it, otherwise I have to create it and the destination node
            if (perform_find && (find_result = goto_table->find(*pattern_element)) != goto_table->end()) {
                next_state_id = find_result->second;
            } else {
                next_state_id = (type_state_id) this->v_state_id_to_node.size();
                if (next_state_id == (type_state_id) -1) {
                    throw std::runtime_error("Too many nodes have been inserted in the automaton");
                }
                this->v_state_id_to_node.push_back(
                        AhoCorasickNode(AhoCorasickAutomaton::NO_GOTO_ID, AhoCorasickAutomaton::NO_PATTERN_ID));
                goto_table->operator[](*pattern_element) = next_state_id;
            }

            // go on to the next element of the pattern and to the next node in the trie
            curr_node = &(this->v_state_id_to_node[next_state_id]);
        }

        // check if this pattern is already in
        if (curr_node->l_pattern_id != AhoCorasickAutomaton::NO_PATTERN_ID) {
            throw std::invalid_argument("The given pattern was already inside the automaton");
        }

        // update the internal data structures _first, and then the destination node;
        size_t next_pattern_id = this->v_pattern_id_to_pattern_key.size();
        if (next_pattern_id == AhoCorasickAutomaton::NO_PATTERN_ID) {
            throw std::runtime_error("Too many patterns have been inserted");
        }

        type_pattern_id pattern_id = this->v_pattern_id_to_pattern_key.size();
        this->v_pattern_id_to_pattern_key.push_back(key);
        this->h_pattern_key_to_pattern_id[key] = pattern_id;
        curr_node->l_pattern_id = pattern_id;
    }

    void
    _compile() {
        this->v_pattern_id_to_longest_suffix_pattern_id.resize(this->v_pattern_id_to_pattern_key.size(),
                                                               AhoCorasickAutomaton::NO_PATTERN_ID);

        // the following queue is used to perform a bfs and update all the goto tables, the l_pattern_id and
        // the v_pattern_id_to_longest_suffix_pattern_id
        std::queue<BFSQueueEntry> bfs_queue;

        // temporary variables used in the next loops
        AhoCorasickNode
                *root_node,
                *curr_node,
                *fail_node;
        GotoTableType
                *root_goto_table,
                *curr_goto_table,
                *fail_goto_table;
        GotoTableIteratorType
                root_goto_it,
                root_goto_it_end,
                fail_goto_it,
                fail_goto_it_end,
                curr_goto_it,
                curr_goto_it_end;

        root_node = &(this->v_state_id_to_node[0]);
        root_goto_table = &(this->v_goto_id_to_goto[root_node->l_goto_id]);
        root_goto_it_end = root_goto_table->end();


        // 1) put the _first level of the trie into the queue
        if (root_node->l_goto_id != AhoCorasickAutomaton::NO_GOTO_ID) {
            // iterate over the nodes of the _first level
            for (root_goto_it = root_goto_table->begin(); root_goto_it != root_goto_it_end; ++root_goto_it) {
                bfs_queue.push(BFSQueueEntry(0, root_goto_it->second));
            }
        }

        // 2) loop until there are items in the queue
        while (!bfs_queue.empty()) {
            BFSQueueEntry queue_entry = bfs_queue.front();
            bfs_queue.pop();

            fail_node = &this->v_state_id_to_node[queue_entry.fail_state_id];
            curr_node = &this->v_state_id_to_node[queue_entry.curr_state_id];
            fail_goto_table = (fail_node->l_goto_id == AhoCorasickAutomaton::NO_GOTO_ID) ? nullptr
                                                                                         : &(this->v_goto_id_to_goto[fail_node->l_goto_id]);
            curr_goto_table = (curr_node->l_goto_id == AhoCorasickAutomaton::NO_GOTO_ID) ? nullptr
                                                                                         : &(this->v_goto_id_to_goto[curr_node->l_goto_id]);

            if (fail_goto_table) {
                fail_goto_it_end = fail_goto_table->end();
            }

            // 2.1) update the pattern of the current node (together with its suffix link)
            if (fail_node->l_pattern_id != AhoCorasickAutomaton::NO_PATTERN_ID) {
                if (curr_node->l_pattern_id != AhoCorasickAutomaton::NO_PATTERN_ID) {
                    if (this->v_pattern_id_to_longest_suffix_pattern_id[curr_node->l_pattern_id] !=
                        AhoCorasickAutomaton::NO_PATTERN_ID) {
                        std::runtime_error("AssertionError: the suffix link already exists");
                    }
                    this->v_pattern_id_to_longest_suffix_pattern_id[curr_node->l_pattern_id] = fail_node->l_pattern_id;
                } else {
                    curr_node->l_pattern_id = fail_node->l_pattern_id;
                }
            }

            // 2.2) put the children of the current node in the bfs queue
            if (curr_goto_table != nullptr) {
                for (curr_goto_it = curr_goto_table->begin(), curr_goto_it_end = curr_goto_table->end();
                     curr_goto_it != curr_goto_it_end; ++curr_goto_it) {
                    // what we are doing here is the same of doing the following:
                    // bfs_queue.push(BFSQueueEntry(_get_next_state_id(queue_entry.fail_state_id, curr_goto_it->_first), curr_goto_it->_second));

                    // 2.2.1) check if the same branch exists in the fail node
                    if (fail_goto_table) {
                        fail_goto_it = fail_goto_table->find(curr_goto_it->first);
                        if (fail_goto_it != fail_goto_it_end) {
                            bfs_queue.push(BFSQueueEntry(fail_goto_it->second, curr_goto_it->second));
                            continue;
                        }
                    }

                    // 2.2.2) check if the same branch exists in the root node (if this is different from the fail one)
                    if (root_goto_table != fail_goto_table) {
                        root_goto_it = root_goto_table->find(curr_goto_it->first);
                        if (root_goto_it != root_goto_it_end) {
                            bfs_queue.push(BFSQueueEntry(root_goto_it->second, curr_goto_it->second));
                            continue;
                        }
                    }

                    // 2.2.3) put in the queue the pair < root , child >
                    bfs_queue.push(BFSQueueEntry(0, curr_goto_it->second));
                }
            }

            // 2.3) Extend the goto table of the current node with the children of the fail node (if it isn't the root one) that are not children of this node.
            // a) we don't reuse the goto of the root for memory saving
            // b) if the fail node hasn't children (no goto table) we have nothing to extend
            if (queue_entry.fail_state_id != 0 && fail_goto_table) {
                // check if the goto table must be created
                if (curr_goto_table) {
                    // 2.3.1) put in the current table all the entries of the fail goto table that don't appear here
                    for (fail_goto_it = fail_goto_table->begin(); fail_goto_it != fail_goto_it_end; ++fail_goto_it) {
                        if (curr_goto_table->count(fail_goto_it->first) == 0) {
                            curr_goto_table->operator[](fail_goto_it->first) = fail_goto_it->second;
                        }
                    }
                } else {
                    // 2.3.2) the goto table doesn't exists so we can reuse the table of the fail node, otherwise we should copy all the entries
                    curr_node->l_goto_id = fail_node->l_goto_id;
                }
            }
        } // loop end
    }

    type_state_id
    _get_next_state_id(
            type_state_id current_state_id,
            const SequenceType &sequence_element
    ) const {
        // remember: goto 0 is the default behaviour
        const AhoCorasickNode &curr_node = this->v_state_id_to_node[current_state_id];

        // no goto table, restart from the root
        if (curr_node.l_goto_id == AhoCorasickAutomaton::NO_GOTO_ID) {
            return (current_state_id == 0) ? 0 : this->_get_next_state_id(0, sequence_element);
        }

        const GotoTableType &goto_table = this->v_goto_id_to_goto[curr_node.l_goto_id];
        GotoTableIteratorType find_result = goto_table.find(sequence_element);
        // no goto entry, restart from the root
        if (find_result == goto_table.end()) {
            return (current_state_id == 0) ? 0 : this->_get_next_state_id(0, sequence_element);
        }

        // the edge exists
        return find_result->second;
    }
};


#endif //AHOCORASICKAUTOAMATA_HPP
